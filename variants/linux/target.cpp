#include "target.h"
#include "LinuxHal.h"

LinuxBoard board;

// LinuxHal is created lazily in radio_init() once board.config is loaded.
static LinuxHal* hal = nullptr;

RADIO_CLASS radio = new Module(hal,
                               RADIOLIB_NC, RADIOLIB_NC,
                               RADIOLIB_NC, RADIOLIB_NC);
WRAPPER_CLASS radio_driver(radio, board);

LinuxRTCClock rtc_clock;
EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
#endif

bool radio_init() {
    rtc_clock.begin();

    hal = new LinuxHal(board.config.spidev, "gpiochip0", 2000000);
    hal->init();
    hal->spiBegin();

    radio = new Module(hal,
                       board.config.lora_nss_pin,
                       board.config.lora_irq_pin,
                       board.config.lora_reset_pin,
                       board.config.lora_busy_pin);
    return radio.std_init(nullptr);
}

uint32_t radio_get_rng_seed() {
    return radio.random(0x7FFFFFFF);
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
    radio.setFrequency(freq);
    radio.setSpreadingFactor(sf);
    radio.setBandwidth(bw);
    radio.setCodingRate(cr);
}

void radio_set_tx_power(uint8_t dbm) {
    radio.setOutputPower(dbm);
}

mesh::LocalIdentity radio_new_identity() {
    RadioNoiseListener rng(radio);
    return mesh::LocalIdentity(&rng);
}
