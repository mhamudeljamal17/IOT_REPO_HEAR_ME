#include "audio_capture.h"
#include "config.h"
#include <I2S.h>

// XIAO ESP32S3 Sense onboard mic uses PDM via I2S, pins commonly WS=42, DATA=41. :contentReference[oaicite:7]{index=7}
static constexpr int kPdmWsPin   = 42; // word select / LRCLK
static constexpr int kPdmDataPin = 41; // data in

bool audio_init() {
  // Arduino I2S library (Espressif core). Seeed’s example uses PDM_MONO_MODE. :contentReference[oaicite:8]{index=8}
  I2S.setAllPins(-1, kPdmWsPin, kPdmDataPin, -1, -1);

  // PDM mono, 16-bit samples; we later convert to float.
  if (!I2S.begin(PDM_MONO_MODE, kSampleRate, 16)) {
    Serial.println("I2S begin failed");
    return false;
  }
  return true;
}

bool audio_record_2s(float* out_pcm_f32, int n_samples) {
  // Reads 16-bit signed samples from PDM mic and stores normalized float [-1,1].
  int i = 0;
  while (i < n_samples) {
    int16_t s = 0;
    int bytes = I2S.read((void*)&s, sizeof(s));
    if (bytes == sizeof(s)) {
      out_pcm_f32[i++] = (float)s / 32768.0f;
    }
    // If bytes != 2, just retry; mic stream sometimes yields partial reads.
  }
  return true;
}
