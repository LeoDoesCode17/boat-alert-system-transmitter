#include "oled.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "sensors/gps.h"
#include "sensors/mpu9250.h"
#include "networks/lora.h"

namespace oled
{
    namespace
    {
        Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
        Screen current = SCREEN_LANDING;

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

        void drawLanding()
        {
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            printCentered("BOAT", 4, 2);
            printCentered("ALERT", 24, 2);
            printCentered("SYSTEM", 44, 2);
            display.display();
        }

        void drawSensor()
        {
            const mpu9250::Data &d = mpu9250::data();
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.printf("aX:%.2f aY:%.2f\n", d.aX, d.aY);
            display.printf("aZ:%.2f |a|:%.2f\n", d.aZ, d.aSqrt);
            display.println();
            display.printf("gX:%.2f\n", d.gX);
            display.printf("gY:%.2f\n", d.gY);
            display.printf("gZ:%.2f\n", d.gZ);
            display.display();
        }

        void drawOrientation()
        {
            const mpu9250::Data &d = mpu9250::data();
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println(F("ORIENTATION"));
            display.println();
            display.setTextSize(2);
            display.printf("R:%6.1f\n", d.roll);
            display.printf("P:%6.1f\n", d.pitch);
            display.printf("Y:%6.1f\n", d.yaw);
            display.display();
        }

        void drawGps()
        {
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println(F("GPS"));

            display.printf("Lat: %.6f\n", gps::latitude());
            display.printf("Lng: %.6f\n", gps::longitude());
            display.println(gps::isUsingDefaultLocation() ? F("(default location)") : F("(live fix)"));

            display.println();
            display.printf("Sats: %d\n", gps::satellites());
            display.display();
        }

        void drawStatus()
        {
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println(F("MODULE STATUS"));
            display.println();
            display.printf("MPU9250: %s\n", mpu9250::isConnected() ? "OK" : "NOT CONNECTED");
            display.printf("GPS    : %s\n", gps::isConnected() ? "OK" : "NOT CONNECTED");
            display.printf("LoRa   : %s\n", lora::isConnected() ? "OK" : "NOT CONNECTED");
            display.display();
        }

        void drawCalibration(uint8_t percent)
        {
            if (percent > 100)
                percent = 100;

            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            printCentered("CALIBRATING", 4, 1);
            printCentered("Keep device level", 20, 1);
            printCentered("and do not move", 30, 1);

            // Progress bar: outline 100x10, fill grows inside it
            display.drawRect(14, 46, 100, 10, SSD1306_WHITE);
            display.fillRect(16, 48, (96 * percent) / 100, 6, SSD1306_WHITE);
            display.display();
        }

        void drawCalibrationResult(bool ok)
        {
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            printCentered("CALIBRATION", 4, 1);

            if (ok)
            {
                printCentered("DONE", 20, 2);

                char line[24];
                snprintf(line, sizeof(line), "R:%.1f P:%.1f",
                         mpu9250::rollOffset(), mpu9250::pitchOffset());
                printCentered(line, 46, 1);
            }
            else
            {
                printCentered("FAILED", 20, 2);
                printCentered("Offsets set to 0", 42, 1);
                printCentered("Check/keep still", 52, 1);
            }
            display.display();
        }
    } // namespace

    bool begin()
    {
        return display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    }

    void showBootMessage()
    {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println(F("OLED OK"));
        display.println(F("Init modules..."));
        display.display();
    }

    void showCalibration(uint8_t percent)
    {
        drawCalibration(percent);
    }

    void showCalibrationResult(bool ok)
    {
        drawCalibrationResult(ok);
    }

    void showLanding()
    {
        current = SCREEN_LANDING;
        render();
    }

    void nextScreen()
    {
        current = (Screen)((current + 1) % SCREEN_COUNT);
        render();
    }

    void render()
    {
        switch (current)
        {
        case SCREEN_LANDING:
            drawLanding();
            break;
        case SCREEN_SENSOR:
            drawSensor();
            break;
        case SCREEN_ORIENTATION:
            drawOrientation();
            break;
        case SCREEN_GPS:
            drawGps();
            break;
        case SCREEN_STATUS:
            drawStatus();
            break;
        default:
            break;
        }
    }

    void refresh()
    {
        if (current != SCREEN_LANDING)
        {
            render();
        }
    }

    Screen currentScreen() { return current; }
} // namespace oled