//test2:
#include <Arduino.h>
#include "driver/i2s.h"
#include "esp_task_wdt.h"

#define SAMPLE_RATE 16000
#define BUF_SIZE 1024
int16_t audio_buffer[BUF_SIZE];

void setup() {
  Serial.begin(115200);
  while(!Serial); 
  Serial.println("Test I2S Start");

  i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 512,
      .use_apll = false,
  };
  i2s_pin_config_t pin_cfg = { .bck_io_num = 9, .ws_io_num = 8, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = 7 };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_cfg);
}

void loop() {
  size_t readBytes;
  i2s_read(I2S_NUM_0, audio_buffer, sizeof(audio_buffer), &readBytes, portMAX_DELAY);
  Serial.println("Audio chunk read");
  Serial.print("Free heap: "); Serial.println(ESP.getFreeHeap());

  delay(500);
}
