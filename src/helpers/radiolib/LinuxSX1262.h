#pragma once

#include <RadioLib.h>

#define SX126X_IRQ_HEADER_VALID                     0b0000010000  //  4     4     valid LoRa header received
#define SX126X_IRQ_PREAMBLE_DETECTED           0x04
#define SX126X_PREAMBLE_LENGTH 16

extern LinuxBoard board;

class LinuxSX1262 : public SX1262 {
  public:
    uint8_t currentSF = 0;  // tracks the active spreading factor; 0 until std_init()

    LinuxSX1262(Module *mod) : SX1262(mod) { }

    // Shadow SX1262::setSpreadingFactor to keep currentSF in sync.
    // Called via radio.setSpreadingFactor() in target.cpp (concrete type, not base ptr).
    int16_t setSpreadingFactor(uint8_t sf) {
      currentSF = sf;
      return SX1262::setSpreadingFactor(sf);
    }

    bool std_init(void* /*spi*/ = nullptr)
    {
      LinuxConfig config = board.config;

      Serial.printf("Radio begin %f %f %d %d %f\n", config.lora_freq, config.lora_bw, config.lora_sf, config.lora_cr, config.lora_tcxo);
      int status = begin(config.lora_freq, config.lora_bw, config.lora_sf, config.lora_cr, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, config.lora_tx_power, SX126X_PREAMBLE_LENGTH, config.lora_tcxo);
      // if radio init fails with -707/-706, try again with tcxo voltage set to 0.0f
      if (status == RADIOLIB_ERR_SPI_CMD_FAILED || status == RADIOLIB_ERR_SPI_CMD_INVALID) {
        status = begin(config.lora_freq, config.lora_bw, config.lora_sf, config.lora_cr, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, config.lora_tx_power, SX126X_PREAMBLE_LENGTH, 0.0f);
      }
      if (status != RADIOLIB_ERR_NONE) {
        Serial.print("ERROR: radio init failed: ");
        Serial.println(status);
        return false;  // fail
      }

      currentSF = config.lora_sf;
      setCRC(1);

      setCurrentLimit(config.current_limit);
      setDio2AsRfSwitch(config.dio2_as_rf_switch);
      setRxBoostedGainMode(config.rx_boosted_gain);
      if (config.lora_rxen_pin != RADIOLIB_NC || config.lora_txen_pin != RADIOLIB_NC) {
        setRfSwitchPins(config.lora_rxen_pin, config.lora_txen_pin);
      }

      return true;
    }

    bool isReceiving() {
      uint16_t irq = getIrqFlags();
      bool detected = (irq & SX126X_IRQ_HEADER_VALID) || (irq & SX126X_IRQ_PREAMBLE_DETECTED);
      return detected;
    }
};
