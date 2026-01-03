//
// Created by Andreas W. on 02.01.26.
//
#include <sstream>

#ifdef ARDUINO
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#endif

#include "Network.h"

#ifdef ARDUINO
#include <set>
#endif



Network::Network(const char *ssid, const char *password, Status::Status &status) : status(status)
#ifdef ARDUINO
    , ntpClient(wifiUdp)
#endif
{
    this->ssid = ssid;
    this->password = password;
}


[[noreturn]] void Network::task(void *pvParameters) {
#ifdef ARDUINO
    status.connecting();

    WiFi.begin(ssid, password);

    {
        std::stringstream ss;
        ss << "Connecting to WiFi: " << this->ssid;
#ifdef ARDUINO
        Serial.println(ss.str().c_str());
#endif
    }

    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
#ifdef ARDUINO
        Serial.print(".");
#endif
    }
    status.connected();


    ntpClient.begin();

    ntpClient.update();

#ifdef ARDUINO
    Serial.println("connected");
#endif

    {
        std::stringstream ss;
        ss << "ip " << WiFi.localIP().toString().c_str() << " " << " " << ntpClient.getEpochTime() << " " << ntpClient.getFormattedTime().c_str();
#ifdef ARDUINO
        Serial.println(ss.str().c_str());
#endif
    }

    auto lastNtpUpdate = ntpClient.getEpochTime();

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (ntpClient.getEpochTime() - lastNtpUpdate > 60) {
            auto result = ntpClient.update();
            std::stringstream ss;
            ss << "NTP update " << ntpClient.getFormattedTime().c_str() << " " << result;
#ifdef ARDUINO
            Serial.println(ss.str().c_str());
#endif
            lastNtpUpdate = ntpClient.getEpochTime();
        }

    }
#endif
}
