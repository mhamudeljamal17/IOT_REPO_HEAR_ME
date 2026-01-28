#include "AngerDetector.h"
#include "config.h"
#include "MFCC.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

AngerDetector::AngerDetector() : nn(nullptr), initialized(false) {}

AngerDetector::~AngerDetector() {
    if (nn) delete nn;
}
void AngerDetector::blinkRed(int times, int delay_ms) {
    for (int t = 0; t < times; t++) {
        for (int i = 0; i < strip->numPixels(); i++) {
            strip->setPixelColor(i, strip->Color(255, 0, 0)); // Red
        }
        strip->show();
        delay(delay_ms);

        strip->clear(); // Turn off
        strip->show();
        delay(delay_ms);
    }
}

bool AngerDetector::init(Adafruit_NeoPixel* strip) {
    Serial.println("[DETECTOR] Initializing Neural Network...");
    nn = new NeuralNetwork();
    
    if (!nn) {
        Serial.println("[DETECTOR][ERROR] Failed to create Neural Network");
        return false;
    }
    
    // CRITICAL: Call init() to load model and create interpreter
    if (!nn->init()) {
        Serial.println("[DETECTOR][ERROR] Failed to initialize Neural Network");
        return false;
    }
    
    initialized = true;
    Serial.println("[DETECTOR] Neural Network Ready!");
    return true;
}

void AngerDetector::audio_normalized(int32_t *raw_signal, float *normalized_signal) {
    for (int i = 0; i < BUFLEN; i++)
        normalized_signal[i] = (float)raw_signal[i] / 32768.0f;
}

void AngerDetector::copy_mat_inputBuffer(float **matrix, float *inputBuffer) {
    int index = 0;
    for (int i = 0; i < MEL_BANDS; i++) {
        for (int j = 0; j < NUMBER_OF_WINDOWS; j++) {
            inputBuffer[index++] = matrix[i][j];
        }
    }
}

int AngerDetector::interprets_output(float output_value) {
    int result = -1;
    
    Serial.println("\n---------------- Prediction ----------------");
    
    if (output_value >= ANGER_DETECTION_THRESHOLD) {
        result = 1;  // ANGRY
    } else {
        result = 0;  // NOT ANGRY
    }
    
    Serial.print("Sigmoid output: ");
    Serial.println(output_value, 9);
    
    if (result == 1) {
        Serial.println("Predicted emotion: ANGRY 🔴");
    } else {
        Serial.println("Predicted emotion: NOT ANGRY 🟢");
    }
    
    Serial.println("--------------------------------------------\n");
    return result;
}

int AngerDetector::detectAnger(uint8_t *audio_data, size_t data_size) {
    if (!initialized || !nn) {
        Serial.println("[DETECTOR][ERROR] Not initialized");
        return -1;
    }
    
    // Convert byte data to int16 samples
    int16_t *raw_signal = new int16_t[BUFLEN];
    float *inputAudio = new float[SHAPE_INPUT];
    
    memset(raw_signal, 0, BUFLEN * sizeof(int16_t));
    
    // Copy audio data (assuming data is already 16-bit PCM)
    size_t samples_to_copy = min((size_t)BUFLEN, data_size / 2);
    memcpy(raw_signal, audio_data, samples_to_copy * 2);
    
    // Normalize audio
    for (int i = 0; i < BUFLEN; i++) {
        inputAudio[i] = raw_signal[i] / 32767.0f;
    }
    delete[] raw_signal;
    
    // Allocate MFCC matrix
    float **mfcc_mat = new float*[MEL_BANDS];
    for (int i = 0; i < MEL_BANDS; i++) {
        mfcc_mat[i] = new float[NUMBER_OF_WINDOWS];
    }
    
    // Compute MFCC
    Serial.println("[DETECTOR] Computing MFCC...");
    unsigned long t1 = micros();
    mfccs(inputAudio, mfcc_mat);
    unsigned long t2 = micros();
    
    delete[] inputAudio;
    
    // Get NN input buffer and copy MFCC
    float *inputBuffer = nn->getInputBuffer();
    if (!inputBuffer) {
        Serial.println("[DETECTOR][ERROR] NN input buffer is null");
        for (int i = 0; i < MEL_BANDS; i++) delete[] mfcc_mat[i];
        delete[] mfcc_mat;
        return -1;
    }
    
    copy_mat_inputBuffer(mfcc_mat, inputBuffer);
    
    // Free MFCC matrix
    for (int i = 0; i < MEL_BANDS; i++) delete[] mfcc_mat[i];
    delete[] mfcc_mat;
    
    // Run inference
    Serial.println("[DETECTOR] Running inference...");
    unsigned long t3 = micros();
    nn->predict();
    unsigned long t4 = micros();
    
    // Get output
    TfLiteTensor *outputTensor = nn->getOutputTensor();
    if (!outputTensor || !outputTensor->data.f) {
        Serial.println("[DETECTOR][ERROR] Output tensor null");
        return -1;
    }
    
    float output_value = outputTensor->data.f[0];

    int result = interprets_output(output_value);
    
    if (result == 1) {

    if (strip) {
    blinkRed(3, 200);
} else {
    Serial.println("[DETECTOR][WARN] NeoPixel strip not initialized, skipping blink");
}

}


    Serial.printf("[DETECTOR] MFCC time: %.2f ms\n", (t2 - t1) / 1000.0);
    Serial.printf("[DETECTOR] Inference time: %.2f ms\n", (t4 - t3) / 1000.0);
    
    return result;
}


void AngerDetector::printDebugInfo() {
    if (nn) {
        Serial.println("\n[DETECTOR] Neural Network Info:");
        float *input = nn->getInputBuffer();
        if (input) {
            Serial.println("[DETECTOR] NN Status: OK - Input buffer ready");
        } else {
            Serial.println("[DETECTOR] NN Status: ERROR - No input buffer");
        }
    }
}



