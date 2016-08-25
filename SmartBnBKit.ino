#include <Schedule.h>
#include <ESP8266HTTPClient.h>
#include "Configuration.hpp"
#include "Input.hpp"
#include "Utils.hpp"

Configuration config;
Input input;

enum Pins {
    Door     = 4,
    Conf     = 12,
    Motion   = 14,
    RedLED   = 13,
    GreenLED = 15
};

// Timers
os_timer_t DoorTimer;
os_timer_t MotionTimer;

bool IsSomeoneInside = false;
bool waitForMotionEnd = false;

void changeState(bool on) {
    IsSomeoneInside = on;
    Serial.printf("IsSomeoneInside: %d\n", IsSomeoneInside);
    schedule_function([] { 
        HTTPClient http;
        http.begin("http://192.168.241.148:12345/" + String(IsSomeoneInside? "on" : "off"));
        http.GET();
    });
}

void setup() {
    Serial.begin(115200);

    input.init(Pins::Door);
    input.init(Pins::Motion);
    input.init(Pins::Conf);
    
    pinMode(Pins::RedLED, OUTPUT);
    pinMode(Pins::GreenLED, OUTPUT);
    digitalWrite(Pins::RedLED, HIGH);
    digitalWrite(Pins::GreenLED, HIGH);
    
    WiFi.mode(WIFI_STA);
    if (WiFi.SSID()) {
        WiFi.begin();
        
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.printf("\nWiFi connected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        config.begin("SmartBnbKit", "gotoptal");
    }
}

void loop() {
    input.onChanged(Pins::Door, [] (bool on) {
        digitalWrite(Pins::RedLED, !on);
        
        // If there is nothing against the sensor, that means the doors are closed
        // If motion sensor is on, that means there was somebody inside, so he must just leave
        if (!on && digitalRead(Pins::Motion)) {
            waitForMotionEnd = true;
        } else {
            waitForMotionEnd = false;
        }
    });
    input.onChanged(Pins::Motion, [] (bool on) {
        on = !on; // Get the proper logic state
        
        digitalWrite(Pins::GreenLED, !on);
        
        // We've got a move - someone is inside
        if (on) {
            changeState(true);
            waitForMotionEnd = false;
        } else if (waitForMotionEnd) {
            changeState(false);
        }
        
        // If there is no movement for 5 minutes
        // Note: This timeout will be reset after each PIR detection
        startTimer(&MotionTimer, [] {
            changeState(false);
        }, 5 * 60 * 1000, false);
    });
    
    // Config mode after holding button for at least 2 seconds
    input.onChanged(Pins::Conf, [] (bool on) {
        if (on) {
            static os_timer_t confTimer;
            startTimer(&confTimer, [] {
                if (!digitalRead(Pins::Conf)) {
                    schedule_function([] { config.begin("SmartBnbKit", "gotoptal"); });
                }
            }, 2000, false);
        }
    });
}
