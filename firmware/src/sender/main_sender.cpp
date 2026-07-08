// ============================================================
// Datei:   main_sender.cpp
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================
// Sendemodul im Briefkasten (Heltec Wireless Stick Lite V3, ESP32-S3).
// Ablauf pro Zyklus: Aufwachen, Akkuspannung und Distanz messen,
// bei Ereignis verschluesselte LoRa-Nachricht senden, Zustaende im
// RTC-RAM sichern und fuer eine Stunde in den Deep Sleep wechseln.
//
// Handshake: Ereignismeldungen (STATUS ...) tragen eine zufaellige
// Nonce und werden vom Gateway mit "ACK #NONCE" quittiert. Bleibt die
// Quittung aus, wird bis zu ACK_RETRIES mal wiederholt. Erst nach
// erfolgreicher Quittung wird das zugehoerige Flag im RTC-RAM gesetzt,
// andernfalls wiederholt der naechste Weckzyklus die Meldung. SYNC-
// Pakete bleiben bewusst unquittiert, da sie stuendlich erneuert werden.

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include <VL53L0X.h>
#include "esp_sleep.h"
#include "esp_random.h"

#include "../common/nvs_config.h"
#include "../common/aes_crypto.h"

// ------------------------------------------------------------
// Pinbelegung (identisch zur MicroPython-Version)
// ------------------------------------------------------------
static const int PIN_VEXT      = 36;  // Spannungsversorgung Peripherie, aktiv LOW
static const int PIN_ADC_CTRL  = 37;  // Spannungsteiler-Steuerung, aktiv LOW
static const int PIN_ADC_BATT  = 1;   // Akkuspannung ueber Spannungsteiler

static const int PIN_I2C_SDA   = 41;
static const int PIN_I2C_SCL   = 42;

static const int PIN_LORA_SCK  = 9;
static const int PIN_LORA_MOSI = 10;
static const int PIN_LORA_MISO = 11;
static const int PIN_LORA_CS   = 8;
static const int PIN_LORA_DIO1 = 14;
static const int PIN_LORA_RST  = 12;
static const int PIN_LORA_BUSY = 13;

// ------------------------------------------------------------
// Schwellwerte und Zeiten (identisch zur MicroPython-Version)
// ------------------------------------------------------------
static const uint16_t DIST_THRESH_FULL  = 58;   // mm, Brief erkannt
static const uint16_t DIST_THRESH_EMPTY = 60;   // mm, Kasten geleert
static const float    BATT_THRESH_LOW   = 3.45; // V, Warnung ausloesen
static const float    BATT_THRESH_HIGH  = 3.60; // V, Warnung zuruecksetzen
static const uint64_t SLEEP_US          = 3600ULL * 1000000ULL; // 1 Stunde
static const uint32_t RX_WINDOW_MS      = 2000; // Empfangsfenster nach SYNC

// Handshake-Parameter fuer Ereignismeldungen
static const uint32_t ACK_WAIT_MS = 2000;  // Wartezeit auf Quittung je Versuch
static const int      ACK_RETRIES = 3;     // Sendeversuche je Ereignis

// Zustaende ueberleben den Deep Sleep im RTC-RAM.
// Ersetzt rtc.memory() aus MicroPython, ein Power-Cycle setzt beide
// Flags automatisch auf null (ersetzt resett_flags.py).
RTC_DATA_ATTR uint8_t mail_state = 0;
RTC_DATA_ATTR uint8_t batt_state = 0;

// LoRa-Funkmodul an eigenem SPI-Bus
SPIClass  spi_lora(HSPI);
SX1262    radio = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST,
                             PIN_LORA_BUSY, spi_lora);

VL53L0X   tof_sensor;
AesCrypto crypto;

// RX-Flag fuer Empfangsfenster (Quittungen und Kommandos)
volatile bool rx_flag = false;

void IRAM_ATTR onLoraReceive() {
    rx_flag = true;
}

// ------------------------------------------------------------
// Akkuspannung messen: Spannungsteiler aktivieren, wandeln,
// wieder deaktivieren um Ruhestrom zu vermeiden
// ------------------------------------------------------------
static float readBatteryVoltage() {
    digitalWrite(PIN_ADC_CTRL, LOW);
    delay(50);
    int raw = analogRead(PIN_ADC_BATT);
    digitalWrite(PIN_ADC_CTRL, HIGH);
    // 12 Bit Aufloesung, 11 dB Daempfung, Hardware-Teilerfaktor 4.9
    return (raw / 4095.0f) * 3.3f * 4.9f;
}

// ------------------------------------------------------------
// Verschluesselte Statusnachricht senden
// ------------------------------------------------------------
static void sendEncrypted(const char* text) {
    uint8_t packet[80];
    size_t len = crypto.encrypt(String(text), packet, sizeof(packet));
    if (len == 0) return;

    int state = radio.transmit(packet, len);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("Uebertragung erfolgreich");
    } else {
        Serial.print("Fehler: LoRa Uebertragung, Code ");
        Serial.println(state);
    }
}

// ------------------------------------------------------------
// Empfangsfenster: wartet bis zu timeout_ms auf das erste gueltig
// entschluesselbare Paket. Rueckgabe: true wenn eines empfangen wurde.
// ------------------------------------------------------------
static bool receiveDecrypted(uint32_t timeout_ms, String& decoded) {
    decoded = "";
    rx_flag = false;
    radio.setDio1Action(onLoraReceive);
    if (radio.startReceive() != RADIOLIB_ERR_NONE) {
        radio.clearDio1Action();
        return false;
    }

    uint32_t start = millis();
    bool got_packet = false;
    while (millis() - start < timeout_ms) {
        if (rx_flag) {
            rx_flag = false;
            uint8_t payload[80];
            size_t len = radio.getPacketLength();
            if (len > sizeof(payload)) len = sizeof(payload);
            if (radio.readData(payload, len) == RADIOLIB_ERR_NONE && len > 0) {
                decoded = crypto.decrypt(payload, len);
                if (decoded.length() > 0) {
                    got_packet = true;
                    break;
                }
            }
            // Unlesbares Paket verwerfen und weiter lauschen
            radio.startReceive();
        }
        delay(5);
    }

    radio.clearDio1Action();
    radio.standby();
    return got_packet;
}

// ------------------------------------------------------------
// Ereignismeldung mit Handshake senden: Nachricht traegt eine
// zufaellige Nonce, das Gateway antwortet mit "ACK #NONCE".
// Rueckgabe: true wenn das Gateway den Empfang quittiert hat.
// ------------------------------------------------------------
static bool sendWithAck(const char* event_text) {
    uint32_t nonce = esp_random();
    if (nonce == 0) nonce = 1;  // 0 ist fuer Altformat reserviert

    char msg[48];
    snprintf(msg, sizeof(msg), "%s #%08lX", event_text, (unsigned long)nonce);
    char expected[16];
    snprintf(expected, sizeof(expected), "ACK #%08lX", (unsigned long)nonce);

    for (int attempt = 1; attempt <= ACK_RETRIES; attempt++) {
        Serial.print("Sende Ereignis (Versuch ");
        Serial.print(attempt);
        Serial.print("): ");
        Serial.println(msg);
        sendEncrypted(msg);

        String reply;
        if (receiveDecrypted(ACK_WAIT_MS, reply) && reply == String(expected)) {
            Serial.println("Quittung vom Gateway empfangen");
            return true;
        }
        Serial.println("Keine Quittung erhalten");
    }
    Serial.println("Fehler: Ereignis blieb unbestaetigt, Wiederholung im naechsten Zyklus");
    return false;
}

// ------------------------------------------------------------
// SYNC-Paket: macht dem Gateway die Flags, die Akkuspannung und
// die Distanz bekannt (loest dort keine E-Mail aus)
// ------------------------------------------------------------
static void sendSync(float voltage, uint16_t distance) {
    char msg[48];
    snprintf(msg, sizeof(msg), "SYNC %u,%u,%.2f,%u",
             (unsigned)mail_state, (unsigned)batt_state, voltage, (unsigned)distance);
    Serial.print("Sende Sync: ");
    Serial.println(msg);
    sendEncrypted(msg);
}

// ------------------------------------------------------------
// Empfangsfenster nach dem SYNC: das Gateway kann jetzt ein
// vorgemerktes Kommando zustellen (Class-A-artiger Downlink).
// Rueckgabe: true wenn ein Flag-Reset ausgefuehrt wurde.
// ------------------------------------------------------------
static bool listenForCommand() {
    String decoded;
    if (!receiveDecrypted(RX_WINDOW_MS, decoded)) {
        return false;
    }

    if (decoded == "CMD RESET FLAGS") {
        Serial.println("Kommando empfangen: Flags werden zurueckgesetzt");
        mail_state = 0;
        batt_state = 0;
        return true;
    }
    Serial.println("Unbekanntes Kommando verworfen");
    return false;
}

// ------------------------------------------------------------
// Abschaltsequenz und Deep Sleep
// ------------------------------------------------------------
static void enterDeepSleep() {
    Serial.println("Zyklus abgeschlossen, bereite Deep Sleep vor");

    // I2C-Leitungen hochohmig schalten, damit der stromlose Sensor
    // nicht ueber die Pull-ups phantomgespeist wird
    Wire1.end();
    pinMode(PIN_I2C_SDA, INPUT);
    pinMode(PIN_I2C_SCL, INPUT);

    // Funkmodul explizit in den Schlafmodus versetzen
    radio.sleep();

    // Peripheriespannung abschalten
    digitalWrite(PIN_VEXT, HIGH);

    Serial.println("Wechsle in Deep Sleep");
    esp_sleep_enable_timer_wakeup(SLEEP_US);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    delay(100);

    // Peripheriespannung aktivieren
    Serial.println("Aktiviere Vext Spannungsversorgung");
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);
    delay(800);

    Serial.println("Initialisiere Subsysteme");

    // ADC vorbereiten
    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, HIGH);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_ADC_BATT, ADC_11db);

    // AES-Schluessel aus dem NVS laden
    NvsConfig cfg;
    if (!cfg.begin()) {
        Serial.println("Fehler: Konfiguration fehlt, Abbruch in den Sleep");
        enterDeepSleep();
    }
    uint8_t key[AesCrypto::KEY_LEN];
    if (cfg.readBlob("aes_main", key, sizeof(key)) != AesCrypto::KEY_LEN) {
        Serial.println("Fehler: AES Schluessel ungueltig, Abbruch in den Sleep");
        enterDeepSleep();
    }
    crypto.setKey(key);

    // I2C-Bus mit internen Pull-ups und ToF-Sensor initialisieren
    Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    tof_sensor.setBus(&Wire1);
    tof_sensor.setTimeout(1000);
    bool tof_ok = tof_sensor.init();
    if (!tof_ok) {
        Serial.println("Fehler: VL53L0X nicht erreichbar");
    }

    // LoRa-Funkmodul fuer das 868 MHz Band konfigurieren
    spi_lora.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
    int state = radio.begin(868.0, 125.0, 9, 8, 0x12, -5, 8, 1.7, false);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("Fehler: LoRa Initialisierung, Code ");
        Serial.println(state);
        enterDeepSleep();
    }
    radio.setCurrentLimit(60.0);
    radio.setCRC(true);

    // 1. Akkuzustand mit Hysterese auswerten.
    // Das Flag wird erst nach quittierter Zustellung gesetzt, ein
    // unbestaetigtes Ereignis wird im naechsten Zyklus wiederholt.
    float voltage = readBatteryVoltage();
    Serial.print("Gemessene Spannung ");
    Serial.print(voltage, 2);
    Serial.println(" V");

    if (batt_state == 0 && voltage < BATT_THRESH_LOW) {
        Serial.println("Alarm: Spannung kritisch niedrig, starte Uebertragung");
        if (sendWithAck("STATUS BATTERY LOW")) {
            batt_state = 1;
        }
    } else if (batt_state == 1 && voltage > BATT_THRESH_HIGH) {
        Serial.println("Info: Spannung erholt, setze Zustand zurueck");
        batt_state = 0;
    }

    // 2. Briefkastenzustand mit Hysterese auswerten (gleiche Logik)
    uint16_t distance = 0;
    if (tof_ok) {
        distance = tof_sensor.readRangeSingleMillimeters();
        if (tof_sensor.timeoutOccurred()) {
            Serial.println("Fehler: ToF Timeout, Messung wird uebersprungen");
            distance = 0;
        } else {
            Serial.print("Gemessene Distanz ");
            Serial.print(distance);
            Serial.println(" mm");

            if (mail_state == 0 && distance < DIST_THRESH_FULL) {
                Serial.println("Alarm: Distanzschwelle unterschritten, starte Uebertragung");
                if (sendWithAck("STATUS NEW MAIL")) {
                    mail_state = 1;
                }
            } else if (mail_state == 1 && distance > DIST_THRESH_EMPTY) {
                Serial.println("Info: Distanz wiederhergestellt, setze Zustand zurueck");
                mail_state = 0;
            }
        }
    }

    // 3. SYNC-Paket mit aktuellem Zustand an das Gateway senden.
    // Es dient dem Portal (Flags, Akkuspannung, Distanz) und oeffnet
    // zugleich das Zeitfenster fuer ferngesteuerte Kommandos.
    // Bewusst ohne Handshake: SYNC wird stuendlich erneuert.
    sendSync(voltage, distance);

    // 4. Kurzes Empfangsfenster fuer ein vorgemerktes Reset-Kommando
    if (listenForCommand()) {
        // Reset bestaetigen, damit das Portal den neuen Zustand anzeigt
        sendSync(voltage, distance);
    }

    // 5. Abschalten und schlafen
    enterDeepSleep();
}

void loop() {
    // Wird nur erreicht, wenn der Deep Sleep nicht betreten wurde.
    // Sicherheitshalber erneut versuchen statt still zu haengen.
    Serial.println("Warnung: Deep Sleep nicht betreten, erneuter Versuch");
    delay(1000);
    enterDeepSleep();
}
