//
// Project-level logging macros.
//
// On Arduino (ESP32): output is routed through Log::emit(), which formats
// each line as "<ms> <level> <tag> <message>\r\n" on Serial. The macro layer
// undefs the framework's default ESP_LOG* and replaces them, so internal
// messages from libraries (Preferences, WiFi, etc.) also pick up this format.
//
// On native (test) builds: every ESP_LOG* expands to a no-op. No Serial
// reference, no esp_timer, no Arduino dependency.
//
// Usage:
//
//   static const char* TAG = "net";
//   ESP_LOGI(TAG, "Connected to %s", ssid);
//

#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <esp_timer.h>

namespace Log {
    // Single dispatch point for all log output. Today: Serial with
    // timestamp + level letter + tag. Tomorrow: also fan-out to a
    // syslog/file/etc. sink by adding a line inside this function.
    // `msg` is already formatted by the _LEDZ_LOG macro, so sinks
    // receive a `const char*` and never deal with varargs.
    void emit(char level, const char* tag, const char* msg);
}

// Undef framework defaults so our format wins.
#undef ESP_LOGE
#undef ESP_LOGW
#undef ESP_LOGI
#undef ESP_LOGD
#undef ESP_LOGV

#ifndef CORE_DEBUG_LEVEL
#define CORE_DEBUG_LEVEL 0
#endif

#define _LEDZ_LOG(letter, tag, format, ...) do {                 \
    char _log_buf[256];                                         \
    snprintf(_log_buf, sizeof(_log_buf), format, ##__VA_ARGS__); \
    Log::emit(#letter[0], tag, _log_buf);                       \
} while(0)

#define ESP_LOGE(tag, format, ...) _LEDZ_LOG(E, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) _LEDZ_LOG(W, tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) _LEDZ_LOG(I, tag, format, ##__VA_ARGS__)

#if CORE_DEBUG_LEVEL >= 4
#define ESP_LOGD(tag, format, ...) _LEDZ_LOG(D, tag, format, ##__VA_ARGS__)
#else
#define ESP_LOGD(tag, format, ...) do {} while(0)
#endif

#if CORE_DEBUG_LEVEL >= 5
#define ESP_LOGV(tag, format, ...) _LEDZ_LOG(V, tag, format, ##__VA_ARGS__)
#else
#define ESP_LOGV(tag, format, ...) do {} while(0)
#endif

// Keep esp_log_level_set callable but as a no-op — our macros do not use
// the ESP-IDF runtime log system.
#define esp_log_level_set(tag, level) (void)0

#else // !ARDUINO — native test build stubs

#define ESP_LOGE(tag, fmt, ...)
#define ESP_LOGW(tag, fmt, ...)
#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGD(tag, fmt, ...)
#define ESP_LOGV(tag, fmt, ...)
#define esp_log_level_set(tag, level) (void)0

#endif // ARDUINO
