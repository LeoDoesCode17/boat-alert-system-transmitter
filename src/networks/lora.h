#pragma once

#include <Arduino.h>

namespace lora
{
    // ---- Pins (ESP32-C3 SPI) ----
    constexpr int SCK_PIN = 4;
    constexpr int MISO_PIN = 5;
    constexpr int MOSI_PIN = 6;
    constexpr int CS_PIN = 7; // NSS
    constexpr int RST_PIN = 1;
    constexpr int DIO0_PIN = -1; // not connected: TX-only use doesn't need it
    constexpr long FREQUENCY = 433E6;

    // Packet header (sent as AAD, in clear): protocol version, device id.
    constexpr uint8_t PROTOCOL_VERSION = 0x01;
    constexpr uint8_t DEVICE_ID = 0xA1;

    // Max plaintext size (length is sent as a single byte).
    constexpr size_t MAX_PAYLOAD_LEN = 128;

    // Returns false (and does not halt) if the radio isn't found.
    bool begin();
    bool isConnected();

    // Encrypts with AES-128-GCM and transmits.
    // Packet: [aad(2)][nonce][len(1)][ciphertext][tag]
    bool sendEncrypted(const uint8_t *plaintext, size_t len);
} // namespace lora