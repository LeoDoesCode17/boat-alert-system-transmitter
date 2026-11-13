#include "lora.h"
#include <SPI.h>
#include <LoRa.h>
#include "security/aes.h"

namespace lora
{
    namespace
    {
        bool connected = false;
        char mac[MAC_STR_LEN];
        bool mac_ready = false;

        // Builds "AA:BB:CC:DD:EE:FF" from the factory-programmed eFuse MAC.
        // getEfuseMac() stores the first octet in the lowest byte, so reading
        // from the low byte up gives the usual textual order.
        void construct_mac_address()
        {
            const uint64_t mac_result = ESP.getEfuseMac();
            uint8_t b[6];
            for (int i = 0; i < 6; i++)
            {
                b[i] = (mac_result >> (8 * i)) & 0xFF;
            }
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     b[0], b[1], b[2], b[3], b[4], b[5]);
            mac_ready = true;
        }

    }

    bool begin()
    {
        // DIO0 left unconnected: only needed for interrupt-driven receive;
        // blocking beginPacket()/endPacket() TX doesn't use it.
        SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
        LoRa.setPins(CS_PIN, RST_PIN, DIO0_PIN);
        connected = LoRa.begin(FREQUENCY);
        construct_mac_address();
        Serial.printf("[LORA] MAC: %s\n", mac);

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

    bool get_mac_address(char *out, size_t out_len)
    {
        if (out == nullptr || out_len < MAC_STR_LEN)
        {
            Serial.println("[LORA] MAC buffer too small (need MAC_STR_LEN bytes)");
            return false;
        }

        // Works even if begin() was never called or the radio init failed.
        if (!mac_ready)
            construct_mac_address();

        memcpy(out, mac, MAC_STR_LEN);
        return true;
    }
} // namespace lora