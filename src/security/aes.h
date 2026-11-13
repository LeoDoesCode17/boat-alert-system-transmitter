#pragma once
#include <Arduino.h>
#include "core/constant.h"
#include <mbedtls/gcm.h>

namespace aes
{
    // Loads the AES key from flash (NVS) into RAM.
    // - key exists in flash  -> just load it
    // - no (valid) key       -> write the key from secret.h to flash, then use it
    // Must be called once at startup, before encrypt()/decrypt().
    // Returns false on failure (encrypt()/decrypt() then refuse to run).
    bool initialize();

    // Erases the key stored in flash and re-provisions it from secret.h.
    // Use this after you change the key in secret.h.
    bool reset_key();

    bool encrypt(
        const uint8_t *plaintext,
        size_t plaintext_len,
        const uint8_t *nonce,
        const uint8_t *aad,
        size_t aad_len,
        uint8_t *ciphertext,
        uint8_t *tag);
    bool decrypt(
        const uint8_t *ciphertext,
        size_t ciphertext_len,
        const uint8_t *nonce,
        const uint8_t *aad,
        size_t aad_len,
        const uint8_t *tag,
        uint8_t *plaintext);
    void generate_nonce(uint8_t *nonce);
} // namespace aes