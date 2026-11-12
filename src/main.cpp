#include <Arduino.h>
#include <Wire.h>

#include "sensors/button.h"
#include "display/oled.h"
#include "sensors/gps.h"
#include "sensors/mpu9250.h"
#include "networks/lora.h"

// ---- I2C pins: ESP32-C3 Super Mini default I2C ----
#define I2C_SDA 8
#define I2C_SCL 9

constexpr unsigned long UPDATE_INTERVAL_MS = 500;
unsigned long lastUpdate = 0;

void scanI2C()
{
  Serial.println(F("Scanning I2C bus..."));
  byte found = 0;
  for (byte addr = 1; addr < 127; addr++)
  {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0)
    {
      Serial.printf("  Device found at 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0)
  {
    Serial.println(F("  No I2C devices found. Check wiring/pull-ups."));
  }
}

void sendTelemetry()
{
  const mpu9250::Data &d = mpu9250::data();

  char payload[lora::MAX_PAYLOAD_LEN];
  const int n = snprintf(payload, sizeof(payload),
                         "R:%.2f,P:%.2f,Y:%.2f,LAT:%.6f,LON:%.6f,FIX:%d,S:%d",
                         d.roll, d.pitch, d.yaw,
                         gps::latitude(), gps::longitude(),
                         gps::isUsingDefaultLocation() ? 0 : 1,
                         mpu9250::isDanger() ? 1 : 0);
  if (n <= 0 || (size_t)n >= sizeof(payload))
  {
    Serial.println(F("[LORA] Payload formatting failed/truncated"));
    return;
  }

  lora::sendEncrypted((const uint8_t *)payload, (size_t)n);
}

void printDebug()
{
  const mpu9250::Data &d = mpu9250::data();
  Serial.printf("Accel: X=%.2f Y=%.2f Z=%.2f |a|=%.2f\n", d.aX, d.aY, d.aZ, d.aSqrt);
  Serial.printf("Gyro : X=%.2f Y=%.2f Z=%.2f\n", d.gX, d.gY, d.gZ);
  Serial.printf("Orient: Roll=%.2f Pitch=%.2f Yaw=%.2f\n", d.roll, d.pitch, d.yaw);

  if (gps::isUsingDefaultLocation())
  {
    Serial.println(F("GPS  : no fix (timed out) - using default location"));
  }
  else
  {
    Serial.printf("GPS  : Lat=%.6f Lng=%.6f Sats=%d\n",
                  gps::latitude(), gps::longitude(), gps::satellites());
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000); // give native USB-CDC time to enumerate

  button::begin();

  Wire.begin(I2C_SDA, I2C_SCL);
  scanI2C();

  if (!oled::begin())
  {
    Serial.println(F("SSD1306 init failed. Check address/wiring."));
    while (true)
    {
      delay(10);
    }
  }
  oled::showBootMessage();
  delay(800); // let the boot message be readable

  mpu9250::begin(Wire);

  // Zero roll/pitch for however the sensor is mounted. The device must be
  // level and still. A failure is non-fatal: offsets stay at 0.
  oled::showCalibration(0);
  const bool calibrated = mpu9250::calibrate(oled::showCalibration);
  oled::showCalibrationResult(calibrated);
  delay(1500); // let the result be readable

  gps::begin();
  lora::begin(); // failure is reported on the status screen, not fatal

  Serial.println(F("Setup complete."));
  delay(500);

  oled::showLanding();
}

void loop()
{
  if (button::update())
  {
    Serial.println(F("BUTTON PRESSED"));
    oled::nextScreen();
  }

  // Feed the GPS parser every pass so NMEA bytes aren't lost between ticks.
  gps::poll();

  if (millis() - lastUpdate >= UPDATE_INTERVAL_MS)
  {
    lastUpdate = millis();

    mpu9250::update();
    gps::updateLocation();
    printDebug();

    if (lora::isConnected())
    {
      sendTelemetry();
    }
    else
    {
      Serial.println(F("LoRa : not connected, skipping TX"));
    }

    oled::refresh();
  }
}