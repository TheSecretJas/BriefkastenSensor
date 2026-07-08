// ============================================================
// Datei:   aes_crypto.h
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================
// AES-128 im CBC-Modus mit zufaelligem IV und PKCS7-Padding.
// Paketformat auf der Funkstrecke: [16 Byte IV][Ciphertext].
// Kompatibel zum bisherigen MicroPython-Format (ucryptolib, Modus 2).

#ifndef AES_CRYPTO_H
#define AES_CRYPTO_H

#include <Arduino.h>

class AesCrypto {
public:
    static const size_t KEY_LEN   = 16;
    static const size_t BLOCK_LEN = 16;

    // Setzt den 16 Byte langen AES-Schluessel
    void setKey(const uint8_t* key);

    // Verschluesselt einen Text, schreibt IV plus Ciphertext nach out.
    // out muss mindestens BLOCK_LEN + aufgerundete Textlaenge fassen.
    // Rueckgabe: Gesamtlaenge des Pakets, 0 bei Fehler.
    size_t encrypt(const String& text, uint8_t* out, size_t out_len);

    // Entschluesselt ein Paket (IV plus Ciphertext) und prueft das Padding.
    // Rueckgabe: Klartext, leerer String bei Fehler.
    String decrypt(const uint8_t* data, size_t len);

private:
    uint8_t key_[KEY_LEN] = {0};
};

#endif
