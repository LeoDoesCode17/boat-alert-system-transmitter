#pragma once

#include <Arduino.h>

namespace button
{
    // GPIO0 is also a boot-mode strapping pin: fine to use as a normal input
    // once the board has booted, just don't hold it low during power-on/reset.
    constexpr int PIN = 0;
    constexpr unsigned long DEBOUNCE_DELAY_MS = 50;

    void begin();

    // Call on every loop() pass (non-blocking).
    // Returns true exactly once per debounced press.
    bool update();
} // namespace button