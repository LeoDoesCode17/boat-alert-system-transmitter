#include "aes.h"

namespace
{
    const uint8_t AES_KEY[16] = {
        0x10, 0x21, 0x32, 0x43,
        0x54, 0x65, 0x76, 0x87,
        0x98, 0xA9, 0xBA, 0xCB,
        0xDC, 0xED, 0xFE, 0x0F};
    // note :
    /*
    nonce == iv
    */
}
namespace aes
{
    void initialize()
    {
        // retreive the truth AES_KEY from flash memory
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
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);

        int ret = mbedtls_gcm_setkey(
            &ctx,
            MBEDTLS_CIPHER_ID_AES,
            AES_KEY,
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
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);

        int ret = mbedtls_gcm_setkey(
            &ctx,
            MBEDTLS_CIPHER_ID_AES,
            AES_KEY,
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
}