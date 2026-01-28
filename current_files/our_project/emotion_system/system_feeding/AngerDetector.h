#ifndef ANGER_DETECTOR_H
#define ANGER_DETECTOR_H

#include <stdint.h>
#include "NeuralNetwork.h"
#include "MFCC.h"
#include <Adafruit_NeoPixel.h>

// Constants from MFCC.h needed for buffer allocation
#define BUFLEN SHAPE_INPUT  // 8192 samples

class AngerDetector {
private:

    NeuralNetwork *nn;
    bool initialized;
        Adafruit_NeoPixel* strip = nullptr;

    void audio_normalized(int32_t *raw_signal, float *normalized_signal);
    void copy_mat_inputBuffer(float **matrix, float *inputBuffer);
    int interprets_output(float output_value);
    void setStrip(Adafruit_NeoPixel* s) { strip = s; }


public:
    AngerDetector();
    ~AngerDetector();
    
    bool init(Adafruit_NeoPixel* strip);
    int detectAnger(uint8_t *audio_data, size_t data_size);
    void printDebugInfo();
    void blinkRed(int times, int delay_ms);
  

};

#endif