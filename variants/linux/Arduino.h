// Minimal Arduino compatibility layer for the Linux native build.
// Shadows the real Arduino.h so that shared MeshCore code compiles without
// the Arduino framework dependency.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// GPIO / SPI constants
// ---------------------------------------------------------------------------
#define HIGH 1
#define LOW  0
#define INPUT          0
#define OUTPUT         1
#define INPUT_PULLUP   2
#define INPUT_PULLDOWN 3
#define RISING  1
#define FALLING 2
#define CHANGE  3

#define MSBFIRST 1
#define LSBFIRST 0
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------
#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define PROGMEM
#define constrain(amt, low, high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
static inline uint32_t millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

static inline uint32_t micros() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000000UL + ts.tv_nsec / 1000UL);
}

static inline void delay(uint32_t ms) {
    usleep((useconds_t)ms * 1000);
}

static inline void delayMicroseconds(uint32_t us) {
    usleep((useconds_t)us);
}

// ---------------------------------------------------------------------------
// GPIO stubs — real GPIO is handled by LinuxHal via libgpiod
// ---------------------------------------------------------------------------
static inline void pinMode(uint8_t, uint8_t) {}
static inline void digitalWrite(uint8_t, uint8_t) {}
static inline int  digitalRead(uint8_t) { return 0; }
static inline void attachInterrupt(uint8_t, void(*)(), uint8_t) {}
static inline void detachInterrupt(uint8_t) {}
static inline void yield() { usleep(1); }

#ifdef __cplusplus

// ---------------------------------------------------------------------------
// Integer type aliases (C++ only)
// ---------------------------------------------------------------------------
using byte   = uint8_t;
using word   = uint16_t;

// Bring std::min / std::max / std::abs into global scope.
#include <algorithm>
#include <cstdlib>
using std::min;
using std::max;
using std::abs;

template<typename T>
static inline T sq(T x) { return x * x; }

// ---------------------------------------------------------------------------
// Arduino random() / randomSeed() — backed by POSIX srand/rand
// ---------------------------------------------------------------------------
static inline void randomSeed(long seed) { srand((unsigned int)seed); }
static inline long random(long maxVal)          { return maxVal > 0 ? (rand() % maxVal) : 0; }
static inline long random(long minVal, long maxVal) {
    return minVal + (maxVal > minVal ? (rand() % (maxVal - minVal)) : 0);
}

// ---------------------------------------------------------------------------
// Print / Stream base classes
// ---------------------------------------------------------------------------
class Print {
public:
    virtual size_t write(uint8_t c) = 0;

    virtual size_t write(const uint8_t* buf, size_t n) {
        size_t w = 0;
        while (w < n) { if (!write(buf[w++])) break; }
        return w;
    }

    size_t write(const char* s) {
        return s ? write((const uint8_t*)s, strlen(s)) : 0;
    }

    size_t print(const char* s)       { return write(s); }
    size_t print(char c)              { return write((uint8_t)c); }
    size_t print(int n)               { char b[32]; snprintf(b,sizeof(b),"%d",n); return write(b); }
    size_t print(unsigned int n)      { char b[32]; snprintf(b,sizeof(b),"%u",n); return write(b); }
    size_t print(long n)              { char b[32]; snprintf(b,sizeof(b),"%ld",n); return write(b); }
    size_t print(unsigned long n)     { char b[32]; snprintf(b,sizeof(b),"%lu",n); return write(b); }
    size_t print(double f, int d = 2) { char b[32]; snprintf(b,sizeof(b),"%.*f",d,f); return write(b); }

    size_t println()                  { return write("\r\n"); }
    size_t println(const char* s)     { return print(s) + println(); }
    size_t println(char c)            { return print(c) + println(); }
    size_t println(int n)             { return print(n) + println(); }
    size_t println(unsigned int n)    { return print(n) + println(); }
    size_t println(long n)            { return print(n) + println(); }
    size_t println(unsigned long n)   { return print(n) + println(); }
    size_t println(double f, int d=2) { return print(f,d) + println(); }

    int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n > 0) write((const uint8_t*)buf, (size_t)(n < (int)sizeof(buf) ? n : sizeof(buf)-1));
        return n;
    }
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read()      = 0;
    virtual int peek()      = 0;
    virtual void flush()    {}

    size_t readBytes(uint8_t* buf, size_t len) {
        size_t n = 0;
        while (n < len) {
            int c = read();
            if (c < 0) break;
            buf[n++] = (uint8_t)c;
        }
        return n;
    }
    size_t readBytes(char* buf, size_t len) {
        return readBytes((uint8_t*)buf, len);
    }
};

// ---------------------------------------------------------------------------
// HardwareSerial — backed by stdout / stdin
// ---------------------------------------------------------------------------
#include <sys/time.h>
#include <sys/select.h>

class HardwareSerial : public Stream {
public:
    void begin(unsigned long /*baud*/) {}
    void end() {}

    size_t write(uint8_t c) override {
        putchar((int)c);
        fflush(stdout);
        return 1;
    }

    size_t write(const uint8_t* buf, size_t n) override {
        size_t w = fwrite(buf, 1, n, stdout);
        fflush(stdout);
        return w;
    }

    int available() override {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 0};
        return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0 ? 1 : 0;
    }

    int read() override  { return getchar(); }
    int peek() override  { return -1; }
};

extern HardwareSerial Serial;

#endif  // __cplusplus
