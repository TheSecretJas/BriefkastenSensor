// ============================================================
// Datei:   smtp_client.h
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================
// Minimaler SMTP-Client fuer Gmail ueber TLS (Port 465) mit
// Wiederholversuchen und deterministischem Verbindungsabbau.

#ifndef SMTP_CLIENT_H
#define SMTP_CLIENT_H

#include <Arduino.h>
#include <vector>
#include <functional>

class SmtpClient {
public:
    // Callback fuer Statusmeldungen: (Konsolentext, OLED-Kurztext)
    using LogCallback = std::function<void(const String&, const String&)>;

    SmtpClient(const String& user, const String& password);

    // Sendet eine E-Mail an alle Empfaenger, bis zu fuenf Versuche.
    // Rueckgabe: true bei Erfolg.
    bool dispatch(const std::vector<String>& targets,
                  const String& subject,
                  const String& body,
                  LogCallback log_cb = nullptr);

private:
    String host_ = "smtp.gmail.com";
    int    port_ = 465;
    String user_;
    String pass_;
};

#endif
