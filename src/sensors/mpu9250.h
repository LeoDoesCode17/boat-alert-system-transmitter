#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace mpu9250
{
    constexpr uint8_t ADDR_PRIMARY = 0x68;
    constexpr uint8_t ADDR_ALT = 0x69;
    constexpr float TILT_THRESHOLD = 45.0f; // degrees

    struct Data
    {
        float aX = 0, aY = 0, aZ = 0, aSqrt = 0; // accelerometer
        float gX = 0, gY = 0, gZ = 0;            // gyroscope
        float mX = 0, mY = 0, mZ = 0;            // magnetometer
        float roll = 0, pitch = 0, yaw = 0;      // degrees
    };

    // ---- Calibration ----
    constexpr uint16_t CAL_SAMPLES = 100;           // accelerometer samples to average
    constexpr uint16_t CAL_INTERVAL_MS = 20;        // delay between samples (~2 s total)
    constexpr float CAL_MOTION_TOLERANCE_G = 0.15f; // max per-axis spread allowed while calibrating

    // Called during calibrate() with progress 0..100.
    using ProgressCallback = void (*)(uint8_t percent);

    // Call after Wire.begin().
    void begin(TwoWire &wire = Wire);

    // Blocking (~CAL_SAMPLES * CAL_INTERVAL_MS). Keep the device level and still.
    // Averages the accelerometer and stores the resulting roll/pitch as offsets,
    // which are then subtracted from every reading. Returns false (offsets stay
    // at 0) if the sensor is missing or the device moved during calibration.
    bool calibrate(ProgressCallback onProgress = nullptr);
    bool isCalibrated();
    float rollOffset();  // degrees
    float pitchOffset(); // degrees
    void resetCalibration();

    // Re-check presence, read all sensors, recompute orientation.
    void update();

    bool isConnected();
    const Data &data();

    // True when tilt exceeds TILT_THRESHOLD.
    bool isDanger();
} // namespace mpu9250