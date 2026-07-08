// ============================================================
// Datei:   nvs_config.h
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================
// Liest Zugangsdaten und Schluessel aus dem NVS-Namespace "sys_sec".
// Namespace und Schluesselnamen sind identisch zur MicroPython-Version,
// damit bereits provisionierte Chips kompatibel bleiben.

#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H

#include <Arduino.h>
#include <vector>

class NvsConfig {
public:
    // Initialisiert den NVS und oeffnet den Namespace, true bei Erfolg
    bool begin();

    // Liest einen Blob als Bytefolge, gibt die gelesene Laenge zurueck
    size_t readBlob(const char* key, uint8_t* buf, size_t buf_len);

    // Liest einen Blob und liefert ihn als String
    String readString(const char* key);

    // Schreibt einen String als Blob und committet sofort.
    // Wird vom Web-Portal fuer Konfigurationsaenderungen genutzt.
    bool writeString(const char* key, const String& value);

    // Zerlegt eine kommagetrennte Liste in einzelne Strings
    static std::vector<String> splitList(const String& raw);

private:
    uint32_t handle_ = 0;
    bool open_ = false;
};

#endif
