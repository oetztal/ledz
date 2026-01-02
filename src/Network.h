//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_WIFI_H
#define UNTITLED_WIFI_H

#include "Network.h"

#include <WiFiUdp.h>
#include <NTPClient.h>

#include <Adafruit_NeoPixel.h>


#include "Status.h"
#include "strip/Strip.h"

class Network {
    const char *ssid;
    const char *password;

    WiFiUDP wifiUdp;
    NTPClient ntpClient;

    Status::Status &status;

public:
    Network(const char *ssid, const char *password, Status::Status& status);

    [[noreturn]] void task(void *pvParameters);

    // Static trampoline function
    static void taskWrapper(void *pvParameters) {
        // Cast the void pointer back to a Network instance
        auto *instance = static_cast<Network *>(pvParameters);
        instance->task(pvParameters);
    }
};


#endif //UNTITLED_WIFI_H
