#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU9250_asukiaaa.h>
#include <TinyGPSPlus.h>
#include <LoRa.h>

// ---- OLED geometry ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C // Some modules use 0x3D

// ---- I2C pins: ESP32-C3 Super Mini default I2C ----
#define I2C_SDA 8
#define I2C_SCL 9

// ---- Button pin ----
// GPIO0 is also a boot-mode strapping pin: fine to use as a normal input
// once the board has booted, just don't hold it low during power-on/reset.
#define BUTTON_PIN 0

// ---- GPS (GY-NEO6MV2) pins ----
// Uses UART1, separate from the USB-CDC Serial used for debug output.
// GPIO2 is also a boot-mode strapping pin: fine as a normal input once
// booted, just don't hold it low during power-on/reset.
#define GPS_RX_PIN 2 // ESP32 RX <- GPS TX
#define GPS_TX_PIN 3 // ESP32 TX -> GPS RX
#define GPS_BAUD 9600

// If no valid/fresh GPS fix has been obtained for this long, fall back to
// the default location below instead of transmitting stale/invalid data.
#define GPS_FIX_TIMEOUT_MS 15000UL

// Reference location used until (and unless) the GPS gets a real fix.
// TODO: set this to a meaningful reference point for your deployment
// (e.g. home dock / harbor coordinates).
#define DEFAULT_LATITUDE 0.0
#define DEFAULT_LONGITUDE 0.0

// ---- LoRa SX1278 (433MHz) pins ----
// Uses the ESP32-C3's default SPI pins.
#define LORA_SCK 4
#define LORA_MISO 5
#define LORA_MOSI 6
#define LORA_CS 7 // NSS
#define LORA_RST 1
#define LORA_DIO0 -1 // not connected - TX-only use doesn't need it (see setup())
#define LORA_FREQUENCY 433E6

// ---- Module status detection ----
#define MPU9250_ADDR_PRIMARY 0x68
#define MPU9250_ADDR_ALT 0x69
// If no byte has arrived from the GPS UART for this long, treat it as
// disconnected (this is about the UART link being alive, not about
// having a fix - a connected GPS with no satellites still sends NMEA).
#define GPS_CONNECTED_TIMEOUT_MS 3000UL

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MPU9250_asukiaaa mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

float aX, aY, aZ, aSqrt;
float gX, gY, gZ;
float mX, mY, mZ;
float roll, pitch, yaw; // degrees

double currentLat = DEFAULT_LATITUDE;
double currentLon = DEFAULT_LONGITUDE;
bool usingDefaultLocation = true;
unsigned long lastValidFixTime = 0; // millis() of last accepted GPS fix

// ---- Module status ----
bool mpuConnected = false;
bool gpsConnected = false;
bool loraConnected = false;
unsigned long lastGpsByteMillis = 0; // millis() a byte last arrived from the GPS UART

// ---- Button debounce state ----
const unsigned long DEBOUNCE_DELAY_MS = 50;
int buttonReading = HIGH;
int buttonStateDebounced = HIGH; // last debounced state
int lastButtonReading = HIGH;    // last raw reading, for edge detection
unsigned long lastDebounceTime = 0;

// ---- Screens ----
enum Screen
{
  SCREEN_LANDING = 0,
  SCREEN_SENSOR = 1,
  SCREEN_ORIENTATION = 2,
  SCREEN_GPS = 3,
  SCREEN_STATUS = 4,
  SCREEN_COUNT // keep last: total number of screens, used for wraparound
};
Screen currentScreen = SCREEN_LANDING;

void renderScreen(); // forward declaration (defined below, used by checkButton)

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

// Cheap I2C presence check (ACK on the bus), used to report MPU9250 status.
bool checkMpuConnected()
{
  Wire.beginTransmission(MPU9250_ADDR_PRIMARY);
  if (Wire.endTransmission() == 0)
    return true;

  Wire.beginTransmission(MPU9250_ADDR_ALT);
  return (Wire.endTransmission() == 0);
}

// Roll/pitch from the accelerometer, tilt-compensated yaw from the
// magnetometer. Note: for an accurate absolute heading the magnetometer
// normally needs hard/soft-iron calibration; without it, treat yaw as a
// reasonable relative heading rather than a precise compass bearing.
void updateOrientation()
{
  roll = atan2(aY, aZ) * RAD_TO_DEG;
  pitch = atan2(-aX, sqrt(aY * aY + aZ * aZ)) * RAD_TO_DEG;

  float rollRad = roll * DEG_TO_RAD;
  float pitchRad = pitch * DEG_TO_RAD;

  float mxComp = mX * cos(pitchRad) + mZ * sin(pitchRad);
  float myComp = mX * sin(rollRad) * sin(pitchRad) + mY * cos(rollRad) - mZ * sin(rollRad) * cos(pitchRad);

  yaw = atan2(myComp, mxComp) * RAD_TO_DEG;
  if (yaw < 0)
    yaw += 360.0;
}

// Uses the live GPS fix while it's valid and fresh. If no valid fix has
// been seen for GPS_FIX_TIMEOUT_MS, falls back to the default location
// so a dead/blocked GPS doesn't leave the system transmitting stale data.
void updateLocation()
{
  if (gps.location.isValid() && gps.location.age() < 2000)
  {
    currentLat = gps.location.lat();
    currentLon = gps.location.lng();
    usingDefaultLocation = false;
    lastValidFixTime = millis();
  }
  else if (millis() - lastValidFixTime > GPS_FIX_TIMEOUT_MS)
  {
    currentLat = DEFAULT_LATITUDE;
    currentLon = DEFAULT_LONGITUDE;
    usingDefaultLocation = true;
  }
  // else: still within the timeout window with no fresh fix -> keep
  // whatever currentLat/currentLon already held (last known good value).
}

void sendLoRaPacket()
{
  char payload[100];
  snprintf(payload, sizeof(payload),
           "R:%.2f,P:%.2f,Y:%.2f,LAT:%.6f,LON:%.6f,FIX:%d",
           roll, pitch, yaw, currentLat, currentLon, usingDefaultLocation ? 0 : 1);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  Serial.print(F("LoRa TX: "));
  Serial.println(payload);
}

// Draws text centered horizontally at the given y, at the given text size.
void printCentered(const char *text, int16_t y, uint8_t size)
{
  display.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.println(text);
}

void drawLandingScreen()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  printCentered("BOAT", 4, 2);
  printCentered("ALERT", 24, 2);
  printCentered("SYSTEM", 44, 2);
  display.display();
}

void drawSensorScreen()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("aX:%.2f aY:%.2f\n", aX, aY);
  display.printf("aZ:%.2f |a|:%.2f\n", aZ, aSqrt);
  display.println();
  display.printf("gX:%.2f\n", gX);
  display.printf("gY:%.2f\n", gY);
  display.printf("gZ:%.2f\n", gZ);
  display.display();
}

void drawOrientationScreen()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("ORIENTATION"));
  display.println();
  display.setTextSize(2);
  display.printf("R:%6.1f\n", roll);
  display.printf("P:%6.1f\n", pitch);
  display.printf("Y:%6.1f\n", yaw);
  display.display();
}

void drawGpsScreen()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("GPS"));

  display.printf("Lat: %.6f\n", currentLat);
  display.printf("Lng: %.6f\n", currentLon);
  display.println(usingDefaultLocation ? F("(default location)") : F("(live fix)"));

  display.println();
  display.printf("Sats: %d\n", gps.satellites.isValid() ? gps.satellites.value() : 0);
  display.display();
}

void drawStatusScreen()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("MODULE STATUS"));
  display.println();
  display.printf("MPU9250: %s\n", mpuConnected ? "OK" : "NOT CONNECTED");
  display.printf("GPS    : %s\n", gpsConnected ? "OK" : "NOT CONNECTED");
  display.printf("LoRa   : %s\n", loraConnected ? "OK" : "NOT CONNECTED");
  display.display();
}

// Draws whichever screen is currently selected.
void renderScreen()
{
  switch (currentScreen)
  {
  case SCREEN_LANDING:
    drawLandingScreen();
    break;
  case SCREEN_SENSOR:
    drawSensorScreen();
    break;
  case SCREEN_ORIENTATION:
    drawOrientationScreen();
    break;
  case SCREEN_GPS:
    drawGpsScreen();
    break;
  case SCREEN_STATUS:
    drawStatusScreen();
    break;
  default:
    break;
  }
}

void checkButton()
{
  int reading = digitalRead(BUTTON_PIN);

  // Raw level changed -> reset the debounce timer
  if (reading != lastButtonReading)
  {
    lastDebounceTime = millis();
  }

  // Level has been stable longer than the debounce delay -> accept it
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS)
  {
    if (reading != buttonStateDebounced)
    {
      buttonStateDebounced = reading;

      // Button wired to GND with INPUT_PULLUP -> pressed = LOW
      if (buttonStateDebounced == LOW)
      {
        Serial.println(F("BUTTON PRESSED"));

        currentScreen = (Screen)((currentScreen + 1) % SCREEN_COUNT);
        renderScreen(); // redraw immediately, don't wait for the next sensor tick
      }
    }
  }

  lastButtonReading = reading;
}

void setup()
{
  Serial.begin(115200);
  delay(1000); // give native USB-CDC time to enumerate

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  scanI2C();

  // ---- OLED init ----
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 init failed. Check address/wiring."));
    while (true)
    {
      delay(10);
    }
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("OLED OK"));
  display.println(F("Init MPU9250..."));
  display.display();

  // ---- MPU9250 init ----
  mpu.setWire(&Wire);
  mpu.beginAccel();
  mpu.beginGyro();
  mpu.beginMag();
  mpuConnected = checkMpuConnected();
  if (!mpuConnected)
  {
    Serial.println(F("MPU9250 not detected on I2C bus."));
  }

  // ---- GPS init ----
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // ---- LoRa init ----
  // DIO0 left unconnected: it's only needed for interrupt-driven receive
  // (onReceive()); a blocking beginPacket()/endPacket() TX doesn't use it.
  // A failed init no longer halts the system - it's reported on the
  // status screen instead, so the rest of the system keeps running.
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  loraConnected = LoRa.begin(LORA_FREQUENCY);
  if (!loraConnected)
  {
    Serial.println(F("LoRa init failed. Check wiring."));
  }
  else
  {
    Serial.println(F("LoRa init OK."));
  }

  Serial.println(F("Setup complete."));
  delay(500);

  currentScreen = SCREEN_LANDING;
  renderScreen();
}

unsigned long lastUpdate = 0;

void loop()
{
  checkButton(); // non-blocking, checked every loop pass so presses aren't missed

  // Feed the GPS parser every loop pass (not gated by the 500ms timer) so
  // incoming NMEA bytes aren't lost while waiting for the next sensor tick.
  // Any byte arriving at all means the GPS UART link is alive.
  while (gpsSerial.available() > 0)
  {
    gps.encode(gpsSerial.read());
    lastGpsByteMillis = millis();
  }
  gpsConnected = (millis() - lastGpsByteMillis) < GPS_CONNECTED_TIMEOUT_MS;

  if (millis() - lastUpdate >= 500)
  {
    lastUpdate = millis();

    mpuConnected = checkMpuConnected();

    mpu.accelUpdate();
    aX = mpu.accelX();
    aY = mpu.accelY();
    aZ = mpu.accelZ();
    aSqrt = mpu.accelSqrt();

    mpu.gyroUpdate();
    gX = mpu.gyroX();
    gY = mpu.gyroY();
    gZ = mpu.gyroZ();

    mpu.magUpdate();
    mX = mpu.magX();
    mY = mpu.magY();
    mZ = mpu.magZ();

    updateOrientation();
    updateLocation();

    Serial.printf("Accel: X=%.2f Y=%.2f Z=%.2f |a|=%.2f\n", aX, aY, aZ, aSqrt);
    Serial.printf("Gyro : X=%.2f Y=%.2f Z=%.2f\n", gX, gY, gZ);
    Serial.printf("Orient: Roll=%.2f Pitch=%.2f Yaw=%.2f\n", roll, pitch, yaw);

    if (usingDefaultLocation)
    {
      Serial.println(F("GPS  : no fix (timed out) - using default location"));
    }
    else
    {
      Serial.printf("GPS  : Lat=%.6f Lng=%.6f Sats=%d\n",
                    currentLat, currentLon,
                    gps.satellites.isValid() ? gps.satellites.value() : 0);
    }

    if (loraConnected)
    {
      sendLoRaPacket();
    }
    else
    {
      Serial.println(F("LoRa : not connected, skipping TX"));
    }

    // Only redraw the screen that's currently showing; the other screens
    // don't need to change on every sensor/GPS/status tick.
    if (currentScreen == SCREEN_SENSOR || currentScreen == SCREEN_ORIENTATION ||
        currentScreen == SCREEN_GPS || currentScreen == SCREEN_STATUS)
    {
      renderScreen();
    }
  }
}