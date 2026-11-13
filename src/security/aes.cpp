#include "aes.h"
#include "core/secret.h"
#include <Preferences.h>

namespace
{
    // NVS names are limited to 15 characters.
    constexpr const char *NVS_NAMESPACE = "aes";
    constexpr const char *NVS_KEY_NAME = "gcm_key";
    constexpr size_t KEY_LEN = sizeof(secret::AES_KEY);

    // The key actually used for crypto: loaded from flash by initialize().
    uint8_t active_key[KEY_LEN];
    bool initialized = false;
} // namespace

namespace aes
{
    bool initialize()
    {
        initialized = false;

        if (KEY_LEN * 8 != constant::KEY_BITS)
        {
            Serial.println("[AES] secret.h key size does not match constant::KEY_BITS");
            return false;
        }

        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, false)) // false = read/write
        {
            Serial.println("[AES] Failed to open NVS");
            return false;
        }

        if (prefs.isKey(NVS_KEY_NAME) && prefs.getBytesLength(NVS_KEY_NAME) == KEY_LEN)
        {
            // Key already provisioned: fetch it.
            if (prefs.getBytes(NVS_KEY_NAME, active_key, KEY_LEN) != KEY_LEN)
            {
                Serial.println("[AES] Failed to read key from flash");
                prefs.end();
                return false;
            }
            Serial.println("[AES] Key loaded from flash");
        }
        else
        {
            // Missing (first boot) or wrong size (corrupted): provision from secret.h.
            if (prefs.putBytes(NVS_KEY_NAME, secret::AES_KEY, KEY_LEN) != KEY_LEN)
            {
                Serial.println("[AES] Failed to write key to flash");
                prefs.end();
                return false;
            }
            memcpy(active_key, secret::AES_KEY, KEY_LEN);
            Serial.println("[AES] No valid key in flash, provisioned from secret.h");
        }

        prefs.end();
        initialized = true;
        return true;
    }

    bool reset_key()
    {
        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, false))
        {
            Serial.println("[AES] Failed to open NVS");
            return false;
        }
        prefs.remove(NVS_KEY_NAME);
        prefs.end();

        return initialize();
    }

    bool encrypt(
        const uint8_t *plaintext,
        size_t plaintext_len,
        const uint8_t *nonce,
        const uint8_t *aad,
        size_t aad_len,
        uint8_t *ciphertext,
        uint8_t *tag)
    {
        if (!initialized)
        {
            Serial.println("[AES] Not initialized, call aes::initialize() first");
            return false;
        }

        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);

        int ret = mbedtls_gcm_setkey(
            &ctx,
            MBEDTLS_CIPHER_ID_AES,
            active_key,
            constant::KEY_BITS);

        if (ret == 0)
        {
            ret = mbedtls_gcm_crypt_and_tag(
                &ctx,
                MBEDTLS_GCM_ENCRYPT,
                plaintext_len,
                nonce,
                constant::NONCE_LEN,
                aad,
                aad_len,
                plaintext,
                ciphertext,
                constant::TAG_LEN,
                tag);
        }

        mbedtls_gcm_free(&ctx);
        return ret == 0;
    }

    bool decrypt(
        const uint8_t *ciphertext,
        size_t ciphertext_len,
        const uint8_t *nonce,
        const uint8_t *aad,
        size_t aad_len,
        const uint8_t *tag,
        uint8_t *plaintext)
    {
        if (!initialized)
        {
            Serial.println("[AES] Not initialized, call aes::initialize() first");
            return false;
        }

        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);

        int ret = mbedtls_gcm_setkey(
            &ctx,
            MBEDTLS_CIPHER_ID_AES,
            active_key,
            constant::KEY_BITS);

        if (ret == 0)
        {
            ret = mbedtls_gcm_auth_decrypt(
                &ctx,
                ciphertext_len,
                nonce,
                constant::NONCE_LEN,
                aad,
                aad_len,
                tag,
                constant::TAG_LEN,
                ciphertext,
                plaintext);
        }

        mbedtls_gcm_free(&ctx);

        return ret == 0;
    }

    void generate_nonce(uint8_t *nonce)
    {
        for (size_t i = 0; i < constant::NONCE_LEN; i += 4)
        {
            uint32_t r = esp_random();
            memcpy(nonce + i, &r, 4);
        }
    }
} // namespace aes