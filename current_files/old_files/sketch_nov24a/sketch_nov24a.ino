#include <Arduino.h>
#include "driver/i2s.h"

// TensorFlow Lite Micro for ESP32
#include <TensorFlowLite_ESP32.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

// MFCC feature extraction
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/micro_features_generator.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/micro_model_settings.h"  // contains kFeatureElementCount

// ================= AUDIO CONFIG ====================
#define SAMPLE_RATE 16000


int16_t audio_buffer[SAMPLE_BUFFER_SIZE];

// ============== I2S MICROPHONE SETUP ===============
void setupI2S() {
  i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,    // reduced
      .dma_buf_len = 256,    // reduced
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
  };

  i2s_pin_config_t pin_cfg = {
      .bck_io_num = 9,
      .ws_io_num = 8,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = 7
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_cfg);
}

// ================= TFLITE MODEL ====================
#include "model_data.h"   // Your TFLite model

constexpr int tensorArenaSize = 100 * 1024;  // 100 KB
uint8_t tensorArena[tensorArenaSize];

tflite::MicroInterpreter* interpreter;
TfLiteTensor* input;
TfLiteTensor* output;

// Optional: required by tflite_esp32 library
tflite::ErrorReporter* error_reporter = nullptr;
tflite::MicroResourceVariables* resource_variables = nullptr;
tflite::MicroProfiler* profiler = nullptr;

void setupModel() {
  const tflite::Model* model = tflite::GetModel(model_data);
  static tflite::AllOpsResolver resolver;

  static tflite::MicroInterpreter static_interpreter(
      model,
      resolver,
      tensorArena,
      tensorArenaSize,
      error_reporter,
      resource_variables,
      profiler
  );

  interpreter = &static_interpreter;
  interpreter->AllocateTensors();

  input = interpreter->input(0);
  output = interpreter->output(0);
}

// ================= CAPTURE AUDIO ====================
void recordAudio() {
  size_t readBytes = 0;
  i2s_read(I2S_NUM_0, audio_buffer, sizeof(audio_buffer),
           &readBytes, portMAX_DELAY);
}

// ================= RUN INFERENCE ====================
int runInference() {
  static int8_t mfcc_out[kFeatureElementCount];
  size_t mfcc_size = kFeatureElementCount;  // must pass pointer to size

  if (GenerateMicroFeatures(
          error_reporter,     // first argument
          audio_buffer,       // audio data
          SAMPLE_BUFFER_SIZE, // number of samples
          SAMPLE_RATE,        // sample rate
          mfcc_out,           // output array
          &mfcc_size          // pointer to output size
      ) != kTfLiteOk) {
    Serial.println("MFCC generation failed!");
    return -1;
  }

  // Copy features to model input
  memcpy(input->data.int8, mfcc_out, input->bytes);

  // Run inference
  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("Invoke failed!");
    return -1;
  }

  int8_t raw_output = output->data.int8[0];
  return (raw_output > 0) ? 1 : 0;
}

// ================= ARDUINO SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1500);

  setupI2S();
  setupModel();

  Serial.println("Emotion Detection Ready ✔");
}

// ================= MAIN LOOP =====================
void loop() {
  Serial.println("Recording...");
  recordAudio();

  int emotion = runInference();

  if (emotion == 1) Serial.println("Emergency!");
  else if (emotion == 0) Serial.println("Neutral");
  else Serial.println("Inference Error");

  delay(500);    // allow CPU to breathe
  yield();       // background tasks
}
