#include "lora.h"
#include <SPI.h>
#include <LoRa.h>
#include "security/aes.h"

namespace lora
{
    namespace
    {
        bool connected = false;
    }

    bool begin()
    {
        // DIO0 left unconnected: only needed for interrupt-driven receive;
        // blocking beginPacket()/endPacket() TX doesn't use it.
        SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
        LoRa.setPins(CS_PIN, RST_PIN, DIO0_PIN);
        connected = LoRa.begin(FREQUENCY);

        if (connected)
            Serial.println(F("LoRa init OK."));
        else
            Serial.println(F("LoRa init failed. Check wiring."));

        return connected;
    }

    bool isConnected() { return connected; }

    bool sendEncrypted(const uint8_t *plaintext, size_t len)
    {
        if (!connected)
            return false;

        if (len == 0 || len > MAX_PAYLOAD_LEN)
        {
            Serial.println("[LORA] Invalid payload length");
            return false;
        }

        const uint8_t aad[] = {PROTOCOL_VERSION, DEVICE_ID};
        const size_t aad_len = sizeof(aad);
        uint8_t nonce[constant::NONCE_LEN];
        uint8_t tag[constant::TAG_LEN];
        uint8_t ciphertext[MAX_PAYLOAD_LEN]; // fixed size instead of a VLA

        aes::generate_nonce(nonce);
        if (!aes::encrypt(plaintext, len, nonce, aad, aad_len, ciphertext, tag))
        {
            Serial.println("[AES] Failed to encrypt message");
            return false;
        }

        LoRa.beginPacket();
        LoRa.write(aad, aad_len);
        LoRa.write(nonce, constant::NONCE_LEN);
        LoRa.write((uint8_t)len);
        LoRa.write(ciphertext, len);
        LoRa.write(tag, constant::TAG_LEN);
        const bool ok = LoRa.endPacket() == 1;

        Serial.println(ok ? "[LORA] Success to transmit message" : "[LORA] Transmit failed");
        return ok;
    }
} // namespace lora