#include "mpu9250.h"
#include <MPU9250_asukiaaa.h>

namespace mpu9250
{
    namespace
    {
        MPU9250_asukiaaa sensor;
        TwoWire *bus = &Wire;
        Data current;
        bool connected = false;

        // Calibration offsets (degrees), subtracted from raw roll/pitch.
        float rollOff = 0.0f;
        float pitchOff = 0.0f;
        bool calibrated = false;

        // Wrap an angle into [-180, 180].
        float wrap180(float a)
        {
            while (a > 180.0f)
                a -= 360.0f;
            while (a < -180.0f)
                a += 360.0f;
            return a;
        }

        // Raw (uncalibrated) roll/pitch in degrees from an accelerometer vector.
        void accelToRollPitch(float ax, float ay, float az, float &roll, float &pitch)
        {
            roll = atan2(ay, az) * RAD_TO_DEG;
            pitch = atan2(-ax, sqrt(ay * ay + az * az)) * RAD_TO_DEG;
        }

        // Cheap I2C presence check (ACK on the bus).
        bool probe()
        {
            bus->beginTransmission(ADDR_PRIMARY);
            if (bus->endTransmission() == 0)
                return true;

            bus->beginTransmission(ADDR_ALT);
            return (bus->endTransmission() == 0);
        }

        // Roll/pitch from the accelerometer (minus calibration offsets),
        // tilt-compensated yaw from the magnetometer. Without hard/soft-iron
        // calibration, treat yaw as a relative heading rather than a precise
        // compass bearing.
        void updateOrientation()
        {
            float rawRoll, rawPitch;
            accelToRollPitch(current.aX, current.aY, current.aZ, rawRoll, rawPitch);

            // Tilt compensation must use the *physical* tilt (raw angles), not
            // the calibrated ones, or the heading would be skewed by the offsets.
            const float rollRad = rawRoll * DEG_TO_RAD;
            const float pitchRad = rawPitch * DEG_TO_RAD;

            const float mxComp = current.mX * cos(pitchRad) + current.mZ * sin(pitchRad);
            const float myComp = current.mX * sin(rollRad) * sin(pitchRad) +
                                 current.mY * cos(rollRad) -
                                 current.mZ * sin(rollRad) * cos(pitchRad);

            current.yaw = atan2(myComp, mxComp) * RAD_TO_DEG;
            if (current.yaw < 0)
                current.yaw += 360.0f;

            current.roll = wrap180(rawRoll - rollOff);
            current.pitch = rawPitch - pitchOff;
        }
    } // namespace

    void begin(TwoWire &wire)
    {
        bus = &wire;
        sensor.setWire(bus);
        sensor.beginAccel();
        sensor.beginGyro();
        sensor.beginMag();
        connected = probe();
        if (!connected)
        {
            Serial.println(F("MPU9250 not detected on I2C bus."));
        }
    }

    bool calibrate(ProgressCallback onProgress)
    {
        // Start from a clean slate so a failed run never leaves stale offsets.
        resetCalibration();

        connected = probe();
        if (!connected)
        {
            Serial.println(F("[CAL] MPU9250 not detected, skipping calibration."));
            return false;
        }

        double sumX = 0, sumY = 0, sumZ = 0;
        float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
        float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;
        uint16_t good = 0;

        for (uint16_t i = 0; i < CAL_SAMPLES; i++)
        {
            if (sensor.accelUpdate() == 0)
            {
                const float x = sensor.accelX();
                const float y = sensor.accelY();
                const float z = sensor.accelZ();

                sumX += x;
                sumY += y;
                sumZ += z;
                minX = min(minX, x);
                maxX = max(maxX, x);
                minY = min(minY, y);
                maxY = max(maxY, y);
                minZ = min(minZ, z);
                maxZ = max(maxZ, z);
                good++;
            }

            if (onProgress)
                onProgress((uint8_t)(((i + 1) * 100) / CAL_SAMPLES));

            delay(CAL_INTERVAL_MS);
        }

        if (good < CAL_SAMPLES / 2)
        {
            Serial.println(F("[CAL] Too many failed sensor reads."));
            return false;
        }

        if ((maxX - minX) > CAL_MOTION_TOLERANCE_G ||
            (maxY - minY) > CAL_MOTION_TOLERANCE_G ||
            (maxZ - minZ) > CAL_MOTION_TOLERANCE_G)
        {
            Serial.println(F("[CAL] Device moved during calibration."));
            return false;
        }

        // Average the accelerometer vector first, then convert to angles.
        // (Averaging the angles directly breaks near +/-180 deg roll.)
        float r, p;
        accelToRollPitch(sumX / good, sumY / good, sumZ / good, r, p);

        rollOff = r;
        pitchOff = p;
        calibrated = true;

        Serial.printf("[CAL] Done. Roll offset=%.2f Pitch offset=%.2f (%u samples)\n",
                      rollOff, pitchOff, good);
        return true;
    }

    bool isCalibrated() { return calibrated; }
    float rollOffset() { return rollOff; }
    float pitchOffset() { return pitchOff; }

    void resetCalibration()
    {
        rollOff = 0.0f;
        pitchOff = 0.0f;
        calibrated = false;
    }

    void update()
    {
        connected = probe();

        sensor.accelUpdate();
        current.aX = sensor.accelX();
        current.aY = sensor.accelY();
        current.aZ = sensor.accelZ();
        current.aSqrt = sensor.accelSqrt();

        sensor.gyroUpdate();
        current.gX = sensor.gyroX();
        current.gY = sensor.gyroY();
        current.gZ = sensor.gyroZ();

        sensor.magUpdate();
        current.mX = sensor.magX();
        current.mY = sensor.magY();
        current.mZ = sensor.magZ();

        updateOrientation();
    }

    bool isConnected() { return connected; }
    const Data &data() { return current; }

    bool isDanger()
    {
        return fabs(current.roll) > TILT_THRESHOLD || fabs(current.pitch) > TILT_THRESHOLD;
    }
} // namespace mpu9250