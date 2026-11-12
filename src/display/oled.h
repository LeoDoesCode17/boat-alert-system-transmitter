#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace oled
{
    // ---- SSD1306 OLED geometry ----
    constexpr int SCREEN_WIDTH = 128;
    constexpr int SCREEN_HEIGHT = 64;
    constexpr int OLED_RESET = -1;
    constexpr uint8_t SCREEN_ADDRESS = 0x3C; // Some modules use 0x3D

    enum Screen
    {
        SCREEN_LANDING = 0,
        SCREEN_SENSOR = 1,
        SCREEN_ORIENTATION = 2,
        SCREEN_GPS = 3,
        SCREEN_STATUS = 4,
        SCREEN_COUNT // keep last: total number of screens, used for wraparound
    };

    // Call after Wire.begin(). Returns false if the display isn't found.
    // The display is bound to the global Wire bus.
    bool begin();

    // Shows the "OLED OK / Init modules..." message during setup.
    void showBootMessage();

    // ---- Calibration screens ----
    // Transient screens shown during setup. They are drawn directly and are NOT
    // part of the button-cycled Screen enum. Signature matches
    // mpu9250::ProgressCallback so it can be passed straight to calibrate().
    void showCalibration(uint8_t percent); // "CALIBRATING" + progress bar (0..100)
    void showCalibrationResult(bool ok);   // "DONE" + offsets, or "FAILED"

    // Switch to the landing screen and draw it.
    void showLanding();

    // Advance to the next screen (wraps around) and redraw immediately.
    void nextScreen();

    // Redraw the current screen unconditionally.
    void render();

    // Periodic refresh: redraws the current screen, except the static landing screen.
    void refresh();

    Screen currentScreen();
} // namespace oled