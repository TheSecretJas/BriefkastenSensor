// ============================================================
// Datei:   smtp_client.cpp
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================

#include "smtp_client.h"
#include "gts_root_r1.h"
#include <WiFiClientSecure.h>
#include <base64.h>
#include "esp_task_wdt.h"

static const uint32_t SMTP_TIMEOUT_MS = 15000;
static const int      MAX_RETRIES     = 5;

SmtpClient::SmtpClient(const String& user, const String& password)
    : user_(user), pass_(password) {}

// Liest eine vollstaendige SMTP-Antwort (auch mehrzeilig) und wirft
// bei Statuscodes 4xx oder 5xx einen Fehler ueber den Rueckgabewert
static bool readResponse(WiFiClientSecure& tls, String& last_line) {
    uint32_t start = millis();
    while (true) {
        // Watchdog fuettern, da der Versand laenger als das
        // WDT-Timeout der Hauptschleife dauern kann
        esp_task_wdt_reset();
        if (millis() - start > SMTP_TIMEOUT_MS) {
            last_line = "Timeout beim Lesen";
            return false;
        }
        if (!tls.available()) {
            delay(10);
            continue;
        }
        String line = tls.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        last_line = line;
        // Letzte Zeile einer Antwort traegt ein Leerzeichen nach dem Code
        if (line.length() >= 4 && line.charAt(3) == ' ') break;
        start = millis();
    }
    if (last_line.length() >= 3 &&
        (last_line.charAt(0) == '4' || last_line.charAt(0) == '5')) {
        return false;
    }
    return true;
}

// Sendet ein Kommando und wartet auf die Bestaetigung des Servers
static bool sendCommand(WiFiClientSecure& tls, const String& cmd, String& err) {
    tls.print(cmd + "\r\n");
    return readResponse(tls, err);
}

bool SmtpClient::dispatch(const std::vector<String>& targets,
                          const String& subject,
                          const String& body,
                          LogCallback log_cb) {
    auto logEvent = [&](const String& console_text, const String& oled_text) {
        if (log_cb) {
            log_cb(console_text, oled_text);
        } else {
            Serial.println(console_text);
        }
    };

    if (targets.empty()) {
        logEvent("Abbruch: Keine gueltigen Empfaenger", "Err No Target");
        return false;
    }

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        logEvent("SMTP Versuch " + String(attempt), "SMTP Try " + String(attempt));

        WiFiClientSecure tls;
        // Server-Zertifikat wird gegen das Google-Root-Zertifikat
        // GTS Root R1 geprueft. Voraussetzung: korrekte Systemzeit
        // per NTP, sonst schlaegt die Gueltigkeitspruefung fehl.
        tls.setCACert(GTS_ROOT_R1);
        tls.setTimeout(SMTP_TIMEOUT_MS / 1000);

        String err;
        bool ok = false;

        do {
            esp_task_wdt_reset();
            if (!tls.connect(host_.c_str(), port_)) {
                err = "Verbindungsaufbau fehlgeschlagen";
                break;
            }
            if (!readResponse(tls, err)) break;
            if (!sendCommand(tls, "EHLO esp32", err)) break;
            if (!sendCommand(tls, "AUTH LOGIN", err)) break;
            if (!sendCommand(tls, base64::encode(user_), err)) break;
            if (!sendCommand(tls, base64::encode(pass_), err)) break;
            if (!sendCommand(tls, "MAIL FROM:<" + user_ + ">", err)) break;

            bool rcpt_ok = true;
            for (const String& r : targets) {
                if (!sendCommand(tls, "RCPT TO:<" + r + ">", err)) {
                    rcpt_ok = false;
                    break;
                }
            }
            if (!rcpt_ok) break;

            if (!sendCommand(tls, "DATA", err)) break;

            // Empfaengerliste fuer den To-Header zusammenfuegen
            String to_field;
            for (size_t i = 0; i < targets.size(); i++) {
                if (i > 0) to_field += ", ";
                to_field += targets[i];
            }

            String msg;
            msg.reserve(256 + body.length());
            msg += "From: " + user_ + "\r\n";
            msg += "To: " + to_field + "\r\n";
            msg += "Subject: " + subject + "\r\n\r\n";
            msg += "Moin zusammen,\r\n\r\n";
            msg += body + "\r\n";
            msg += "Liebe Gruesse,\r\nJonas\r\n.\r\n";
            tls.print(msg);

            if (!readResponse(tls, err)) break;
            sendCommand(tls, "QUIT", err);
            ok = true;
        } while (false);

        tls.stop();

        if (ok) {
            logEvent("Versand erfolgreich", "Mail Sent OK");
            return true;
        }

        logEvent("SMTP Fehler: " + err, "SMTP Err " + String(attempt));
        if (attempt < MAX_RETRIES) {
            for (int i = 0; i < 10; i++) {
                esp_task_wdt_reset();
                delay(500);
            }
        }
    }

    logEvent("Kritischer Fehler: SMTP Versand fehlgeschlagen", "Mail Failed");
    return false;
}
