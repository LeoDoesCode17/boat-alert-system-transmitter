#include "gps.h"
#include <TinyGPSPlus.h>

namespace gps
{
    namespace
    {
        // Named "parser" (not "gps") to avoid clashing with the namespace name.
        TinyGPSPlus parser;
        HardwareSerial serial(1);

        double currentLat = DEFAULT_LATITUDE;
        double currentLon = DEFAULT_LONGITUDE;
        bool usingDefault = true;
        unsigned long lastValidFixTime = 0; // millis() of last accepted fix
        unsigned long lastByteMillis = 0;   // millis() a byte last arrived
        bool connected = false;
    } // namespace

    void begin()
    {
        serial.begin(BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    }

    void poll()
    {
        while (serial.available() > 0)
        {
            parser.encode(serial.read());
            lastByteMillis = millis();
        }
        connected = (millis() - lastByteMillis) < CONNECTED_TIMEOUT_MS;
    }

    // Uses the live fix while valid and fresh. If no valid fix has been seen
    // for FIX_TIMEOUT_MS, falls back to the default location so a dead/blocked
    // GPS doesn't leave the system transmitting stale data.
    void updateLocation()
    {
        if (parser.location.isValid() && parser.location.age() < 2000)
        {
            currentLat = parser.location.lat();
            currentLon = parser.location.lng();
            usingDefault = false;
            lastValidFixTime = millis();
        }
        else if (millis() - lastValidFixTime > FIX_TIMEOUT_MS)
        {
            currentLat = DEFAULT_LATITUDE;
            currentLon = DEFAULT_LONGITUDE;
            usingDefault = true;
        }
        // else: within the timeout window -> keep last known good value.
    }

    bool isConnected() { return connected; }
    bool isUsingDefaultLocation() { return usingDefault; }
    double latitude() { return currentLat; }
    double longitude() { return currentLon; }
    int satellites() { return parser.satellites.isValid() ? parser.satellites.value() : 0; }
} // namespace gps