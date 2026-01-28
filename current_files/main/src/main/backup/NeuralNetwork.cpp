#include <esp_heap_caps.h>
#include <Arduino.h>

#include "NeuralNetwork.h"
#include "model_data.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "MFCC.h"

// Arena size for TFLite - reduced to fit ESP32 PSRAM better
const int kArenaSize = 200*1024;

// Static error reporter (needed for TFLite Micro)
static tflite::MicroErrorReporter micro_error_reporter;

// Constructorin
NeuralNetwork::NeuralNetwork()
{
    error_reporter = &micro_error_reporter;

    // Get the model
    Serial.println("[NN] Loading model from embedded data...");
    model = tflite::GetModel(modelo_tesis_binario_Angry_finalv5_tflite);
    
    if (model == nullptr) {
        TF_LITE_REPORT_ERROR(error_reporter, "Model loading failed - nullptr");
        return;
    }

    // Verify schema version
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        TF_LITE_REPORT_ERROR(error_reporter, 
            "Model schema version %d != supported %d\n", 
            model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    Serial.println("[NN] Model schema verified");

    // Create resolver with adequate operator support
    resolver = new tflite::MicroMutableOpResolver<12>();
    if (resolver == nullptr) {
        TF_LITE_REPORT_ERROR(error_reporter, "Failed to create resolver");
        return;
    }


resolver->AddConv2D();
resolver->AddMaxPool2D();
resolver->AddFullyConnected();
resolver->AddReshape();
resolver->AddRelu();
resolver->AddSoftmax();

// REQUIRED for INT8
resolver->AddQuantize();
resolver->AddDequantize();

// Optional (only if model uses them)
resolver->AddLogistic();   // sigmoid
resolver->AddL2Normalization();


    Serial.println("[NN] Operators added to resolver");

    // Allocate tensor arena in SPIRAM (if available) or regular heap
    Serial.print("[NN] Allocating tensor arena: ");
    Serial.print(kArenaSize);
    Serial.println(" bytes");
    Serial.print("Free heap before alloc:");
    Serial.println(ESP.getFreeHeap());
    

    
    tensor_arena = (uint8_t *) heap_caps_malloc(kArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (!tensor_arena) {
        Serial.println("[NN] SPIRAM allocation failed, trying internal memory...");
        tensor_arena = (uint8_t *) heap_caps_malloc(kArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        
        if (!tensor_arena) {
            TF_LITE_REPORT_ERROR(error_reporter, "Failed to allocate tensor arena");
            return;
        }
    }

    Serial.println("[NN] Tensor arena allocated successfully");

    // Create interpreter
    Serial.println("[NN] Creating interpreter...");
    interpreter = new tflite::MicroInterpreter(
        model, *resolver, tensor_arena, kArenaSize, error_reporter);

    if (interpreter == nullptr) {
        TF_LITE_REPORT_ERROR(error_reporter, "Failed to create interpreter");
        return;
    }

    // Allocate tensors
    Serial.println("[NN] Allocating tensors...");
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    
    if (allocate_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed with status %d", allocate_status);
        return;
    }

    size_t used_bytes = interpreter->arena_used_bytes();
    Serial.print("[NN] Arena used: ");
    Serial.print(used_bytes);
    Serial.println(" bytes");

    // Get input and output tensors
    input = interpreter->input(0);

    
 


    output = interpreter->output(0);

    if (input == nullptr || output == nullptr) {
        TF_LITE_REPORT_ERROR(error_reporter, "Failed to get input/output tensors");
        return;
    }

    Serial.println("[NN] Input and output tensors obtained");
    Serial.print("[NN] Input shape: [1, ");
    Serial.print(input->dims->data[1]);
    Serial.print(", ");
    Serial.print(input->dims->data[2]);
    Serial.println("]");
    Serial.println("[NN] Neural Network initialized successfully!");

    Serial.println("==== MODEL IO INFO ====");
    Serial.print("Input tensor type: ");
    Serial.println(input->type);

    Serial.print("Output tensor type: ");
    Serial.println(output->type);


    Serial.print("Input scale: ");
    Serial.println(input->params.scale, 10);
    Serial.print("Input zero point: ");
    Serial.println(input->params.zero_point);
    Serial.println("=======================");

}

// Return input buffer pointer
float *NeuralNetwork::getInputBuffer()
{
    
    if (input == nullptr || input->data.f == nullptr) {
        Serial.println("[ERROR] Input buffer is nullptr");
        return nullptr;
    }
    
    return input->data.f;
}

// Return output tensor
TfLiteTensor* NeuralNetwork::getOutputTensor() {
    if (output == nullptr) {
        Serial.println("[ERROR] Output tensor is nullptr");
        return nullptr;
    }
    return output;
}

// Perform inference
void NeuralNetwork::predict()
{
    if (interpreter == nullptr) {
        Serial.println("[ERROR] Interpreter is nullptr");
        return;
    }
    
    TfLiteStatus invoke_status = interpreter->Invoke();
    
    if (invoke_status != kTfLiteOk) {
        Serial.print("[ERROR] Invoke failed with status: ");
        Serial.println(invoke_status);
        return;
    }
}