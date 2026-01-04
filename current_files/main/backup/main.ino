#include <Arduino.h>
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/c/common.h"

#include "MFCC.h"
#include "NeuralNetwork.h"

/* ===================== CONFIG ===================== */

// Serial audio buffer
#define BUFLEN SHAPE_INPUT

// NN
static NeuralNetwork *nn = nullptr;
static String emotion_vec[] = {"NOT ANGRY", "ANGRY"};

// For timing
unsigned long t1, t2, t3, t4;

// ===================== FUNCTIONS =====================

// Normalize audio
void audio_normalized(int32_t *raw_signal, float *normalized_signal)
{
    for(int i=0; i<BUFLEN; i++)
        normalized_signal[i] = (float)raw_signal[i] / 32768.0f;
}

// Copy MFCC matrix to NN input buffer
void copy_mat_inputBuffer(float **matrix, float *inputBuffer) {
    int index = 0;
    for (int i = 0; i < MEL_BANDS; i++) {
        for (int j = 0; j < NUMBER_OF_WINDOWS; j++) {
            inputBuffer[index++] = matrix[i][j];
        }
    }
}

// // Interpret NN output
// int interprets_output(TfLiteTensor *outputTensor) {
    
    
// float score0 = outputTensor->data.f[0];
// float score1 = outputTensor->data.f[1];
// int result = (score0 > score1) ? 0 : 1;
// Serial.print("score0: "); Serial.println(score0);
// Serial.print("score1: "); Serial.println(score1);
// Serial.print("Predicted emotion: "); Serial.println(emotion_vec[result]);

//     return result;
// }
int interprets_output(TfLiteTensor *outputTensor) {
    float score0 = outputTensor->data.f[0]; // NOT ANGRY
    float score1 = outputTensor->data.f[1]; // ANGRY
    int result;

    const float THRESHOLD = 0.5f;

    // Use threshold
    if (score1 >= THRESHOLD) {
        result = 1; // ANGRY
    } else {
        result = 0; // NOT ANGRY
    }

    Serial.print("score0: "); Serial.println(score0);
    Serial.print("score1: "); Serial.println(score1);
    Serial.print("Predicted emotion: "); Serial.println(emotion_vec[result]);

    return result;
}

// ===================== SETUP =====================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n===============================");
    Serial.println("Angry Voice Detector - START");
    Serial.println("===============================\n");

    // Instantiate NN
    nn = new NeuralNetwork();
    if(!nn)
    {Serial.println("opsss , neural network is null!");}
    else{
    Serial.println("Neural Network Ready!");
    }
}

// ===================== LOOP =====================
void loop() {
    Serial.println("-------------------------------");
    Serial.println("Waiting for audio from PC (send BUFLEN samples)...");

    // ----------------- Wait for START command -----------------
    while (true) {
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            if (cmd == "START") break;
        }
        delay(10);
    }

    // ----------------- Allocate buffers -----------------
    int16_t *raw_signal = new int16_t[BUFLEN];       // store as int16_t
    float *inputAudio = new float[SHAPE_INPUT];
    Serial.printf("Free heap before MFCC: %u bytes\n", ESP.getFreeHeap());

    float **mfcc_mat = new float*[MEL_BANDS];
    for (int i = 0; i < MEL_BANDS; i++) mfcc_mat[i] = new float[NUMBER_OF_WINDOWS];

    memset(raw_signal, 0, BUFLEN * sizeof(int16_t));

    // ----------------- Read audio from Serial -----------------
    int bytes_read = 0;
    uint8_t low, high;
    int16_t sample16;

    while (bytes_read < BUFLEN * 2) {
        if (Serial.available() >= 2) {
            low  = Serial.read();
            high = Serial.read();
            sample16 = (int16_t)((high << 8) | low); // little-endian
            raw_signal[bytes_read / 2] = sample16;
            bytes_read += 2;
        } else {
            delay(1);
        }
    }

    // Discard any leftover END line
    if (Serial.available()) Serial.readStringUntil('\n');

    // ----------------- Debug: raw audio min/max -----------------
    int16_t raw_min = 32767, raw_max = -32768;
    for (int i = 0; i < BUFLEN; i++) {
        if (raw_signal[i] < raw_min) raw_min = raw_signal[i];
        if (raw_signal[i] > raw_max) raw_max = raw_signal[i];
    }
    Serial.printf("Raw audio range: min=%d max=%d\n", raw_min, raw_max);

    // ----------------- Normalize to [-1,1] -----------------
    for (int i = 0; i < BUFLEN; i++) {
        inputAudio[i] = raw_signal[i] / 32767.0f;
    }
    delete[] raw_signal;

    // ----------------- Debug: normalized audio min/max -----------------
    float norm_min = 1e6f, norm_max = -1e6f;
    for (int i = 0; i < BUFLEN; i++) {
        if (inputAudio[i] < norm_min) norm_min = inputAudio[i];
        if (inputAudio[i] > norm_max) norm_max = inputAudio[i];
    }
    Serial.printf("Normalized audio range: min=%f max=%f\n", norm_min, norm_max);

    // ----------------- Compute MFCC -----------------
    t1 = micros();
    mfccs(inputAudio, mfcc_mat);
    t2 = micros();

    delete[] inputAudio;

    // ----------------- Debug: MFCC min/max -----------------
    float mfcc_min = 1e6f, mfcc_max = -1e6f;
    for (int i = 0; i < MEL_BANDS; i++) {
        for (int j = 0; j < NUMBER_OF_WINDOWS; j++) {
            if (mfcc_mat[i][j] < mfcc_min) mfcc_min = mfcc_mat[i][j];
            if (mfcc_mat[i][j] > mfcc_max) mfcc_max = mfcc_mat[i][j];
        }
    }
    Serial.printf("MFCC range: min=%f max=%f\n", mfcc_min, mfcc_max);

    // ----------------- Copy MFCC to NN input buffer -----------------
    float *inputBuffer = nn->getInputBuffer();
    if (!inputBuffer) {
        Serial.println("[ERROR] NN input buffer is null, skipping inference");
        for (int i = 0; i < MEL_BANDS; i++) delete[] mfcc_mat[i];
        delete[] mfcc_mat;
        return;
    }

    copy_mat_inputBuffer(mfcc_mat, inputBuffer);

    Serial.println("=== MODEL INPUT (first 30 values) ===");
    for (int i = 0; i < 30; i++) {
        Serial.print(inputBuffer[i], 6);
        Serial.print(", ");
    }
    Serial.println("\n====================================");

    // ----------------- Debug: NN input buffer min/max -----------------
    float in_min = 1e6f, in_max = -1e6f;
    int i_min = -1, i_max = -1;
    for (int i = 0; i < SHAPE_INPUT; i++) {
        if (inputBuffer[i] < in_min) { in_min = inputBuffer[i]; i_min = i; }
        if (inputBuffer[i] > in_max) { in_max = inputBuffer[i]; i_max = i; }
    }
    Serial.printf("NN input buffer range: min=%f max=%f  i_min=%d i_max=%d\n", in_min, in_max, i_min, i_max);

    for (int i = 0; i < MEL_BANDS; i++) delete[] mfcc_mat[i];
    delete[] mfcc_mat;

    // ----------------- Run inference -----------------
    t3 = micros();
    nn->predict();
    t4 = micros();

    TfLiteTensor *outputTensor = nn->getOutputTensor();
    if (!outputTensor || !outputTensor->data.f) {
        Serial.println("[ERROR] Output tensor null, skipping prediction");
        return;
    }

    int feeling = interprets_output(outputTensor);

    // ----------------- Print timings -----------------
    Serial.printf("MFCC calc time: %.2f ms\n", (t2 - t1)/1000.0);
    Serial.printf("Inference time: %.2f ms\n", (t4 - t3)/1000.0);
    Serial.printf("Total calc time: %.2f ms\n", (t4 - t1)/1000.0);
}
