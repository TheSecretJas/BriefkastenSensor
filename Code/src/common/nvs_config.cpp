// ============================================================
// Datei:   nvs_config.cpp
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================

#include "nvs_config.h"
#include "nvs_flash.h"
#include "nvs.h"

bool NvsConfig::begin() {
    // NVS-Partition initialisieren, bei beschaedigter Partition neu anlegen
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        Serial.println("Fehler: NVS Initialisierung fehlgeschlagen");
        return false;
    }

    nvs_handle_t handle;
    err = nvs_open("sys_sec", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        Serial.println("Fehler: NVS Namespace sys_sec nicht vorhanden");
        return false;
    }

    handle_ = handle;
    open_ = true;
    return true;
}

size_t NvsConfig::readBlob(const char* key, uint8_t* buf, size_t buf_len) {
    if (!open_) return 0;

    size_t len = buf_len;
    esp_err_t err = nvs_get_blob((nvs_handle_t)handle_, key, buf, &len);
    if (err != ESP_OK) {
        Serial.print("Fehler: Konfigurationsschluessel fehlt: ");
        Serial.println(key);
        return 0;
    }
    return len;
}

String NvsConfig::readString(const char* key) {
    uint8_t buf[128];
    size_t len = readBlob(key, buf, sizeof(buf));
    if (len == 0) return String("");

    String result;
    result.reserve(len);
    for (size_t i = 0; i < len; i++) {
        result += (char)buf[i];
    }
    return result;
}

bool NvsConfig::writeString(const char* key, const String& value) {
    // Eigenes Schreib-Handle, das Lese-Handle bleibt unangetastet
    nvs_handle_t handle;
    if (nvs_open("sys_sec", NVS_READWRITE, &handle) != ESP_OK) {
        Serial.println("Fehler: NVS Schreibzugriff fehlgeschlagen");
        return false;
    }
    esp_err_t err = nvs_set_blob(handle, key, value.c_str(), value.length());
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        Serial.print("Fehler: Speichern fehlgeschlagen fuer ");
        Serial.println(key);
        return false;
    }
    return true;
}

std::vector<String> NvsConfig::splitList(const String& raw) {
    std::vector<String> items;
    int start = 0;
    while (start < (int)raw.length()) {
        int comma = raw.indexOf(',', start);
        if (comma < 0) comma = raw.length();
        String item = raw.substring(start, comma);
        item.trim();
        if (item.length() > 0) items.push_back(item);
        start = comma + 1;
    }
    return items;
}
