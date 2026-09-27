# Boat Alert System

ESP32-C3 Super Mini firmware that reads an MPU9250 (accel/gyro/mag) and a
GY-NEO6MV2 GPS module, computes roll/pitch/yaw, and transmits the readings
plus GPS coordinates over a LoRa SX1278 (433MHz) link. Status is shown
locally on a 0.96" I2C OLED, cycled through with a push button.

## Hardware

- ESP32-C3 Super Mini
- 0.96" I2C OLED display (SSD1306 driver, 128x64)
- MPU9250 (I2C accelerometer/gyroscope/magnetometer)
- GY-NEO6MV2 GPS module (UART)
- LoRa SX1278 433MHz module (SPI)
- 1x push button

## Pin Configuration

| Peripheral | Signal | ESP32-C3 GPIO | Notes |
|---|---|---|---|
| OLED (I2C) | SDA | 8 | shared I2C bus with MPU9250 |
| OLED (I2C) | SCL | 9 | shared I2C bus with MPU9250 |
| MPU9250 (I2C) | SDA | 8 | shared I2C bus with OLED |
| MPU9250 (I2C) | SCL | 9 | shared I2C bus with OLED |
| Button | Signal | 0 | wired to GND, uses internal pull-up. Also a boot-strapping pin — safe as a normal input once booted, just don't hold it low during power-on/reset |
| GPS (UART1) | RX (ESP32) ← GPS TX | 2 | also a boot-strapping pin, same caveat as above |
| GPS (UART1) | TX (ESP32) → GPS RX | 3 | |
| LoRa (SPI) | SCK | 4 | |
| LoRa (SPI) | MISO | 5 | |
| LoRa (SPI) | MOSI | 6 | |
| LoRa (SPI) | NSS / CS | 7 | |
| LoRa | RESET | 1 | |
| LoRa | DIO0 | not connected | only needed for interrupt-driven receive; this build is TX-only |

Power: OLED, MPU9250, and LoRa run on 3.3V. The GY-NEO6MV2 can typically
take 3.3V or 5V depending on the module's onboard regulator — check the
silkscreen. The LoRa SX1278 is 3.3V only.

## Software

- **Framework:** PlatformIO, Arduino framework
- **Board:** `esp32-c3-devkitm-1` (generic ESP32-C3 target)
- Build flags enable native USB-CDC serial (`ARDUINO_USB_CDC_ON_BOOT`), required
  because most ESP32-C3 Super Mini boards have no separate USB-UART chip.

### Libraries

| Library | Purpose |
|---|---|
| `adafruit/Adafruit SSD1306` | OLED driver |
| `adafruit/Adafruit GFX Library` | OLED graphics primitives |
| `asukiaaa/MPU9250_asukiaaa` | MPU9250 accel/gyro/mag readout |
| `mikalhart/TinyGPSPlus` | NMEA sentence parsing |
| `sandeepmistry/LoRa` | SX127x LoRa driver |

## OLED Screens

Cycled in order each time the button is pressed (debounced, 50ms):

1. **Landing** – static "BOAT ALERT SYSTEM" title screen (default on boot)
2. **Sensor** – raw accelerometer (X/Y/Z, magnitude) and gyroscope (X/Y/Z)
3. **Orientation** – computed roll, pitch, yaw (large text)
4. **GPS** – current latitude/longitude, whether it's a live fix or the
   default fallback, and satellite count
5. **Status** – connection status (OK / NOT CONNECTED) for MPU9250, GPS,
   and LoRa

## Orientation (Roll / Pitch / Yaw)

The MPU9250 doesn't output orientation directly, so it's derived each
cycle:

- **Roll / Pitch** — standard accelerometer tilt formulas
- **Yaw** — tilt-compensated magnetometer heading, using the computed
  roll/pitch to correct the mag readings

Note: without hard/soft-iron calibration of the magnetometer, yaw is a
reasonable *relative* heading rather than a precise absolute compass
bearing.

## GPS Fallback Logic

- A fix is only accepted as "live" if `gps.location.isValid()` is true
  **and** the fix is fresh (age < 2s).
- If no valid/fresh fix has been seen for `GPS_FIX_TIMEOUT_MS`
  (currently **15 seconds**), the system falls back to a hardcoded
  default location (`DEFAULT_LATITUDE` / `DEFAULT_LONGITUDE`, currently
  placeholder `0.0, 0.0`).
  **TODO: replace these with a real reference point** (e.g. home
  dock/harbor coordinates) before deployment.
- Between fixes and before that timeout, the last known good
  coordinates are kept rather than immediately flipping to the default.

## LoRa Transmission

Every 500ms (if LoRa initialized successfully), a CSV packet is sent:

```
R:<roll>,P:<pitch>,Y:<yaw>,LAT:<latitude>,LON:<longitude>,FIX:<0|1>
```

Example:

```
R:2.13,P:-1.05,Y:184.30,LAT:-5.123456,LON:120.123456,FIX:1
```

`FIX:1` means the coordinates are from a live GPS fix; `FIX:0` means the
default fallback location is being sent instead.

## Module Status Detection

Shown on the Status screen, re-checked continuously:

| Module | How it's determined |
|---|---|
| MPU9250 | Direct I2C probe (address `0x68`, falls back to `0x69`) |
| GPS | Whether any byte has arrived on the GPS UART within the last 3s (checks the physical link, not whether it has a satellite fix) |
| LoRa | Result of `LoRa.begin()` at startup (confirms SPI communication with the chip) |

If LoRa fails to initialize, the system no longer halts — it continues
running and reports the failure on the Status screen (and skips LoRa
transmission each cycle).

## Building & Uploading

```
pio run -t upload
pio device monitor
```

Serial monitor runs at 115200 baud.