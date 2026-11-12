#include "button.h"

namespace button
{
    namespace
    {
        int stateDebounced = HIGH; // last debounced state
        int lastReading = HIGH;    // last raw reading, for edge detection
        unsigned long lastDebounceTime = 0;
    } // namespace

    void begin()
    {
        pinMode(PIN, INPUT_PULLUP);
    }

    bool update()
    {
        bool pressed = false;
        const int reading = digitalRead(PIN);

        // Raw level changed -> reset the debounce timer
        if (reading != lastReading)
        {
            lastDebounceTime = millis();
        }

        // Level stable longer than the debounce delay -> accept it
        if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS)
        {
            if (reading != stateDebounced)
            {
                stateDebounced = reading;

                // Wired to GND with INPUT_PULLUP -> pressed = LOW
                if (stateDebounced == LOW)
                {
                    pressed = true;
                }
            }
        }

        lastReading = reading;
        return pressed;
    }
} // namespace button