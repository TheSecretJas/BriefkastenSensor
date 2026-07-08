# Anleitung: Umstieg von MicroPython auf das C++ Setup

Projekt: Briefkastensensor / Autor: Jonas Thern
Hinweis: Diese Anleitung wurde mit Unterstuetzung von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.

Diese Anleitung fuehrt durch die Einrichtung von PlatformIO und die vollstaendige Umruestung beider Boards (Sender im Briefkasten und Gateway) von MicroPython auf die neue C++ Firmware.

---

## 1. Voraussetzungen

Benoetigt werden:

- Visual Studio Code (https://code.visualstudio.com)
- Die Erweiterung "PlatformIO IDE" (in VS Code: Extensions, nach "PlatformIO" suchen, installieren, VS Code danach neu starten). PlatformIO installiert Compiler, Toolchain und esptool beim ersten Build automatisch.
- Ein USB-C-Datenkabel (kein reines Ladekabel).
- Windows: falls das Board nach dem Anstecken nicht als COM-Port erscheint, den CP210x-Treiber von Silicon Labs installieren. Linux: der Benutzer muss in der Gruppe `dialout` sein (`sudo usermod -a -G dialout $USER`, danach neu anmelden).

Wichtig: Immer nur EIN Board gleichzeitig anschliessen. PlatformIO waehlt den Port automatisch; bei zwei angeschlossenen Boards kann sonst das falsche geflasht werden.

## 2. Projekt einrichten

1. Den Ordner `briefkasten-cpp` aus dem Zip an einen dauerhaften Ort entpacken.
2. In VS Code: "File > Open Folder" und den Ordner `briefkasten-cpp` oeffnen. PlatformIO erkennt die `platformio.ini` und richtet das Projekt ein (erster Start dauert einige Minuten, da die ESP32-Plattform heruntergeladen wird).
3. Im Projektstamm die Datei `.env.example` als `.env` kopieren und alle Werte eintragen:
   - `AES_MAIN`: exakt 16 Zeichen, identisch zum bisherigen Schluessel (oder ein neuer, da ohnehin beide Boards neu provisioniert werden)
   - `WIFI_SSID` / `WIFI_PASS`: Zugangsdaten des Heimnetzes
   - `SMTP_USER` / `SMTP_PASS`: Gmail-Adresse und App-Passwort
   - `RECIPIENTS`: Empfaenger, kommagetrennt
   - `PORTAL_PASS`: Passwort fuer das Web-Portal (dringend empfohlen, Benutzer ist `admin`)

   Die `.env` bleibt lokal auf dem Rechner und wird nie auf die Chips kopiert; die Werte wandern nur einmalig per Provisionierungs-Firmware in den NVS.

4. Ein Terminal in VS Code oeffnen ("Terminal > New Terminal"). Falls der Befehl `pio` dort nicht gefunden wird, stattdessen das PlatformIO-eigene Terminal nutzen (PlatformIO-Symbol in der Seitenleiste > "PIO Terminal") oder die im Folgenden genannten Aufgaben ueber die PlatformIO-Oberflaeche anklicken (Project Tasks > Umgebung > General/Platform).

## 3. Gateway umruesten (Heltec WiFi LoRa 32 V3)

Das Gateway zuerst umruesten, damit es bereitsteht, sobald der Sender das erste Mal sendet.

1. Gateway per USB anschliessen (nur dieses Board).

2. Flash vollstaendig loeschen. Das entfernt MicroPython, das Dateisystem und alle alten NVS-Daten. Noetig, weil sich die Partitionstabellen von MicroPython und Arduino unterscheiden:
   ```
   pio run -e provision -t erase
   ```
   Falls der Vorgang mit einem Verbindungsfehler abbricht: die PRG/BOOT-Taste auf dem Board gedrueckt halten, kurz RST druecken, PRG loslassen und den Befehl wiederholen (das Board ist dann im Download-Modus).

3. Provisionierung flashen. Dabei liest das Build-Skript die `.env` und brennt eine Einmal-Firmware, die die Werte in den NVS schreibt:
   ```
   pio run -e provision -t upload
   pio device monitor
   ```
   Im Monitor muss erscheinen: "Gespeichert: aes_main", "Gespeichert: wifi_ssid" usw. und abschliessend "Zugangsdaten erfolgreich im NVS gespeichert". Den Monitor mit Strg+C beenden. Erscheint stattdessen "Warnung: ... fehlt in der .env Datei", die `.env` pruefen und Schritt 3 wiederholen.

4. Gateway-Firmware flashen (die NVS-Daten bleiben dabei erhalten, es wird nur die Anwendung ersetzt):
   ```
   pio run -e gateway -t upload
   pio device monitor
   ```
   Erwarteter Ablauf im Monitor: WLAN verbindet, "Systemzeit gesetzt" (NTP, wichtig fuer den TLS-E-Mail-Versand), "Portal erreichbar unter http://briefkastensensor.local/", "Gateway betriebsbereit". Das OLED zeigt das Raumklima.

5. Portal testen: Im Browser eines Geraets im selben WLAN http://briefkastensensor.local/ oeffnen und mit Benutzer `admin` und dem `PORTAL_PASS` anmelden. Falls die .local-Adresse nicht aufloest (aeltere Windows-Versionen ohne Bonjour, manche Android-Geraete), die IP-Adresse aus der seriellen Ausgabe verwenden. Das Sendemodul zeigt zunaechst ueberall "-", da noch kein SYNC-Paket eingegangen ist.

## 4. Sender umruesten (Heltec Wireless Stick Lite V3)

1. Gateway vom USB trennen (es kann weiter ueber den Akku oder ein Netzteil laufen), Sender anschliessen.

2. Flash loeschen, dann dieselbe Provisionierung flashen (identische `.env`, damit der AES-Schluessel auf beiden Chips gleich ist):
   ```
   pio run -e provision -t erase
   pio run -e provision -t upload
   pio device monitor
   ```
   Ausgabe wie beim Gateway pruefen. WLAN- und SMTP-Werte landen zwar auch auf dem Sender, werden dort aber nicht verwendet; das ist unkritisch.

3. Sender-Firmware flashen:
   ```
   pio run -e sender -t upload
   pio device monitor
   ```
   Der Sender durchlaeuft sofort einen kompletten Messzyklus: Spannung und Distanz erscheinen im Monitor, dann "Sende Sync: SYNC ...", das zweisekuendige Empfangsfenster und "Wechsle in Deep Sleep". Im Portal muessen jetzt Spannung, Distanz und Flags sichtbar sein.

   Hinweis: Im Deep Sleep ist der Chip fuer Uploads nicht ansprechbar. Fuer erneutes Flashen die RST-Taste druecken (der Upload startet dann waehrend der Bootphase) oder den Download-Modus wie oben erzwingen.

## 5. Funktionstest vor dem Einbau

1. Posteinwurf simulieren: Hand oder Karton vor den ToF-Sensor (unter 65 mm), RST am Sender druecken. Erwartung: "STATUS NEW MAIL" im Log, E-Mail "Posteinwurf" kommt an, OLED zeigt "NEW MAIL ARRIVED", Portal zeigt "POST EINGEWORFEN".
2. Flag-Reset testen: Im Portal den Button "Sender-Flags zuruecksetzen" klicken (Button zeigt "Reset wartet auf Sender..."), dann RST am Sender druecken. Der Sender empfaengt das Kommando im Empfangsfenster, im Portal springt der Zustand auf "leer" zurueck.
3. Danach den Sender im Briefkasten montieren und einmal RST druecken, damit Distanz-Nulllage und Portalanzeige zur realen Einbausituation passen.

## 6. Alltagsbetrieb und Stolpersteine

- Der Sender meldet sich stuendlich per SYNC; "Letzter Kontakt" im Portal sollte nie deutlich ueber 60 Minuten steigen. Tut er es doch, ist Akku oder Funkstrecke zu pruefen.
- Ein Klick auf den Reset-Button wirkt erst beim naechsten stuendlichen Kontakt (maximal 1 h Verzoegerung); das ist konstruktionsbedingt, da der Sender dazwischen tief schlaeft.
- Konfigurationsaenderungen im Portal (WLAN, SMTP, Empfaenger) loesen einen Neustart des Gateways aus. Der AES-Schluessel ist bewusst nur ueber eine erneute Provisionierung aenderbar und muss dann auf BEIDEN Chips erneuert werden.
- Ein Power-Cycle des Senders (Akku ab und wieder an) setzt beide Flags auf null, das ersetzt das alte `resett_flags.py`.
- Schlaegt der E-Mail-Versand mit TLS-Fehlern fehl, zuerst pruefen, ob im Bootlog "Systemzeit gesetzt" erscheint (NTP braucht Internetzugang). Bleibt es bei Fehlern, koennte Google die Zertifikatskette gewechselt haben; dann `src/gateway/gts_root_r1.h` gemaess README aktualisieren.
- Upload schlaegt fehl / Port nicht gefunden: anderes USB-Kabel testen, Download-Modus erzwingen (PRG halten, RST tippen, PRG loslassen), unter Windows Treiber pruefen. Waehlt PlatformIO den falschen Port, diesen explizit angeben: `--upload-port COM5` (Windows) bzw. `--upload-port /dev/ttyUSB0` (Linux).

## 7. Befehls-Kurzreferenz

```
pio run -e provision -t erase     Flash vollstaendig loeschen (einmalig pro Board)
pio run -e provision -t upload    Zugangsdaten aus der .env in den NVS schreiben
pio run -e gateway -t upload      Gateway-Firmware flashen
pio run -e sender -t upload       Sender-Firmware flashen
pio device monitor -b 115200      Serielle Ausgabe ansehen (Strg+C beendet)
```

Alle Befehle sind in VS Code auch grafisch erreichbar: PlatformIO-Symbol in der Seitenleiste, dann "Project Tasks" und die jeweilige Umgebung (provision, gateway, sender).
