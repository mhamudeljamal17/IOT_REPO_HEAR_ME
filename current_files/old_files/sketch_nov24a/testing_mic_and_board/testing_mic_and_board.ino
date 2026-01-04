


#include <Arduino.h>
#include "driver/i2s.h"

#define SAMPLE_RATE     16000
#define RECORD_SECONDS  5
#define BUFFER_SIZE     2048

#define I2S_DATA_PIN    41
#define I2S_CLK_PIN     42

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("ESP32-S3 READY");

  i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
  };

  i2s_pin_config_t pin_cfg = {
      .bck_io_num = I2S_PIN_NO_CHANGE,
      .ws_io_num = I2S_CLK_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_DATA_PIN
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_cfg);
}

void loop() {

  // Wait for 'rec' command from PC
  while (true) {
    Serial.println("Type 'rec' to start recording...");
    while (!Serial.available()) delay(10);

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("rec")) break;

    Serial.println("Invalid command, try again.");
    delay(1000);
  }

  Serial.println("Recording raw audio...");

  size_t total_samples = SAMPLE_RATE * RECORD_SECONDS;
  uint8_t buffer[BUFFER_SIZE];
  size_t bytes_read;

  // Stream raw PCM data to PC
  for (size_t i = 0; i < total_samples; i += (BUFFER_SIZE / 2)) {
    i2s_read(I2S_NUM_0, buffer, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
    Serial.write(buffer, bytes_read);
  }

  Serial.println("\nRecording complete!");
  delay(1000); // pause before next recording
}


