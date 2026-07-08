# Briefkastensensor (C++ / PlatformIO)

Portierung des MicroPython-Projekts auf C++ (Arduino-Framework, PlatformIO). Der Wechsel reduziert den RAM-Bedarf deutlich, da kein Interpreter, kein Heap-Garbage-Collector und keine Bytecode-Strukturen mehr benoetigt werden. Die Funkschnittstelle, das Verschluesselungsformat und die NVS-Schluessel sind zur MicroPython-Version vollstaendig kompatibel.

Hinweis: Dokumentation und Kommentare wurden mit Unterstuetzung von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.

## Architektur

Sender (Heltec Wireless Stick Lite V3, im Briefkasten):
stuendlicher Deep-Sleep-Zyklus, Messung der Akkuspannung (GPIO 1, Teilerfaktor 4.9, Hysterese 3.45 V / 3.60 V) und der Distanz zur Rueckwand per VL53L0X (I2C an GPIO 41/42, Hysterese 65 mm / 75 mm). Bei einem Ereignis wird eine AES-128-CBC-verschluesselte Nachricht per SX1262 gesendet (868 MHz, SF9, BW 125 kHz, CR 4/8, Syncword 0x12, -5 dBm). Die Zustandsflags liegen im RTC-RAM und ueberleben den Deep Sleep.

Gateway (Heltec WiFi LoRa 32 V3):
WLAN-Anbindung, nicht blockierender LoRa-Empfang per DIO1-Interrupt, Entschluesselung und Versand einer E-Mail ueber Gmail (Port 465, bis zu fuenf Versuche). Das OLED zeigt Raumklima (BME280 an GPIO 41/42, Adresse 0x77) und den Systemstatus. Ein Task-Watchdog (15 s) ueberwacht die Hauptschleife und wird waehrend des SMTP-Versands mitgefuettert.

## Projektstruktur

```
platformio.ini            Umgebungen: sender, gateway, provision
.env.example              Vorlage fuer Zugangsdaten
scripts/load_env.py       Liest .env fuer die Provisionierung ein
src/common/               Gemeinsame Module (NVS, AES)
src/sender/               Firmware Sendemodul
src/gateway/              Firmware Gateway inkl. SMTP-Client
src/provision/            Einmalige NVS-Provisionierung (ersetzt setup_NVS.py)
```

## Inbetriebnahme

1. `.env.example` als `.env` kopieren und Werte eintragen. Der AES-Schluessel muss exakt 16 Zeichen lang sein.

2. Flash der Chips vollstaendig loeschen. Das ist wichtig, weil die Partitionstabelle von MicroPython nicht mit der Arduino-Tabelle uebereinstimmt und alte NVS-Reste sonst zu undefiniertem Verhalten fuehren koennen:
   ```
   pio run -e provision -t erase
   ```

3. Provisionierung auf beide Chips flashen und die serielle Ausgabe pruefen:
   ```
   pio run -e provision -t upload
   pio device monitor
   ```
   Erwartete Ausgabe: "Zugangsdaten erfolgreich im NVS gespeichert".

4. Eigentliche Firmware flashen (die NVS-Daten bleiben dabei erhalten):
   ```
   pio run -e sender -t upload    # Chip im Briefkasten
   pio run -e gateway -t upload   # Gateway
   ```

## Web-Portal

Das Gateway stellt im LAN ein Web-Portal unter http://briefkastensensor.local/ bereit (alternativ ueber die IP-Adresse, siehe serielle Ausgabe). Es zeigt:

- Zustand des Sendemoduls: Brief-Flag, Akku-Warnflag, Akkuspannung, gemessene Distanz und Zeitpunkt des letzten Kontakts
- Raumklima des Gateways (BME280) und die aktuelle Statuszeile
- Button "Sender-Flags zuruecksetzen" fuer den ferngesteuerten Reset
- Konfigurationsformular fuer WLAN, SMTP und Empfaenger

Das initiale Setup laeuft ausschliesslich ueber die .env Provisionierung. Danach koennen die Werte im Portal geaendert werden; nach dem Speichern startet das Gateway neu. Sicherheitsregeln des Portals:

- Gespeicherte Passwoerter werden niemals angezeigt (Schreibfelder, leer = unveraendert)
- Der AES-Schluessel ist nicht ueber das Portal aenderbar, nur per .env
- Mit PORTAL_PASS in der .env wird das Portal per HTTP Basic Auth geschuetzt (Benutzer: admin). Ohne Passwort ist das Portal fuer jeden im LAN offen.

## Funkprotokoll und ferngesteuerter Flag-Reset

Der Sender schlaeft fast durchgehend, das Gateway kann ihm daher nichts direkt zustellen. Geloest wird das Class-A-artig:

1. Der Sender sendet bei jedem Aufwachen (stuendlich) ein verschluesseltes SYNC-Paket: `SYNC <mail_flag>,<batt_flag>,<spannung>,<distanz>`. Das Gateway aktualisiert damit das Portal, versendet aber keine E-Mail. Flag-Aenderungen und der Akkustatus sind so automatisch stuendlich im Portal sichtbar (uebererfuellt die 24-h-Anforderung bei minimalen Energiekosten von rund 150 ms Sendezeit plus 2 s Empfangsfenster pro Stunde).
2. Nach dem SYNC lauscht der Sender 2 Sekunden auf Kommandos.
3. Ein Klick auf den Reset-Button merkt das Kommando im Gateway vor. Beim naechsten SYNC wird `CMD RESET FLAGS` verschluesselt zugestellt, der Sender setzt beide Flags auf null und bestaetigt mit einem weiteren SYNC. Wirksamkeit somit in maximal einer Stunde.
4. Ereignisnachrichten (`STATUS NEW MAIL`, `STATUS BATTERY LOW`) loesen weiterhin E-Mails aus. Der Versand wird kurz aufgeschoben, bis der Paket-Burst des Senders abgearbeitet ist, damit das Reset-Kommando das Empfangsfenster nicht verpasst.

## Hinweise und Unterschiede zur MicroPython-Version

- Zustandsspeicher: statt `rtc.memory()` mit String-Parsing werden `RTC_DATA_ATTR`-Variablen verwendet. Ein Power-Cycle (Akku ab und wieder an) setzt beide Flags auf null; das ersetzt `resett_flags.py`.
- Board-Definition: fuer den Wireless Stick Lite V3 existiert in PlatformIO keine eigene Definition. Es wird die des WiFi LoRa 32 V3 verwendet, da beide Boards elektrisch identisch sind (ESP32-S3FN8, 8 MB Flash, gleiche SX1262-Pinbelegung).
- TLS: der SMTP-Client prueft das Zertifikat von smtp.gmail.com per `setCACert()` gegen das eingebettete Google-Root-Zertifikat GTS Root R1 (gueltig bis 2036, siehe `src/gateway/gts_root_r1.h`). Dafuer synchronisiert das Gateway nach dem WLAN-Aufbau die Systemzeit per NTP; ohne gueltige Zeit schlaegt die Zertifikatspruefung fehl. Sollte Google die Kette wechseln, muss das Zertifikat aus https://pki.goog/repository/ aktualisiert werden.
- Watchdog: die MicroPython-Version konnte waehrend eines langen SMTP-Versands theoretisch in den Watchdog-Reset laufen. Der C++-SMTP-Client fuettert den Watchdog daher aktiv mit.
- Padding-Pruefung: das Gateway validiert das PKCS7-Padding vollstaendig, bevor eine Nachricht akzeptiert wird. Ungueltige oder fremde Pakete werden verworfen.
- ADC: die Umrechnung `(raw / 4095) * 3.3 * 4.9` wurde beibehalten, damit die kalibrierten Schwellwerte gueltig bleiben. Genauer waere `analogReadMilliVolts()`, dann muessten die Schwellen aber neu vermessen werden.

## Sicherheitshinweis

Die `.env` Datei enthaelt Zugangsdaten und darf nicht eingecheckt werden (siehe `.gitignore`). Das Funkprotokoll bietet Vertraulichkeit, aber keinen Schutz gegen Replay-Angriffe: ein mitgeschnittenes Paket koennte erneut gesendet werden und loest dann eine E-Mail aus. Falls das relevant ist, kann ein Zaehler in die Nachricht aufgenommen werden, den das Gateway auf Monotonie prueft.
