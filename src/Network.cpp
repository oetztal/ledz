//
// Created by Andreas W. on 02.01.26.
//
#include <sstream>

#include <WiFi.h>
#include <Adafruit_NeoPixel.h>

#include "Network.h"

#include "../../../../.platformio/packages/toolchain-riscv32-esp@8.4.0+2021r2-patch5/riscv32-esp-elf/include/c++/8.4.0/set"



Network::Network(const char *ssid, const char *password, Status::Status &status) : status(status), ntpClient(wifiUdp) {
    this->ssid = ssid;
    this->password = password;
}


[[noreturn]] void Network::task(void *pvParameters) {
    status.connecting();

    WiFi.begin(ssid, password);

    {
        std::stringstream ss;
        ss << "Connecting to WiFi: " << this->ssid;
        Serial.println(ss.str().c_str());
    }

    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
    }
    status.connected();


    ntpClient.begin();

    ntpClient.update();

    Serial.println("connected");

    {
        std::stringstream ss;
        ss << "ip " << WiFi.localIP().toString().c_str() << " " << " " << ntpClient.getEpochTime() << " " << ntpClient.getFormattedTime().c_str();
        Serial.println(ss.str().c_str());
    }

    auto lastNtpUpdate = ntpClient.getEpochTime();

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (ntpClient.getEpochTime() - lastNtpUpdate > 60) {
            auto result = ntpClient.update();
            std::stringstream ss;
            ss << "NTP update " << ntpClient.getFormattedTime().c_str() << " " << result;
            Serial.println(ss.str().c_str());
            lastNtpUpdate = ntpClient.getEpochTime();
        }

    }
}
