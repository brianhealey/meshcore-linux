// RadioLib hardware abstraction layer for Linux.
// Implements RadioLibHal using /dev/spidev (SPI) and libgpiod v2 (GPIO).
// Used in place of RadioLib's ArduinoHal.
#pragma once

#include <RadioLib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

// ---------------------------------------------------------------------------
// Per-pin IRQ state (libgpiod v2)
// ---------------------------------------------------------------------------
struct LinuxIrqEntry {
    struct gpiod_line_request* req = nullptr;
    void (*callback)()             = nullptr;
    pthread_t thread               = 0;
    volatile bool running          = false;
};

static void* irq_thread_fn(void* arg) {
    LinuxIrqEntry* e = (LinuxIrqEntry*)arg;
    struct gpiod_edge_event_buffer* buf = gpiod_edge_event_buffer_new(1);
    while (e->running) {
        // 10 ms timeout (in nanoseconds)
        int ret = gpiod_line_request_wait_edge_events(e->req, 10000000LL);
        if (ret == 1) {
            if (gpiod_line_request_read_edge_events(e->req, buf, 1) > 0) {
                struct gpiod_edge_event* evt =
                    gpiod_edge_event_buffer_get_event(buf, 0);
                if (gpiod_edge_event_get_event_type(evt) ==
                        GPIOD_EDGE_EVENT_RISING_EDGE) {
                    if (e->callback) e->callback();
                }
            }
        }
    }
    gpiod_edge_event_buffer_free(buf);
    return nullptr;
}

// ---------------------------------------------------------------------------
// LinuxHal
// ---------------------------------------------------------------------------
class LinuxHal : public RadioLibHal {
public:
    // spidev_path: e.g. "/dev/spidev0.0"
    // chip_path:   gpiochip device path, e.g. "/dev/gpiochip0"
    // speed_hz:    SPI clock frequency
    explicit LinuxHal(const char* spidev_path,
                      const char* chip_path = "/dev/gpiochip0",
                      uint32_t    speed_hz  = 2000000)
        : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
          _spi_fd(-1), _speed(speed_hz), _spi_path(spidev_path),
          _chip(nullptr), _chip_path(chip_path) {}

    ~LinuxHal() { term(); }

    // -------------------------------------------------------------------
    // Init / term
    // -------------------------------------------------------------------
    void init() override {
        _chip = gpiod_chip_open(_chip_path);
        if (!_chip) {
            fprintf(stderr, "LinuxHal: cannot open %s: %s\n",
                    _chip_path, strerror(errno));
        }
    }

    void term() override {
        // stop all interrupt threads
        for (int i = 0; i < _nirq; i++) {
            _irq[i].running = false;
            pthread_join(_irq[i].thread, nullptr);
            if (_irq[i].req) {
                gpiod_line_request_release(_irq[i].req);
                _irq[i].req = nullptr;
            }
        }
        _nirq = 0;

        // release output/input requests
        for (int i = 0; i < _nreq; i++) {
            if (_reqs[i]) { gpiod_line_request_release(_reqs[i]); _reqs[i] = nullptr; }
        }
        _nreq = 0;

        if (_spi_fd >= 0) { close(_spi_fd); _spi_fd = -1; }
        if (_chip)        { gpiod_chip_close(_chip); _chip = nullptr; }
    }

    // -------------------------------------------------------------------
    // SPI
    // -------------------------------------------------------------------
    void spiBegin() override {
        _spi_fd = open(_spi_path, O_RDWR);
        if (_spi_fd < 0) {
            fprintf(stderr, "LinuxHal: cannot open %s: %s\n",
                    _spi_path, strerror(errno));
            return;
        }
        uint8_t mode  = SPI_MODE_0;
        uint8_t bits  = 8;
        uint32_t spd  = _speed;
        ioctl(_spi_fd, SPI_IOC_WR_MODE,          &mode);
        ioctl(_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        ioctl(_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ,  &spd);
    }

    void spiBeginTransaction() override {}
    void spiEndTransaction()   override {}

    void spiEnd() override {
        if (_spi_fd >= 0) { close(_spi_fd); _spi_fd = -1; }
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
        struct spi_ioc_transfer tr = {};
        tr.tx_buf        = (unsigned long)out;
        tr.rx_buf        = (unsigned long)in;
        tr.len           = (uint32_t)len;
        tr.speed_hz      = _speed;
        tr.bits_per_word = 8;
        ioctl(_spi_fd, SPI_IOC_MESSAGE(1), &tr);
    }

    // -------------------------------------------------------------------
    // GPIO
    // -------------------------------------------------------------------
    void pinMode(uint32_t pin, uint32_t mode) override {
        if (!_chip || pin == RADIOLIB_NC || _nreq >= MAX_REQ) return;

        struct gpiod_line_settings* settings = gpiod_line_settings_new();
        if (!settings) return;

        if (mode == OUTPUT) {
            gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
            gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);
        } else {
            gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
        }

        struct gpiod_line_config* line_cfg = gpiod_line_config_new();
        unsigned int offset = (unsigned int)pin;
        gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);

        struct gpiod_request_config* req_cfg = gpiod_request_config_new();
        gpiod_request_config_set_consumer(req_cfg, "meshcored");

        struct gpiod_line_request* req =
            gpiod_chip_request_lines(_chip, req_cfg, line_cfg);

        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);

        if (!req) {
            fprintf(stderr, "LinuxHal: pinMode pin %u: %s\n", pin, strerror(errno));
            return;
        }

        _reqs[_nreq]     = req;
        _req_pins[_nreq] = pin;
        _nreq++;
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        struct gpiod_line_request* req = findReq(pin);
        if (!req) return;
        enum gpiod_line_value v = value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
        gpiod_line_request_set_value(req, (unsigned int)pin, v);
    }

    uint32_t digitalRead(uint32_t pin) override {
        struct gpiod_line_request* req = findReq(pin);
        if (!req) return 0;
        enum gpiod_line_value v = gpiod_line_request_get_value(req, (unsigned int)pin);
        return (v == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
    }

    void attachInterrupt(uint32_t pin, void (*cb)(), uint32_t /*mode*/) override {
        if (!_chip || pin == RADIOLIB_NC || _nirq >= MAX_IRQ) return;

        // RadioLib calls pinMode(irq, INPUT) then attachInterrupt(irq, ...).
        // Release any existing plain-input request on this pin first so we
        // don't get EBUSY when requesting it again with edge detection.
        for (int i = 0; i < _nreq; i++) {
            if (_req_pins[i] == pin && _reqs[i]) {
                gpiod_line_request_release(_reqs[i]);
                _reqs[i] = nullptr;
                for (int j = i; j < _nreq - 1; j++) {
                    _reqs[j]     = _reqs[j+1];
                    _req_pins[j] = _req_pins[j+1];
                }
                _nreq--;
                break;
            }
        }

        struct gpiod_line_settings* settings = gpiod_line_settings_new();
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
        gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_RISING);

        struct gpiod_line_config* line_cfg = gpiod_line_config_new();
        unsigned int offset = (unsigned int)pin;
        gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);

        struct gpiod_request_config* req_cfg = gpiod_request_config_new();
        gpiod_request_config_set_consumer(req_cfg, "meshcored-irq");

        struct gpiod_line_request* req =
            gpiod_chip_request_lines(_chip, req_cfg, line_cfg);

        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);

        if (!req) {
            fprintf(stderr, "LinuxHal: attachInterrupt pin %u: %s\n",
                    pin, strerror(errno));
            return;
        }

        LinuxIrqEntry* e = &_irq[_nirq++];
        e->req      = req;
        e->callback = cb;
        e->running  = true;
        pthread_create(&e->thread, nullptr, irq_thread_fn, e);
    }

    void detachInterrupt(uint32_t pin) override {
        for (int i = 0; i < _nirq; i++) {
            if (!_irq[i].req) continue;
            // Each IRQ request covers exactly one pin; check it directly.
            unsigned int req_offset = 0;
            gpiod_line_request_get_requested_offsets(_irq[i].req, &req_offset, 1);
            if (req_offset != (unsigned int)pin) continue;

            _irq[i].running = false;
            pthread_join(_irq[i].thread, nullptr);
            gpiod_line_request_release(_irq[i].req);
            _irq[i].req = nullptr;
            for (int j = i; j < _nirq - 1; j++) _irq[j] = _irq[j+1];
            _nirq--;
            return;
        }
    }

    // -------------------------------------------------------------------
    // Timing
    // -------------------------------------------------------------------
    void          delay(unsigned long ms)  override { usleep((useconds_t)ms * 1000); }
    void          delayMicroseconds(unsigned long us) override { usleep((useconds_t)us); }
    unsigned long millis() override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
    }
    unsigned long micros() override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (unsigned long)(ts.tv_sec * 1000000UL + ts.tv_nsec / 1000UL);
    }
    long pulseIn(uint32_t, uint32_t, unsigned long) override { return 0; }
    void yield() override { usleep(1); }

    // -------------------------------------------------------------------
    // Unsupported
    // -------------------------------------------------------------------
    void tone(uint32_t, unsigned int, unsigned long) override {}
    void noTone(uint32_t) override {}

private:
    static constexpr int MAX_IRQ = 4;
    static constexpr int MAX_REQ = 16;

    int         _spi_fd;
    uint32_t    _speed;
    const char* _spi_path;

    struct gpiod_chip* _chip;
    const char*        _chip_path;

    LinuxIrqEntry  _irq[MAX_IRQ] = {};
    int            _nirq = 0;

    // general (non-IRQ) line requests indexed by pin
    struct gpiod_line_request* _reqs[MAX_REQ] = {};
    uint32_t                   _req_pins[MAX_REQ] = {};
    int                        _nreq = 0;

    struct gpiod_line_request* findReq(uint32_t pin) {
        for (int i = 0; i < _nreq; i++) {
            if (_req_pins[i] == pin) return _reqs[i];
        }
        return nullptr;
    }
};
