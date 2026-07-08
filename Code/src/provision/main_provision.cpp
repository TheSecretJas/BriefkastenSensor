// ============================================================
// Datei:   main_provision.cpp
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================
// Ersetzt setup_NVS.py: schreibt die Zugangsdaten aus der .env Datei
// in den NVS-Namespace "sys_sec". Die Werte werden beim Kompilieren
// durch das Skript scripts/load_env.py als Defines eingebettet.
// Ablauf: .env pflegen, dieses Environment flashen, serielle Ausgabe
// pruefen, danach die eigentliche Firmware (sender/gateway) flashen.

#include <Arduino.h>
#include "nvs_flash.h"
#include "nvs.h"

// Hilfsmakros: Defines liegen bereits als String vor
#ifndef PROV_AES_MAIN
#define PROV_AES_MAIN ""
#endif
#ifndef PROV_WIFI_SSID
#define PROV_WIFI_SSID ""
#endif
#ifndef PROV_WIFI_PASS
#define PROV_WIFI_PASS ""
#endif
#ifndef PROV_SMTP_USER
#define PROV_SMTP_USER ""
#endif
#ifndef PROV_SMTP_PASS
#define PROV_SMTP_PASS ""
#endif
#ifndef PROV_RECIPIENTS
#define PROV_RECIPIENTS ""
#endif
#ifndef PROV_PORTAL_PASS
#define PROV_PORTAL_PASS ""
#endif

// Schreibt einen String als Blob, identisch zum MicroPython-Verhalten
static void storeBlob(nvs_handle_t handle, const char* key, const char* value) {
    if (strlen(value) == 0) {
        Serial.print("Uebersprungen (leer): ");
        Serial.println(key);
        return;
    }
    esp_err_t err = nvs_set_blob(handle, key, value, strlen(value));
    if (err == ESP_OK) {
        Serial.print("Gespeichert: ");
        Serial.println(key);
    } else {
        Serial.print("Fehler beim Speichern von ");
        Serial.println(key);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Starte Provisionierung des NVS");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        Serial.println("Fehler: NVS Initialisierung fehlgeschlagen");
        return;
    }

    nvs_handle_t handle;
    if (nvs_open("sys_sec", NVS_READWRITE, &handle) != ESP_OK) {
        Serial.println("Fehler: NVS Namespace konnte nicht geoeffnet werden");
        return;
    }

    // Der AES-Schluessel muss exakt 16 Zeichen umfassen (AES-128)
    if (strlen(PROV_AES_MAIN) != 16) {
        Serial.println("Warnung: AES_MAIN hat nicht exakt 16 Zeichen");
    }

    storeBlob(handle, "aes_main",   PROV_AES_MAIN);
    storeBlob(handle, "wifi_ssid",  PROV_WIFI_SSID);
    storeBlob(handle, "wifi_pass",  PROV_WIFI_PASS);
    storeBlob(handle, "smtp_user",  PROV_SMTP_USER);
    storeBlob(handle, "smtp_pass",  PROV_SMTP_PASS);
    storeBlob(handle, "recipients", PROV_RECIPIENTS);
    storeBlob(handle, "portal_pass", PROV_PORTAL_PASS);

    nvs_commit(handle);
    nvs_close(handle);

    Serial.println("Zugangsdaten erfolgreich im NVS gespeichert");
    Serial.println("Jetzt die eigentliche Firmware flashen");
}

void loop() {
    delay(10000);
}
