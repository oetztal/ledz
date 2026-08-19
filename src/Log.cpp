//
// Project-level logging dispatch.
//

#include "Log.h"

#ifdef ARDUINO

void Log::emit(char level, const char* tag, const char* msg) {
    Serial.printf("%08lu %c %6s %s\r\n",
                  (unsigned long)(esp_timer_get_time() / 1000ULL),
                  level, tag, msg);
    // Future sink hook: when syslog/file/MQTT sinks land, fan out here.
    // Example:
    //   if (SyslogOutput::isActive()) SyslogOutput::send(level, tag, msg);
}

#endif // ARDUINO
