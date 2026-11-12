#pragma once

#include <Arduino.h>

namespace gps
{
    // ---- Pins (UART1, separate from the USB-CDC Serial used for debug) ----
    // GPIO2 is also a boot-mode strapping pin: fine as a normal input once
    // booted, just don't hold it low during power-on/reset.
    constexpr int RX_PIN = 2; // ESP32 RX <- GPS TX
    constexpr int TX_PIN = 3; // ESP32 TX -> GPS RX
    constexpr uint32_t BAUD = 9600;

    // No valid/fresh fix for this long -> fall back to the default location.
    constexpr unsigned long FIX_TIMEOUT_MS = 15000UL;
    // No byte from the GPS UART for this long -> treat module as disconnected.
    constexpr unsigned long CONNECTED_TIMEOUT_MS = 3000UL;

    // Reference location used until (and unless) the GPS gets a real fix.
    // TODO: set this to a meaningful reference point for your deployment.
    constexpr double DEFAULT_LATITUDE = 0.0;
    constexpr double DEFAULT_LONGITUDE = 0.0;

    void begin();

    // Feed the NMEA parser. Call on every loop() pass (not timer-gated).
    void poll();

    // Refresh latitude/longitude from the parser (or fall back to default).
    void updateLocation();

    bool isConnected(); // UART link alive (not the same as having a fix)
    bool isUsingDefaultLocation();
    double latitude();
    double longitude();
    int satellites();
} // namespace gps