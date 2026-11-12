#pragma once
#include <Arduino.h>
#include "core/constant.h"
#include <mbedtls/gcm.h>

namespace aes
{
    void initialize();
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
