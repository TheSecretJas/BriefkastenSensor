# Anleitung: PlatformIO-Setup und Inbetriebnahme

Projekt: Briefkastensensor / Autor: Jonas Thern
Hinweis: Diese Anleitung wurde mit Unterstützung von künstlicher Intelligenz (Claude, Anthropic) erstellt.

Diese Anleitung führt durch die Einrichtung von PlatformIO und die vollständige Inbetriebnahme beider Boards (Sender im Briefkasten und Gateway im Haus).

---

## 1. Voraussetzungen

Benötigt werden:

- Visual Studio Code (https://code.visualstudio.com)
- Die Erweiterung "PlatformIO IDE" (in VS Code: Extensions, nach "PlatformIO" suchen, installieren, VS Code danach neu starten). PlatformIO installiert Compiler, Toolchain und esptool beim ersten Build automatisch.
- Ein USB-C-Datenkabel (kein reines Ladekabel).
- Windows: falls das Board nach dem Anstecken nicht als COM-Port erscheint, den CP210x-Treiber von Silicon Labs installieren. Linux: der Benutzer muss in der Gruppe `dialout` sein (`sudo usermod -a -G dialout $USER`, danach neu anmelden).

Wichtig: Immer nur EIN Board gleichzeitig anschließen. PlatformIO wählt den Port automatisch; bei zwei angeschlossenen Boards kann sonst das falsche geflasht werden.

## 2. Projekt einrichten

1. Den Ordner `briefkasten-cpp` aus dem Zip an einen dauerhaften Ort entpacken.
2. In VS Code: "File > Open Folder" und den Ordner `briefkasten-cpp` öffnen. PlatformIO erkennt die `platformio.ini` und richtet das Projekt ein (erster Start dauert einige Minuten, da die ESP32-Plattform heruntergeladen wird).
3. Im Projektstamm die Datei `.env.example` als `.env` kopieren und alle Werte eintragen:
   - `AES_MAIN`: exakt 16 Zeichen, der gemeinsame Schlüssel für beide Boards
   - `WIFI_SSID` / `WIFI_PASS`: Zugangsdaten des Heimnetzes
   - `SMTP_USER` / `SMTP_PASS`: Gmail-Adresse und App-Passwort
   - `RECIPIENTS`: Empfänger, kommagetrennt
   - `PORTAL_PASS`: Passwort für das Web-Portal (dringend empfohlen, Benutzer ist `admin`)

   Die `.env` bleibt lokal auf dem Rechner und wird nie auf die Chips kopiert; die Werte wandern nur einmalig per Provisionierungs-Firmware in den NVS.

4. Ein Terminal in VS Code öffnen ("Terminal > New Terminal"). Falls der Befehl `pio` dort nicht gefunden wird, stattdessen das PlatformIO-eigene Terminal nutzen (PlatformIO-Symbol in der Seitenleiste > "PIO Terminal") oder die im Folgenden genannten Aufgaben über die PlatformIO-Oberfläche anklicken (Project Tasks > Umgebung > General/Platform).

## 3. Gateway einrichten (Heltec WiFi LoRa 32 V3)

Das Gateway zuerst einrichten, damit es bereitsteht, sobald der Sender das erste Mal sendet.

1. Gateway per USB anschließen (nur dieses Board).

2. Flash vollständig löschen. Das entfernt Reste einer eventuell zuvor installierten Firmware und alle alten NVS-Daten, sodass der Chip garantiert sauber startet:
   ```
   pio run -e provision -t erase
   ```
   Falls der Vorgang mit einem Verbindungsfehler abbricht: die PRG/BOOT-Taste auf dem Board gedrückt halten, kurz RST drücken, PRG loslassen und den Befehl wiederholen (das Board ist dann im Download-Modus).

3. Provisionierung flashen. Dabei liest das Build-Skript die `.env` und brennt eine Einmal-Firmware, die die Werte in den NVS schreibt:
   ```
   pio run -e provision -t upload
   pio device monitor
   ```
   Im Monitor muss erscheinen: "Gespeichert: aes_main", "Gespeichert: wifi_ssid" usw. und abschließend "Zugangsdaten erfolgreich im NVS gespeichert". Den Monitor mit Strg+C beenden. Erscheint stattdessen "Warnung: ... fehlt in der .env Datei", die `.env` prüfen und Schritt 3 wiederholen.

4. Gateway-Firmware flashen (die NVS-Daten bleiben dabei erhalten, es wird nur die Anwendung ersetzt):
   ```
   pio run -e gateway -t upload
   pio device monitor
   ```
   Erwarteter Ablauf im Monitor: WLAN verbindet, "Systemzeit gesetzt" (NTP, wichtig für den TLS-E-Mail-Versand), "Portal erreichbar unter http://briefkastensensor.local/", "Gateway betriebsbereit". Das OLED zeigt das Raumklima.

5. Portal testen: Im Browser eines Geräts im selben WLAN http://briefkastensensor.local/ öffnen und mit Benutzer `admin` und dem `PORTAL_PASS` anmelden. Falls die .local-Adresse nicht auflöst (ältere Windows-Versionen ohne Bonjour, manche Android-Geräte), die IP-Adresse aus der seriellen Ausgabe verwenden. Das Sendemodul zeigt zunächst überall "-", da noch kein SYNC-Paket eingegangen ist.

## 4. Sender einrichten (Heltec Wireless Stick Lite V3)

1. Gateway vom USB trennen (es kann weiter über den Akku oder ein Netzteil laufen), Sender anschließen.

2. Flash löschen, dann dieselbe Provisionierung flashen (identische `.env`, damit der AES-Schlüssel auf beiden Chips gleich ist):
   ```
   pio run -e provision -t erase
   pio run -e provision -t upload
   pio device monitor
   ```
   Ausgabe wie beim Gateway prüfen. WLAN- und SMTP-Werte landen zwar auch auf dem Sender, werden dort aber nicht verwendet; das ist unkritisch.

3. Sender-Firmware flashen:
   ```
   pio run -e sender -t upload
   pio device monitor
   ```
   Der Sender durchläuft sofort einen kompletten Messzyklus: Spannung und Distanz erscheinen im Monitor, dann "Sende Sync: SYNC ...", das zweisekündige Empfangsfenster und "Wechsle in Deep Sleep". Im Portal müssen jetzt Spannung, Distanz und Flags sichtbar sein.

   Hinweis: Im Deep Sleep ist der Chip für Uploads nicht ansprechbar. Für erneutes Flashen die RST-Taste drücken (der Upload startet dann während der Bootphase) oder den Download-Modus wie oben erzwingen.

## 5. Funktionstest vor dem Einbau

1. Posteinwurf simulieren: Hand oder Karton vor den ToF-Sensor (unter 65 mm), RST am Sender drücken. Erwartung: "STATUS NEW MAIL" im Log, E-Mail "Posteinwurf" kommt an, OLED zeigt "NEW MAIL ARRIVED", Portal zeigt "POST EINGEWORFEN".
2. Flag-Reset testen: Im Portal den Button "Sender-Flags zurücksetzen" klicken (Button zeigt "Reset wartet auf Sender..."), dann RST am Sender drücken. Der Sender empfängt das Kommando im Empfangsfenster, im Portal springt der Zustand auf "leer" zurück.
3. Danach den Sender im Briefkasten montieren und einmal RST drücken, damit Distanz-Nulllage und Portalanzeige zur realen Einbausituation passen.

## 6. Alltagsbetrieb und Stolpersteine

- Der Sender meldet sich stündlich per SYNC; "Letzter Kontakt" im Portal sollte nie deutlich über 60 Minuten steigen. Tut er es doch, ist Akku oder Funkstrecke zu prüfen.
- Ein Klick auf den Reset-Button wirkt erst beim nächsten stündlichen Kontakt (maximal 1 h Verzögerung); das ist konstruktionsbedingt, da der Sender dazwischen tief schläft.
- Konfigurationsänderungen im Portal (WLAN, SMTP, Empfänger) lösen einen Neustart des Gateways aus. Der AES-Schlüssel ist bewusst nur über eine erneute Provisionierung änderbar und muss dann auf BEIDEN Chips erneuert werden.
- Ein Power-Cycle des Senders (Akku ab und wieder an) setzt beide Flags auf null.
- Schlägt der E-Mail-Versand mit TLS-Fehlern fehl, zuerst prüfen, ob im Bootlog "Systemzeit gesetzt" erscheint (NTP braucht Internetzugang). Bleibt es bei Fehlern, könnte Google die Zertifikatskette gewechselt haben; dann `src/gateway/gts_root_r1.h` gemäß README aktualisieren.
- Upload schlägt fehl / Port nicht gefunden: anderes USB-Kabel testen, Download-Modus erzwingen (PRG halten, RST tippen, PRG loslassen), unter Windows Treiber prüfen. Wählt PlatformIO den falschen Port, diesen explizit angeben: `--upload-port COM5` (Windows) bzw. `--upload-port /dev/ttyUSB0` (Linux).
- "Could not open COM5, the port doesn't exist" bzw. "Zugriff verweigert": meist hält ein anderes Programm (z. B. ein noch offener serieller Monitor oder Thonny) den Port belegt. Programm schließen und Upload wiederholen.

## 7. Befehls-Kurzreferenz

```
pio run -e provision -t erase     Flash vollständig löschen (einmalig pro Board)
pio run -e provision -t upload    Zugangsdaten aus der .env in den NVS schreiben
pio run -e gateway -t upload      Gateway-Firmware flashen
pio run -e sender -t upload       Sender-Firmware flashen
pio device monitor -b 115200      Serielle Ausgabe ansehen (Strg+C beendet)
```

Alle Befehle sind in VS Code auch grafisch erreichbar: PlatformIO-Symbol in der Seitenleiste, dann "Project Tasks" und die jeweilige Umgebung (provision, gateway, sender).
