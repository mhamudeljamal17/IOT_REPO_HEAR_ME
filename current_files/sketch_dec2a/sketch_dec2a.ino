#include <TensorFlowLite_ESP32.h>



/*#include <TensorFlowLite_ESP32.h>

#include <Arduino.h>
#include "driver/i2s.h"
#include "model_data.h"  // Contains emotion_model_tflite[] and length

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// -----------------
// Microphone / Audio settings
// -----------------
#define SAMPLE_RATE        16000
#define AUDIO_BUFFER_SIZE  16000   // 1 second @ 16kHz

// XIAO ESP32S3 I2S PDM pins (adjust if needed)
#define I2S_DATA_PIN       41
#define I2S_CLK_PIN        42

int16_t audio_buffer[AUDIO_BUFFER_SIZE];

// -----------------
// Feature settings (must match your model input!)
// -----------------
#define MFCC_COEFFS        20
#define MFCC_FRAMES        64
#define kFeatureElementCount (MFCC_COEFFS * MFCC_FRAMES)
int8_t mfcc_buffer[kFeatureElementCount];

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

// -----------------
// TFLite Micro globals
// -----------------
const tflite::Model* model = nullptr;

// Adjust size if AllocateTensors fails or you extend the model
static uint8_t tensor_arena[120 * 1024] __attribute__((aligned(16)));

tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// Error reporter
static tflite::MicroErrorReporter micro_error_reporter;

// If your model output classes are: [neutral, emergency, other]
#define EMERGENCY_CLASS_INDEX  1

// -----------------
// Simple placeholder "MFCC"
// (just subsamples the waveform; NOT real MFCC!)
// -----------------
void computeMFCC(int16_t* audio, int8_t* mfcc_out) {
  for (int i = 0; i < kFeatureElementCount; i++) {
    int idx = i * (AUDIO_BUFFER_SIZE / kFeatureElementCount);
    if (idx >= AUDIO_BUFFER_SIZE) idx = AUDIO_BUFFER_SIZE - 1;
    mfcc_out[i] = audio[idx] >> 8; // int16 -> int8
  }
}

// -----------------
// Initialize I2S microphone
// -----------------
void initI2S() {
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
    .ws_io_num  = I2S_CLK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_DATA_PIN
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_cfg);
}

// -----------------
// Setup
// -----------------
void setup() {
  Serial.begin(115200);
  delay(500);

  // Init mic
  initI2S();
  Serial.println("I2S Microphone initialized.");

  // Load model
  model = tflite::GetModel(anger_mode_int8_tflite);
  if (!model) {
    Serial.println("ERROR: model pointer is null!");
    while (true) { delay(1000); }
  }

  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.print("ERROR: Model schema ");
    Serial.print(model->version());
    Serial.print(" != supported schema ");
    Serial.println(TFLITE_SCHEMA_VERSION);
    while (true) { delay(1000); }
  }
  Serial.println("Model loaded and schema OK.");

  // Resolver
  static tflite::MicroMutableOpResolver<10> resolver;
  resolver.AddConv2D();
  resolver.AddMaxPool2D();
  resolver.AddFullyConnected();
  resolver.AddReshape();
  resolver.AddSoftmax();
  resolver.AddLogistic();

  // Interpreter (static, no 'new')
  static tflite::MicroInterpreter static_interpreter(
      model,
      resolver,
      tensor_arena,
      sizeof(tensor_arena),
      &micro_error_reporter  // <-- Required by this library
  );
  interpreter = &static_interpreter;
  Serial.println("Interpreter object created.");

  // Allocate tensors
  TfLiteStatus alloc_status = interpreter->AllocateTensors();
  if (alloc_status != kTfLiteOk) {
    Serial.println("ERROR: AllocateTensors() failed!");
    while (true) { delay(1000); }
  }
  Serial.println("Tensors allocated OK.");

  input = interpreter->input(0);
  output = interpreter->output(0);

  if (!input || !output) {
    Serial.println("ERROR: input or output tensor is null!");
    while (true) { delay(1000); }
  }

  Serial.println("TFLite Micro interpreter ready.");
}

// -----------------
// Loop
// -----------------
void loop() {
  if (!interpreter) {
    Serial.println("Interpreter not initialized!");
    delay(1000);
    return;
  }

  // 1. Record 1 second of audio
  size_t bytes_read = 0;
  for (int i = 0; i < AUDIO_BUFFER_SIZE; i += 256) {
    i2s_read(I2S_NUM_0, audio_buffer + i, 256, &bytes_read, portMAX_DELAY);
  }

  // 2. Convert audio to features
  computeMFCC(audio_buffer, mfcc_buffer);

  // 3. Copy features into input tensor
  int feature_count = MIN(kFeatureElementCount, input->bytes);
  for (int i = 0; i < feature_count; i++) {
    input->data.int8[i] = mfcc_buffer[i];
  }
  for (int i = feature_count; i < input->bytes; i++) {
    input->data.int8[i] = 0;
  }

  // 4. Run inference
  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("Invoke failed!");
    delay(200);
    return;
  }

  // 5. Read all output scores
  int num_dims = output->dims->size;
  int num_classes = output->dims->data[num_dims - 1];

  Serial.print("Raw scores: ");
  for (int i = 0; i < num_classes; i++) {
    Serial.print(output->data.int8[i]);
    Serial.print(" ");
  }
  Serial.println();

  // 6. Argmax
  int best_idx = 0;
  int8_t best_score = output->data.int8[0];
  for (int i = 1; i < num_classes; i++) {
    int8_t s = output->data.int8[i];
    if (s > best_score) {
      best_score = s;
      best_idx = i;
    }
  }

  // 7. Map index → labels
  const char* LABELS_3[] = {"neutral", "emergency", "other"};

  Serial.print("Predicted class index: ");
  Serial.print(best_idx);
  Serial.print("  label: ");
  if (num_classes == 3) Serial.print(LABELS_3[best_idx]);
  else Serial.print("class_"); Serial.print(best_idx);
  Serial.print("  score: "); Serial.println(best_score);

  // 8. Decide emergency
  if (best_idx == EMERGENCY_CLASS_INDEX) {
    Serial.println("⚠  EMERGENCY DETECTED");
  } else {
    Serial.println("✓ Neutral");
  }

  Serial.println("------");
  delay(300);
}



*/
#include <Arduino.h>

/* ===================== PROJECT HEADERS ===================== */
#include "config.h"
#include "audio_capture.h"
#include "mfcc.h"
#include "model_data.h"

/* ===================== TFLITE MICRO ===================== */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"


/* ===================== AUDIO + MFCC ===================== */
static float g_pcm[kNumSamples];
static float g_mfcc[kNumMel][kNumFrames];

/* ===================== TFLITE GLOBALS ===================== */
constexpr int kTensorArenaSize = 60 * 1024;
static uint8_t tensor_arena[kTensorArenaSize];

tflite::MicroInterpreter* interpreter;
TfLiteTensor* input;
TfLiteTensor* output;

/* ===================== MODEL INIT ===================== */
void model_init() {
  static tflite::MicroErrorReporter error_reporter;

  const tflite::Model* model = tflite::GetModel(anger_mode_int8_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("❌ Model schema mismatch");
    while (1);
  }

static tflite::MicroMutableOpResolver<8> resolver; // increase template size to match total ops
resolver.AddConv2D();
resolver.AddDepthwiseConv2D();
resolver.AddFullyConnected();
resolver.AddReshape();
resolver.AddAveragePool2D();
resolver.AddMaxPool2D();
resolver.AddSoftmax();
resolver.AddLogistic();  // <-- add this


  static tflite::MicroInterpreter static_interpreter(
    model,
    resolver,
    tensor_arena,
    kTensorArenaSize,
    &error_reporter
  );

  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("❌ AllocateTensors failed");
    while (1);
  }

  input  = interpreter->input(0);
  output = interpreter->output(0);
  Serial.print("Input type: ");
Serial.println(input->type);

Serial.print("Output type: ");
Serial.println(output->type);

  Serial.println("✅ Model initialized");
}

/* ===================== CLASSIFY (OPTION A) ===================== */
const char* classify(float mfcc[kNumMel][kNumFrames]) {
  int idx = 0;

  const float input_scale = input->params.scale;
  const int input_zero_point = input->params.zero_point;

  // 🔥 Quantize MFCCs
  for (int m = 0; m < kNumMel; m++) {
    for (int t = 0; t < kNumFrames; t++) {
      int8_t q = (int8_t)(mfcc[m][t] / input_scale + input_zero_point);
      input->data.int8[idx++] = q;
    }
  }

  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("❌ Inference failed");
    return "Error";
  }

  // 🔥 Dequantize output
  const float output_scale = output->params.scale;
  const int output_zero_point = output->params.zero_point;

  int8_t raw = output->data.int8[0];
  float score = (raw - output_zero_point) * output_scale;

  Serial.print("Raw output int8: ");
  Serial.println(raw);

  Serial.print("Model score (dequantized): ");
  Serial.println(score, 6);

  if (score >= 0.5f)
    return "Angry";
  else
    return "Other";
}


/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- ESP32 Emotion Detection (OPTION A) ---");

  Serial.println("Initializing microphone...");
  if (!audio_init()) {
    Serial.println("❌ Mic init failed");
    while (1);
  }

  model_init();

  Serial.println("✅ System ready\n");
}

/* ===================== LOOP ===================== */
void loop() {
  Serial.println("Recording audio...");

  // 1️⃣ Record audio
  audio_record_2s(g_pcm, kNumSamples);

  // 2️⃣ Compute MFCCs
  compute_mfcc_20x63(g_pcm, g_mfcc);

  // 3️⃣ Debug small MFCC sample
  Serial.println("MFCC[0][0..4]:");
  for (int i = 0; i < 5; i++) {
    Serial.print(g_mfcc[0][i], 6);
    Serial.print(" ");
  }
  Serial.println();

  // 4️⃣ Classify
  const char* result = classify(g_mfcc);

  Serial.print("🎯 Detected emotion: ");
  Serial.println(result);
  Serial.println("----------------------------------");

  delay(300);
}





/*
#include <Arduino.h>
#include "driver/i2s.h"

#define SAMPLE_RATE 16000
#define I2S_DATA_PIN 41
#define I2S_CLK_PIN 42

void initI2S() {
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
    .ws_io_num  = I2S_CLK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_DATA_PIN
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_cfg);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  initI2S();
  Serial.println("I2S init done");
}

void loop() { }
*/





