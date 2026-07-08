# Briefkastensensor

Ein batteriebetriebener Sensor meldet Posteinwürfe per LoRa-Funk an ein
Gateway im Haus, das daraufhin eine E-Mail versendet und den Systemzustand
in einem lokalen Web-Portal bereitstellt. Das Projekt umfasst die komplette
Firmware (C++ / PlatformIO), die CAD-Modelle des Sensorhalters und den
fertigen G-Code für den 3D-Druck.

---

## Funktionsweise

Der Sender sitzt im Briefkasten und misst ein Mal pro Stunde per
Time-of-Flight-Sensor die Distanz zur Rückwand. Fällt die Distanz unter
65 mm, liegt ein Brief im Kasten: der Sender schickt eine
AES-verschlüsselte Meldung an das Gateway, das eine E-Mail an die
hinterlegten Empfänger versendet. Steigt die Distanz wieder über 75 mm
(Kasten geleert), setzt sich der Zustand automatisch zurück (Hysterese,
verhindert Mehrfachmeldungen). Analog überwacht der Sender seine
Akkuspannung (Warnung unter 3.45 V, Rücksetzen über 3.60 V).

Zusätzlich sendet der Sender bei jedem Aufwachen ein SYNC-Paket mit
Flags, Akkuspannung und Distanz. Das Gateway zeigt diese Werte zusammen
mit dem Raumklima in einem Web-Portal unter http://briefkastensensor.local/
an. Über das Portal lassen sich WLAN-, SMTP- und Empfänger-Einstellungen
ändern sowie die Sender-Flags ferngesteuert zurücksetzen: Nach jedem
SYNC lauscht der Sender zwei Sekunden auf Kommandos, das Gateway stellt
vorgemerkte Befehle in genau diesem Fenster zu (Class-A-Prinzip).

## Hardware

| Komponente | Funktion |
|---|---|
| Heltec Wireless Stick Lite V3 (ESP32-S3, SX1262, 868 MHz) | Sender im Briefkasten |
| Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262, OLED, 868 MHz) | Gateway im Haus |
| M5Stack Mini ToF Unit, 90 Grad (VL53L0X) | Distanzmessung zur Briefkasten-Rückwand |
| Waveshare BME280 | Raumklima am Gateway (Temperatur, Luftfeuchte) |
| Heltec 800 mAh LiPo 802540, 3.7 V | Stromversorgung des Senders |

### Pinbelegung

Beide Boards nutzen den gleichen SX1262-Anschluss: SPI an GPIO 8 (CS),
9 (SCK), 10 (MOSI), 11 (MISO), dazu DIO1 an 14, RST an 12, BUSY an 13.
Die Peripheriespannung (Vext, aktiv LOW) liegt auf GPIO 36.

Sender: VL53L0X per I2C an GPIO 41 (SDA) / 42 (SCL). Akkumessung über
GPIO 1 (ADC) mit Spannungsteiler-Freigabe an GPIO 37 und Teilerfaktor 4.9.

Gateway: OLED per I2C an GPIO 17 (SDA) / 18 (SCL), Reset an 21. BME280
(Adresse 0x77) am zweiten I2C-Bus an GPIO 41 / 42.

### Funkparameter

868 MHz, Bandbreite 125 kHz, Spreading Factor 9, Coding Rate 4/8,
Syncword 0x12 (privat), Sendeleistung -5 dBm, CRC aktiv. Alle Pakete sind
AES-128-CBC-verschlüsselt (Paketformat: 16 Byte Zufalls-IV plus
Ciphertext mit PKCS7-Padding).

## Mechanik: Sensorhalter (3D-Druck)

Der Halter ist zweiteilig aufgebaut:

1. **Aufnahme** (Briefkastensensorikaufnahme): wird fest in den
   Briefkasten eingeklebt und bleibt dauerhaft montiert.
2. **Deckel** (Briefkastendeckel): herausnehmbarer Einsatz, der die
   komplette Elektronik trägt: ToF-Sensor, Wireless Stick Lite V3 und
   den Akku. Zum Laden oder Flashen wird nur der Deckel entnommen, die
   verklebte Aufnahme bleibt im Kasten.

Der ToF-Sensor sitzt dank der 90-Grad-Bauform der M5Stack-Einheit flach
im Deckel und misst parallel zum Briefkastenboden auf die Rückwand.

Dateien im Ordner `CAD_Model/`:

| Datei | Inhalt |
|---|---|
| Briefkastensensorikaufnahme.CATPart | Aufnahme, CATIA-Quellmodell |
| Briefkastendeckel.CATPart | Deckel, CATIA-Quellmodell |
| Briefkastensensor.stl / Briefkastendeckel.stl | Exportierte Druckdaten |
| Zusammenbau.CATProduct / Product1.CATProduct | CATIA-Baugruppen beider Teile |

Im Ordner `3D_Print_GCode/` liegt fertig gesliceter G-Code für beide
Teile (Profil AI3MSPRO). Für andere Drucker die STL-Dateien mit dem
eigenen Slicer neu aufbereiten.

## Ordnerstruktur

```
.
|-- firmware/               C++ Firmware (PlatformIO-Projekt)
|   |-- src/sender/     Firmware Sendemodul
|   |-- src/gateway/    Firmware Gateway (SMTP, Web-Portal)
|   |-- src/common/     Gemeinsame Module (AES, NVS)
|   |-- src/provision/  Einmalige Zugangsdaten-Provisionierung
|   |-- README.md       Technische Details der Firmware
|   `-- ANLEITUNG.md    Schritt-für-Schritt: Setup und Flashen
|-- hardware/
|   |-- CAD_Model/          CATIA-Modelle und STL-Exporte des Halters
|   |-- 3D_Print_GCode/     Gesliceter G-Code für beide Druckteile
|-- docs/
```

## Software im Überblick

Die Firmware ist als PlatformIO-Projekt mit drei Umgebungen organisiert:

- `sender`: Deep-Sleep-Zyklus (1 h), Messung, Ereignis- und SYNC-Versand,
  2-Sekunden-Empfangsfenster für Gateway-Kommandos. Zustandsflags liegen
  im RTC-RAM; ein Power-Cycle setzt sie zurück.
- `gateway`: LoRa-Empfang per Interrupt, E-Mail-Versand über Gmail
  (Port 465, TLS mit Zertifikatsprüfung gegen GTS Root R1, NTP-Zeitsync),
  OLED-Anzeige, Web-Portal mit mDNS, Task-Watchdog.
- `provision`: liest die lokale `.env` beim Kompilieren ein und schreibt
  die Zugangsdaten einmalig in den NVS des jeweiligen Chips.

Verwendete Bibliotheken: RadioLib (SX1262), Adafruit SSD1306/GFX/BME280,
Pololu VL53L0X, mbedtls (AES, im ESP32-Framework enthalten).

## Sicherheit

- Alle Funkpakete sind AES-128-CBC-verschlüsselt; der Schlüssel wird
  ausschließlich per `.env`-Provisionierung gesetzt und ist bewusst
  nicht über das Web-Portal änderbar.
- Der SMTP-Versand prüft das Gmail-Zertifikat gegen das eingebettete
  Google-Root-Zertifikat (gültig bis 2036).
- Das Web-Portal ist per HTTP Basic Auth geschützt (PORTAL_PASS in der
  `.env`); gespeicherte Passwörter werden nie angezeigt.
- Bekannte Einschränkung: das Funkprotokoll enthält keinen
  Replay-Schutz. Ein mitgeschnittenes Paket könnte erneut gesendet
  werden und löst schlimmstenfalls eine überflüssige E-Mail oder ein
  Flag-Reset aus.

## Inbetriebnahme

Die vollständige Anleitung (PlatformIO-Setup, Flash-Erase, Provisionierung,
Flashen, Funktionstests, Fehlerbehebung) steht in `docs/ANLEITUNG.md`.
Kurzfassung:

```
cd firmware
cp .env.example .env          # Werte eintragen
pio run -e provision -t erase # pro Board: Flash löschen
pio run -e provision -t upload
pio run -e gateway -t upload  # bzw. -e sender
```

## Betrieb

- Der Sender meldet sich stündlich; "Letzter Kontakt" im Portal sollte
  60 Minuten nicht deutlich überschreiten.
- Akku laden: Deckel aus der Aufnahme nehmen, Akku bzw. Board per USB
  laden, Deckel wieder einsetzen und einmal RST drücken.
- Ein Flag-Reset über das Portal wird beim nächsten stündlichen
  Kontakt zugestellt.
