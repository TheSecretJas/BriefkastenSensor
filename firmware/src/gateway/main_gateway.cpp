// ============================================================
// Datei:   main_gateway.cpp
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================
// Gateway (Heltec WiFi LoRa 32 V3, ESP32-S3). Empfaengt verschluesselte
// LoRa-Statusmeldungen des Briefkastensensors, entschluesselt sie und
// versendet E-Mail-Benachrichtigungen. Das OLED zeigt Raumklima
// (BME280) und Systemstatus an. Ein Watchdog ueberwacht die Hauptschleife.
//
// Handshake: Ereignismeldungen des Senders tragen eine Nonce im Format
// "STATUS ... #NONCE" und werden unmittelbar mit "ACK #NONCE" quittiert.
// Trifft dieselbe Nonce erneut ein (Quittung ging verloren), wird nur
// die Quittung wiederholt, das Ereignis aber nicht doppelt verarbeitet.
// Meldungen ohne Nonce (Altformat) werden weiterhin akzeptiert.
//
// Zusaetzlich stellt das Gateway ein LAN-Web-Portal unter
// http://briefkastensensor.local/ bereit. Es zeigt den Zustand des
// Sendemoduls (SYNC-Pakete: Flags, Akkuspannung, Distanz), erlaubt
// Konfigurationsaenderungen und kann ein Flag-Reset-Kommando an den
// Sender zustellen. SYNC-Pakete loesen keine E-Mail aus.

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <RadioLib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include "esp_task_wdt.h"
#include <time.h>

#include "../common/nvs_config.h"
#include "../common/aes_crypto.h"
#include "smtp_client.h"
#include "web_portal.h"

// ------------------------------------------------------------
// Pinbelegung (identisch zur MicroPython-Version)
// ------------------------------------------------------------
static const int PIN_VEXT      = 36;  // aktiv LOW
static const int PIN_OLED_RST  = 21;
static const int PIN_OLED_SDA  = 17;
static const int PIN_OLED_SCL  = 18;

static const int PIN_EXT_SDA   = 41;  // BME280
static const int PIN_EXT_SCL   = 42;

static const int PIN_LORA_SCK  = 9;
static const int PIN_LORA_MOSI = 10;
static const int PIN_LORA_MISO = 11;
static const int PIN_LORA_CS   = 8;
static const int PIN_LORA_DIO1 = 14;
static const int PIN_LORA_RST  = 12;
static const int PIN_LORA_BUSY = 13;

static const uint32_t WDT_TIMEOUT_S = 15;
static const uint32_t ALERT_HOLD_S  = 60;   // Anzeigedauer des Postalarms
static const uint32_t BURST_WAIT_MS = 800;  // Wartezeit auf Folgepakete

// ------------------------------------------------------------
// Globale Objekte und Zustaende
// ------------------------------------------------------------
SPIClass         spi_lora(HSPI);
SX1262           radio = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST,
                                    PIN_LORA_BUSY, spi_lora);
Adafruit_SSD1306 display(128, 64, &Wire, PIN_OLED_RST);
Adafruit_BME280  bme;
AesCrypto        crypto;
NvsConfig        cfg;
WebPortal        portal;

SmtpClient*         mailer = nullptr;
std::vector<String> recipients;

String   sys_status   = "RX Ready";
uint32_t status_until = 0;
bool     alert_active = false;
uint32_t alert_time   = 0;
bool     bme_ok       = false;

// Nonce des zuletzt verarbeiteten Ereignisses fuer die Duplikaterkennung
uint32_t last_evt_nonce = 0;

// Aufgeschobener E-Mail-Versand: wird erst nach dem Abarbeiten
// eines Paket-Bursts ausgefuehrt, damit Quittung und Reset-Kommando
// die Empfangsfenster des Senders nicht verpassen
String pending_subject;
String pending_body;

// RX-Flag wird in der Interruptroutine gesetzt
volatile bool msg_flag = false;

// ------------------------------------------------------------
// Interruptroutine, ausgeloest durch DIO1 bei empfangenem Paket
// ------------------------------------------------------------
void IRAM_ATTR onLoraReceive() {
    msg_flag = true;
}

// ------------------------------------------------------------
// OLED-Anzeige: Raumklima und Statuszeile
// ------------------------------------------------------------
static void updateScreen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    if (alert_active) {
        display.print("NEW MAIL ARRIVED");
    } else {
        display.print("ROOM CLIMATE");
    }

    if (bme_ok) {
        float temp_c = bme.readTemperature();
        float hum_p  = bme.readHumidity();
        display.setCursor(0, 20);
        display.print("Temp ");
        display.print(temp_c, 1);
        display.print(" C");
        display.setCursor(0, 35);
        display.print("Hum  ");
        display.print(hum_p, 1);
        display.print(" %");
    } else {
        display.setCursor(0, 20);
        display.print("BME280 Fehler");
    }

    display.setCursor(0, 55);
    display.print(sys_status);
    display.display();
}

// ------------------------------------------------------------
// Logausgabe auf Konsole und OLED-Statuszeile
// ------------------------------------------------------------
static void logUi(const String& console_text, const String& oled_text = "",
                  uint32_t duration_s = 2) {
    Serial.println(console_text);
    sys_status = oled_text.length() ? oled_text : console_text.substring(0, 16);
    status_until = millis() / 1000 + duration_s;
    updateScreen();
}

// ------------------------------------------------------------
// WLAN-Verbindung mit Wiederholversuchen aufbauen
// ------------------------------------------------------------
static void initWlan() {
    String ssid = cfg.readString("wifi_ssid");
    String pwd  = cfg.readString("wifi_pass");
    if (ssid.length() == 0) {
        Serial.println("Fehler: WLAN Zugangsdaten fehlen");
        return;
    }

    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.print("WLAN Verbindungsversuch ");
        Serial.println(attempt);
        WiFi.disconnect(true);
        delay(500);
        WiFi.mode(WIFI_STA);
        // Modem-Sleep deaktivieren, damit das Portal fluessig reagiert
        WiFi.setSleep(false);
        WiFi.begin(ssid.c_str(), pwd.c_str());

        int timeout = 40;  // 40 x 500 ms = 20 Sekunden
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(500);
            timeout--;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WLAN erfolgreich verbunden");
            return;
        }
        Serial.println("Verbindung fehlgeschlagen, neuer Versuch");
    }
    Serial.println("Fehler: Kritischer WLAN Ausfall");
}

// ------------------------------------------------------------
// Systemzeit per NTP synchronisieren. Zwingend erforderlich fuer
// die TLS-Zertifikatspruefung des SMTP-Clients: ohne gueltige
// Systemzeit schlaegt die Gueltigkeitspruefung des Zertifikats fehl.
// ------------------------------------------------------------
static void syncClock() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("Starte NTP Zeitsynchronisation");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // Bis zu 15 Sekunden auf eine plausible Zeit warten
    time_t now = 0;
    for (int i = 0; i < 30; i++) {
        now = time(nullptr);
        if (now > 1700000000) {  // Plausibilitaet: nach Nov 2023
            Serial.print("Systemzeit gesetzt: ");
            Serial.println(ctime(&now));
            return;
        }
        delay(500);
    }
    Serial.println("Warnung: NTP fehlgeschlagen, E-Mail-Versand wird scheitern");
}

// ------------------------------------------------------------
// Verbindungspruefung, bei Ausfall harter Neustart
// ------------------------------------------------------------
static void ensureNetwork() {
    if (WiFi.status() != WL_CONNECTED) {
        logUi("WLAN verloren, starte System neu", "WLAN Error");
        delay(2000);
        ESP.restart();
    }
}

// ------------------------------------------------------------
// Verschluesselten Downlink an den Sender uebertragen.
// Wird nur aufgerufen, wenn der Sender gerade ein Empfangsfenster
// geoeffnet hat (nach Ereignis- oder SYNC-Paket).
// ------------------------------------------------------------
static void transmitDownlink(const char* text, const char* oled_label) {
    uint8_t packet[80];
    size_t len = crypto.encrypt(String(text), packet, sizeof(packet));
    if (len == 0) return;

    // Dem Sender kurz Zeit geben, von TX auf RX umzuschalten
    delay(100);
    int state = radio.transmit(packet, len);
    radio.startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        logUi(String("Downlink gesendet: ") + text, oled_label);
    } else {
        Serial.print("Fehler: Downlink-Uebertragung, Code ");
        Serial.println(state);
    }
}

// ------------------------------------------------------------
// Quittung fuer ein Ereignis mit Nonce senden
// ------------------------------------------------------------
static void sendAck(uint32_t nonce) {
    char msg[16];
    snprintf(msg, sizeof(msg), "ACK #%08lX", (unsigned long)nonce);
    transmitDownlink(msg, "ACK Sent");
}

// ------------------------------------------------------------
// Ereigniszeile pruefen: akzeptiert "PREFIX" (Altformat) und
// "PREFIX #NONCE" (Handshake). Nonce 0 bedeutet Altformat.
// ------------------------------------------------------------
static bool matchEvent(const String& decoded, const char* prefix, uint32_t& nonce) {
    nonce = 0;
    if (!decoded.startsWith(prefix)) return false;
    int pos = decoded.indexOf('#', strlen(prefix));
    if (pos >= 0) {
        nonce = strtoul(decoded.c_str() + pos + 1, nullptr, 16);
    }
    return true;
}

// ------------------------------------------------------------
// Einzelnes empfangenes Paket entschluesseln und verarbeiten
// ------------------------------------------------------------
static void handlePacket() {
    uint8_t payload[80];
    size_t len = radio.getPacketLength();
    if (len > sizeof(payload)) len = sizeof(payload);
    int state = radio.readData(payload, len);

    // Empfang unmittelbar wieder aktivieren
    radio.startReceive();

    if (state != RADIOLIB_ERR_NONE || len == 0) {
        Serial.print("Fehler: LoRa Empfang, Code ");
        Serial.println(state);
        return;
    }

    String decoded = crypto.decrypt(payload, len);
    if (decoded.length() == 0) return;

    uint32_t now = millis() / 1000;
    uint32_t nonce = 0;

    if (decoded.startsWith("SYNC ")) {
        // Zustandsmeldung des Senders: Portal aktualisieren, keine E-Mail
        unsigned mail = 0, batt = 0, dist = 0;
        float volt = 0;
        if (sscanf(decoded.c_str() + 5, "%u,%u,%f,%u",
                   &mail, &batt, &volt, &dist) == 4) {
            portal.updateRemote((uint8_t)mail, (uint8_t)batt, volt, (uint16_t)dist);
            Serial.print("Sync empfangen: ");
            Serial.println(decoded);

            // Vorgemerktes Reset-Kommando im Empfangsfenster zustellen
            if (portal.resetPending()) {
                transmitDownlink("CMD RESET FLAGS", "CMD Sent");
                portal.clearResetPending();
            }
        } else {
            Serial.println("Fehler: Sync-Paket nicht lesbar");
        }

    } else if (matchEvent(decoded, "STATUS NEW MAIL", nonce)) {
        // Empfang sofort quittieren, der Sender wartet nur kurz
        if (nonce != 0) sendAck(nonce);
        if (nonce != 0 && nonce == last_evt_nonce) {
            Serial.println("Duplikat erkannt, nur Quittung wiederholt");
        } else {
            last_evt_nonce = nonce;
            alert_active = true;
            alert_time = now;
            logUi("Neuer Postalarm", "Valid Alert");
            pending_subject = "Posteinwurf";
            pending_body = "Die Sensorik meldet einen Einwurf, neue Post liegt zur Abholung bereit.";
        }

    } else if (matchEvent(decoded, "STATUS BATTERY LOW", nonce)) {
        if (nonce != 0) sendAck(nonce);
        if (nonce != 0 && nonce == last_evt_nonce) {
            Serial.println("Duplikat erkannt, nur Quittung wiederholt");
        } else {
            last_evt_nonce = nonce;
            logUi("Batteriewarnung", "Batt Alert");
            pending_subject = "Akku kritisch";
            pending_body = "Die Akkuspannung am Sendemodul ist unter die kritische Schwelle gefallen. Bitte aufladen!";
        }

    } else {
        Serial.println("Unbekannte Nachricht empfangen");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    // Konfiguration aus dem NVS laden
    if (!cfg.begin()) {
        Serial.println("Fehler: NVS Konfiguration fehlt, Neustart in 10 s");
        delay(10000);
        ESP.restart();
    }
    uint8_t key[AesCrypto::KEY_LEN];
    if (cfg.readBlob("aes_main", key, sizeof(key)) != AesCrypto::KEY_LEN) {
        Serial.println("Fehler: AES Schluessel ungueltig, Neustart in 10 s");
        delay(10000);
        ESP.restart();
    }
    crypto.setKey(key);

    String smtp_user = cfg.readString("smtp_user");
    String smtp_pass = cfg.readString("smtp_pass");
    mailer = new SmtpClient(smtp_user, smtp_pass);

    recipients = NvsConfig::splitList(cfg.readString("recipients"));
    if (recipients.empty()) {
        Serial.println("Warnung: Keine Empfaenger im NVS gefunden");
    }

    // WLAN frueh aufbauen (Phase 1 der MicroPython-Version)
    initWlan();

    // Systemzeit fuer die TLS-Zertifikatspruefung setzen
    syncClock();

    // Web-Portal und mDNS starten
    portal.begin(&cfg, cfg.readString("portal_pass"));
    portal.attachStatusText(&sys_status);
    portal.setClimateProvider([](float& temp, float& hum) -> bool {
        if (!bme_ok) return false;
        temp = bme.readTemperature();
        hum  = bme.readHumidity();
        return true;
    });

    // Peripheriespannung aktivieren
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);
    delay(100);

    // OLED-Reset und Initialisierung
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("Fehler: OLED nicht erreichbar");
    }

    // BME280 auf dem externen I2C-Bus
    Wire1.begin(PIN_EXT_SDA, PIN_EXT_SCL);
    bme_ok = bme.begin(0x77, &Wire1);
    if (!bme_ok) {
        Serial.println("Fehler: BME280 nicht erreichbar");
    }

    // LoRa-Hardware-Reset erzwingen, dann initialisieren
    Serial.println("Erzwinge LoRa Hardware-Reset");
    pinMode(PIN_LORA_RST, OUTPUT);
    digitalWrite(PIN_LORA_RST, LOW);
    delay(50);
    digitalWrite(PIN_LORA_RST, HIGH);
    delay(50);

    spi_lora.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
    int state = radio.begin(868.0, 125.0, 9, 8, 0x12, -5, 8, 1.7, false);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Fehler: LoRa Initialisierung, Code ");
        Serial.println(state);
        delay(5000);
        ESP.restart();
    }
    radio.setCurrentLimit(60.0);
    radio.setCRC(true);

    // Nicht blockierender Empfang mit Interrupt auf DIO1
    radio.setDio1Action(onLoraReceive);
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Fehler: LoRa RX Start, Code ");
        Serial.println(state);
    }

    updateScreen();

    // Watchdog fuer die Hauptschleife aktivieren
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    logUi("Gateway betriebsbereit", "RX Ready");
}

void loop() {
    static uint32_t last_update = 0;
    uint32_t now = millis() / 1000;

    esp_task_wdt_reset();
    portal.handle();

    // Statuszeile nach Ablauf der Anzeigedauer zuruecksetzen
    if (sys_status != "RX Ready" && now > status_until) {
        sys_status = "RX Ready";
        updateScreen();
    }

    // Anzeige alle 5 Sekunden auffrischen, Alarm nach 60 s loeschen
    if (now - last_update > 5) {
        if (alert_active && (now - alert_time > ALERT_HOLD_S)) {
            alert_active = false;
        }
        updateScreen();
        last_update = now;
    }

    // Empfangene Pakete verarbeiten. Der Sender schickt Ereignis- und
    // SYNC-Pakete als Burst, daher kurz auf Folgepakete warten, damit
    // Quittung und Reset-Kommando vor dem E-Mail-Versand zugestellt
    // werden koennen.
    if (msg_flag) {
        msg_flag = false;
        handlePacket();

        uint32_t burst_start = millis();
        while (millis() - burst_start < BURST_WAIT_MS) {
            esp_task_wdt_reset();
            if (msg_flag) {
                msg_flag = false;
                handlePacket();
                burst_start = millis();
            }
            delay(10);
        }

        // Aufgeschobene E-Mail jetzt versenden
        if (pending_subject.length() > 0) {
            ensureNetwork();
            mailer->dispatch(recipients, pending_subject, pending_body,
                [](const String& c, const String& o) { logUi(c, o); });
            pending_subject = "";
            pending_body = "";
            logUi("Sequenz abgeschlossen, zurueck in Bereitschaft", "RX Ready");
        }
    }

    delay(20);
}
