// Minimal RTClib stub for the Linux native build.
// Provides only DateTime, which CommonCLI uses for timestamp formatting.
// Shadows the real Adafruit RTClib header (which requires Wire/Arduino).
#pragma once

#include <stdint.h>
#include <time.h>

class DateTime {
    uint32_t _t;
public:
    explicit DateTime(uint32_t t = 0) : _t(t) {}

    uint16_t year()   const { return _tm().tm_year + 1900; }
    uint8_t  month()  const { return (uint8_t)(_tm().tm_mon + 1); }
    uint8_t  day()    const { return (uint8_t)_tm().tm_mday; }
    uint8_t  hour()   const { return (uint8_t)_tm().tm_hour; }
    uint8_t  minute() const { return (uint8_t)_tm().tm_min; }
    uint8_t  second() const { return (uint8_t)_tm().tm_sec; }
    uint32_t unixtime() const { return _t; }

private:
    struct tm _tm() const {
        time_t t = (time_t)_t;
        struct tm r;
        gmtime_r(&t, &r);
        return r;
    }
};
