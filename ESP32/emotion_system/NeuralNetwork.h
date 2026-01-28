// #ifndef __NeuralNetwork__
// #define __NeuralNetwork__

// #include <stdint.h>
// #include "tensorflow/lite/c/common.h"

// namespace tflite
// {
//     template <unsigned int tOpCount>
//     class MicroMutableOpResolver;
//     class ErrorReporter;
//     class Model;
//     class MicroInterpreter;
// }

// struct TfLiteTensor;

// class NeuralNetwork
// {
// private:
//     tflite::MicroMutableOpResolver<12> *resolver;
//     tflite::ErrorReporter *error_reporter;
//     const tflite::Model *model;
//     tflite::MicroInterpreter *interpreter;
//     TfLiteTensor *input;
//     TfLiteTensor *output;
//     uint8_t *tensor_arena;

// public:
//     float *getInputBuffer();
//     TfLiteTensor *getOutputTensor();
//     NeuralNetwork();
//     void predict();
// };

// #endif
#ifndef __NeuralNetwork__
#define __NeuralNetwork__

#include <stdint.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_data.h"

struct TfLiteTensor;

class NeuralNetwork {
private:
    const tflite::Model *model;
    tflite::MicroInterpreter *interpreter;
    TfLiteTensor *input;
    TfLiteTensor *output;
    uint8_t *tensor_arena;
    static constexpr int kArenaSize = 250 * 1024;  // ~250KB
    
    tflite::MicroMutableOpResolver<12> *resolver;
    
public:
    NeuralNetwork();
    ~NeuralNetwork();
            TfLiteTensor *getInputTensor();   // ✅ ADD THIS

    bool init();
    float *getInputBuffer();
    TfLiteTensor *getOutputTensor();
    void predict();
    bool isInitialized() const;
};

#endif