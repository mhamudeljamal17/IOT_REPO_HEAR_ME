#ifndef __NeuralNetwork__
#define __NeuralNetwork__

#include <stdint.h>
#include "tensorflow/lite/c/common.h"

namespace tflite
{
    template <unsigned int tOpCount>
    class MicroMutableOpResolver;
    class ErrorReporter;
    class Model;
    class MicroInterpreter;
}

struct TfLiteTensor;

class NeuralNetwork
{
private:
    tflite::MicroMutableOpResolver<12> *resolver;
    tflite::ErrorReporter *error_reporter;
    const tflite::Model *model;
    tflite::MicroInterpreter *interpreter;
    TfLiteTensor *input;
    TfLiteTensor *output;
    uint8_t *tensor_arena;

public:
    float *getInputBuffer();
    TfLiteTensor *getOutputTensor();
    NeuralNetwork();
    void predict();
};

#endif