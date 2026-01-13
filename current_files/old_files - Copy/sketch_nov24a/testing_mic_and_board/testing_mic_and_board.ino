#include <Arduino.h>
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 5
#define BUFFER_SIZE 1024

#define PDM_CLK 42
#define PDM_DATA 41

i2s_chan_handle_t rx_chan;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("ESP32-S3 READY");

  // --- Create I2S RX channel ---
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_chan);

  // --- PDM RX config ---
i2s_pdm_rx_config_t pdm_cfg = {
  .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
  .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT,
      I2S_SLOT_MODE_MONO
  ),
  .gpio_cfg = {
      .clk = GPIO_NUM_42,
      .din = GPIO_NUM_41,
      .invert_flags = {
          .clk_inv = false,
      },
  },
};


  i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_cfg);
  i2s_channel_enable(rx_chan);
}

void loop() {
  while (!Serial.available()) delay(10);
  if (Serial.readStringUntil('\n') != "rec") return;

  uint8_t buffer[BUFFER_SIZE];
  size_t bytes_read;

  size_t total_bytes = SAMPLE_RATE * RECORD_SECONDS * 2;
  size_t sent = 0;

  while (sent < total_bytes) {
    i2s_channel_read(rx_chan, buffer, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
    Serial.write(buffer, bytes_read);
    sent += bytes_read;
  }
}
