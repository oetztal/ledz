// Device ID generation helper
// Generates unique device identifier from ESP32 MAC address
//

#ifndef LEDZ_DEVICEID_H
#define LEDZ_DEVICEID_H

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_system.h>

#include <array>
#include <cstdio>
#include <cstring>

namespace DeviceId {
    /**
     * Get unique device ID based on MAC address
     * Format: "AABBCC" where AABBCC are the last 3 bytes of MAC
     * @return Device ID string
     */
    inline String getDeviceId() {
        uint64_t mac = ESP.getEfuseMac();
        std::array<uint8_t, 6> mac_bytes;
        memcpy(mac_bytes.data(), &mac, 6);

        std::array<char, 16> id;
        snprintf(id.data(), id.size(), "%02X%02X%02X", mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        return String(id.data());
    }

    /**
     * Get full MAC address as string
     * Format: "AA:BB:CC:DD:EE:FF"
     * @return MAC address string
     */
    inline String getMacAddress() {
        uint64_t mac = ESP.getEfuseMac();
        std::array<uint8_t, 6> mac_bytes;
        memcpy(mac_bytes.data(), &mac, 6);

        std::array<char, 18> mac_str;
        snprintf(mac_str.data(), mac_str.size(), "%02X:%02X:%02X:%02X:%02X:%02X", mac_bytes[0], mac_bytes[1],
                 mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        return String(mac_str.data());
    }
} // namespace DeviceId

#endif // ARDUINO

#endif // LEDZ_DEVICEID_H
