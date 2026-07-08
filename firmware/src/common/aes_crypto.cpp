// ============================================================
// Datei:   aes_crypto.cpp
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================

#include "aes_crypto.h"
#include "mbedtls/aes.h"
#include "esp_random.h"

void AesCrypto::setKey(const uint8_t* key) {
    memcpy(key_, key, KEY_LEN);
}

size_t AesCrypto::encrypt(const String& text, uint8_t* out, size_t out_len) {
    size_t text_len = text.length();

    // PKCS7: aufgefuellte Laenge ist stets ein Vielfaches der Blockgroesse
    size_t pad_len    = BLOCK_LEN - (text_len % BLOCK_LEN);
    size_t padded_len = text_len + pad_len;
    size_t total_len  = BLOCK_LEN + padded_len;

    if (out_len < total_len) {
        Serial.println("Fehler: Ausgabepuffer zu klein");
        return 0;
    }

    // Zufaelligen IV erzeugen und an den Paketanfang schreiben
    esp_fill_random(out, BLOCK_LEN);

    // Klartext mit Padding in einen Arbeitspuffer kopieren
    uint8_t plain[128];
    if (padded_len > sizeof(plain)) {
        Serial.println("Fehler: Nachricht zu lang");
        return 0;
    }
    memcpy(plain, text.c_str(), text_len);
    memset(plain + text_len, (uint8_t)pad_len, pad_len);

    // CBC-Verschluesselung, IV wird von mbedtls veraendert, daher Kopie
    uint8_t iv[BLOCK_LEN];
    memcpy(iv, out, BLOCK_LEN);

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key_, 128);
    int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, padded_len,
                                   iv, plain, out + BLOCK_LEN);
    mbedtls_aes_free(&ctx);

    if (rc != 0) {
        Serial.println("Fehler: AES Verschluesselung fehlgeschlagen");
        return 0;
    }
    return total_len;
}

String AesCrypto::decrypt(const uint8_t* data, size_t len) {
    // Paket muss IV plus mindestens einen Cipher-Block enthalten
    if (len < 2 * BLOCK_LEN || ((len - BLOCK_LEN) % BLOCK_LEN) != 0) {
        Serial.println("Fehler: Ungueltige Paketlaenge");
        return String("");
    }

    size_t cipher_len = len - BLOCK_LEN;
    uint8_t plain[128];
    if (cipher_len > sizeof(plain)) {
        Serial.println("Fehler: Paket zu gross");
        return String("");
    }

    uint8_t iv[BLOCK_LEN];
    memcpy(iv, data, BLOCK_LEN);

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key_, 128);
    int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, cipher_len,
                                   iv, data + BLOCK_LEN, plain);
    mbedtls_aes_free(&ctx);

    if (rc != 0) {
        Serial.println("Fehler: AES Entschluesselung fehlgeschlagen");
        return String("");
    }

    // PKCS7-Padding pruefen und entfernen
    uint8_t pad = plain[cipher_len - 1];
    if (pad == 0 || pad > BLOCK_LEN || pad > cipher_len) {
        Serial.println("Fehler: Ungueltiges Padding");
        return String("");
    }
    for (size_t i = cipher_len - pad; i < cipher_len; i++) {
        if (plain[i] != pad) {
            Serial.println("Fehler: Ungueltiges Padding");
            return String("");
        }
    }

    size_t text_len = cipher_len - pad;
    String result;
    result.reserve(text_len);
    for (size_t i = 0; i < text_len; i++) {
        result += (char)plain[i];
    }
    return result;
}
