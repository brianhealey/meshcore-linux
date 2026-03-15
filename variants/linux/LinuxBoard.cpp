#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "LinuxBoard.h"

void LinuxBoard::begin() {
  config.load("/etc/meshcored/meshcored.ini");

  Serial.printf("data_dir  : %s\n", config.data_dir);
  Serial.printf("spidev    : %s\n", config.spidev);
  Serial.printf("gpiochip  : %s\n", config.gpiochip);
  Serial.printf("LoRa pins : NSS=%d BUSY=%d IRQ=%d RESET=%d RXEN=%d TXEN=%d\n",
                (int)config.lora_nss_pin,
                (int)config.lora_busy_pin,
                (int)config.lora_irq_pin,
                (int)config.lora_reset_pin,
                (int)config.lora_rxen_pin,
                (int)config.lora_txen_pin);
  Serial.printf("LoRa      : freq=%.3f bw=%.1f sf=%d cr=%d pwr=%d\n",
                config.lora_freq, config.lora_bw,
                (int)config.lora_sf, (int)config.lora_cr,
                (int)config.lora_tx_power);
}

// ---------------------------------------------------------------------------
// INI parser helpers
// ---------------------------------------------------------------------------

static void trim(char *str) {
  char *end;
  while (isspace((unsigned char)*str)) str++;
  if (*str == 0) { *str = 0; return; }
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
}

static void safe_copy(char *dst, const char *src, size_t maxlen) {
  strncpy(dst, src, maxlen - 1);
  dst[maxlen - 1] = '\0';
}

int LinuxConfig::load(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "LinuxConfig: %s not found, using defaults\n", filename);
    return 0;
  }

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (isspace(*p)) p++;
    if (*p == '\0' || *p == '#' || *p == ';') continue;

    char *key = p;
    while (*p && !isspace(*p) && *p != '=') p++;
    if (*p == '\0') continue;
    *p++ = '\0';

    while (*p && (isspace(*p) || *p == '=')) p++;
    char *value = p;
    p = value;
    while (*p && *p != '\n' && *p != '\r' && *p != '#' && *p != ';') p++;
    *p = '\0';

    trim(key);
    trim(value);

    // strip optional surrounding quotes from string values
    {
      size_t vlen = strlen(value);
      if (vlen >= 2 && (value[0] == '"' || value[0] == '\'') && value[vlen-1] == value[0]) {
        value[vlen-1] = '\0';
        value++;
      }
    }

    if      (strcmp(key, "spidev") == 0)            safe_copy(spidev, value, sizeof(spidev));
    else if (strcmp(key, "gpiochip") == 0)          safe_copy(gpiochip, value, sizeof(gpiochip));
    else if (strcmp(key, "lora_freq") == 0)         lora_freq = atof(value);
    else if (strcmp(key, "lora_bw") == 0)           lora_bw = atof(value);
    else if (strcmp(key, "lora_sf") == 0)           lora_sf = (uint8_t)atoi(value);
    else if (strcmp(key, "lora_cr") == 0)           lora_cr = (uint8_t)atoi(value);
    else if (strcmp(key, "lora_tcxo") == 0)         lora_tcxo = atof(value);
    else if (strcmp(key, "lora_tx_power") == 0)     lora_tx_power = atoi(value);
    else if (strcmp(key, "current_limit") == 0)     current_limit = atof(value);
    else if (strcmp(key, "dio2_as_rf_switch") == 0) dio2_as_rf_switch = (atoi(value) != 0);
    else if (strcmp(key, "rx_boosted_gain") == 0)   rx_boosted_gain   = (atoi(value) != 0);
    else if (strcmp(key, "lora_irq_pin") == 0)      lora_irq_pin = atoi(value);
    else if (strcmp(key, "lora_reset_pin") == 0)    lora_reset_pin = atoi(value);
    else if (strcmp(key, "lora_nss_pin") == 0)      lora_nss_pin = atoi(value);
    else if (strcmp(key, "lora_busy_pin") == 0)     lora_busy_pin = atoi(value);
    else if (strcmp(key, "lora_rxen_pin") == 0)     lora_rxen_pin = atoi(value);
    else if (strcmp(key, "lora_txen_pin") == 0)     lora_txen_pin = atoi(value);
    else if (strcmp(key, "advert_name") == 0)       safe_copy(advert_name, value, sizeof(advert_name));
    else if (strcmp(key, "admin_password") == 0)    safe_copy(admin_password, value, sizeof(admin_password));
    else if (strcmp(key, "lat") == 0)               lat = atof(value);
    else if (strcmp(key, "lon") == 0)               lon = atof(value);
    else if (strcmp(key, "data_dir") == 0)          safe_copy(data_dir, value, sizeof(data_dir));
  }
  fclose(f);
  return 0;
}
