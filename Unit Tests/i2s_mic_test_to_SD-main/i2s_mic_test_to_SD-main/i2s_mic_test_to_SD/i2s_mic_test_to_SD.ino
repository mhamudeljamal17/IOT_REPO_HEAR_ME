#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"

#define SAMPLE_RATE     16000
#define RECORD_SECONDS  5
#define WAV_FILE        "/record.wav"

#define PDM_CLK  GPIO_NUM_42
#define PDM_DATA GPIO_NUM_41

i2s_chan_handle_t rx_chan;

/* ===== WAV HEADER ===== */
void writeWavHeader(File &file, uint32_t dataSize) {
  uint32_t sampleRate = SAMPLE_RATE;
  uint16_t bitsPerSample = 16;
  uint16_t numChannels = 1;
  uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
  uint16_t blockAlign = numChannels * bitsPerSample / 8;
  uint32_t chunkSize = dataSize + 36;
  uint16_t audioFormat = 1;
  uint32_t subchunk1Size = 16;

  const uint8_t riff[] = {'R','I','F','F'};
  const uint8_t wave[] = {'W','A','V','E'};
  const uint8_t fmt[]  = {'f','m','t',' '};
  const uint8_t data[] = {'d','a','t','a'};

  file.write(riff, 4);
  file.write((uint8_t*)&chunkSize, 4);
  file.write(wave, 4);

  file.write(fmt, 4);
  file.write((uint8_t*)&subchunk1Size, 4);
  file.write((uint8_t*)&audioFormat, 2);
  file.write((uint8_t*)&numChannels, 2);
  file.write((uint8_t*)&sampleRate, 4);
  file.write((uint8_t*)&byteRate, 4);
  file.write((uint8_t*)&blockAlign, 2);
  file.write((uint8_t*)&bitsPerSample, 2);

  file.write(data, 4);
  file.write((uint8_t*)&dataSize, 4);
}
#define SD_CS 21

void setup() {
   Serial.begin(115200);
  delay(2000);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
    while (1);
  }

  Serial.println("SD init OK");

  /* ==== I2S PDM ==== */
  i2s_chan_config_t chan_cfg =
    I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_chan);

  i2s_pdm_rx_config_t pdm_cfg = {
    .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT,
      I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .clk = PDM_CLK,
      .din = PDM_DATA,
      .invert_flags = { .clk_inv = false },
    },
  };

  i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_cfg);
  i2s_channel_enable(rx_chan);

  Serial.println("Type 'rec' to record");
}

void loop() {
  if (!Serial.available()) return;
  if (Serial.readStringUntil('\n') != "rec") return;

  File wav = SD.open(WAV_FILE, FILE_WRITE);
  if (!wav) {
    Serial.println("Failed to open WAV");
    return;
  }

  uint32_t totalBytes = SAMPLE_RATE * RECORD_SECONDS * 2;
  writeWavHeader(wav, totalBytes);

  uint8_t buffer[1024];
  size_t bytesRead;
  uint32_t written = 0;

  while (written < totalBytes) {
    i2s_channel_read(rx_chan, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    wav.write(buffer, bytesRead);
    written += bytesRead;
  }

  wav.close();
  Serial.println("Saved /record.wav");
}
