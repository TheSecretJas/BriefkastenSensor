// ============================================================
// Datei:   web_portal.h
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================
// LAN-Web-Portal des Gateways, erreichbar unter
// http://briefkastensensor.local/
// Zeigt den Zustand des Sendemoduls (Flags, Akkuspannung, Distanz,
// letzter Kontakt) sowie das Raumklima an, erlaubt das Anpassen der
// Konfiguration nach der initialen .env Provisionierung und stellt
// einen Button zum ferngesteuerten Zuruecksetzen der Sender-Flags
// bereit. Optional per HTTP Basic Auth geschuetzt (PORTAL_PASS).

#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include <functional>
#include "../common/nvs_config.h"

// Zuletzt bekannter Zustand des Sendemoduls
struct RemoteState {
    bool     valid        = false;
    uint8_t  mail_flag    = 0;
    uint8_t  batt_flag    = 0;
    float    voltage      = 0.0f;
    uint16_t distance     = 0;
    uint32_t last_seen_ms = 0;
};

class WebPortal {
public:
    // Liefert Temperatur und Luftfeuchte, Rueckgabe false wenn Sensor fehlt
    using ClimateProvider = std::function<bool(float& temp, float& hum)>;

    // Startet Webserver und mDNS, password leer = Portal ohne Login
    bool begin(NvsConfig* cfg, const String& password);

    // Muss zyklisch aus der Hauptschleife aufgerufen werden
    void handle();

    // Aktualisiert den angezeigten Zustand des Sendemoduls
    void updateRemote(uint8_t mail, uint8_t batt, float voltage, uint16_t distance);

    // Reset-Kommando: vom Portal gesetzt, vom LoRa-Handler abgeholt
    bool resetPending() const { return reset_pending_; }
    void clearResetPending() { reset_pending_ = false; }

    void setClimateProvider(ClimateProvider p) { climate_ = p; }

    // Statuszeile des Gateways (gleicher Text wie auf dem OLED)
    void attachStatusText(const String* status) { status_text_ = status; }

private:
    bool requireAuth();
    void handleRoot();
    void handleApiStatus();
    void handleApiReset();
    void handleApiConfig();

    WebServer       server_{80};
    NvsConfig*      cfg_ = nullptr;
    String          pass_;
    ClimateProvider climate_;
    const String*   status_text_ = nullptr;
    RemoteState     remote_;
    volatile bool   reset_pending_ = false;
    uint32_t        restart_at_ms_ = 0;
};

#endif
