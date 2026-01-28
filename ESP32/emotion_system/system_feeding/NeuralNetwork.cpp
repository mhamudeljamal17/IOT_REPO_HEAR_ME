// #include <esp_heap_caps.h>
// #include <Arduino.h>

// #include "NeuralNetwork.h"
// #include "model_data.h"

// #include "tensorflow/lite/micro/micro_error_reporter.h"
// #include "tensorflow/lite/micro/micro_interpreter.h"
// #include "tensorflow/lite/schema/schema_generated.h"

// #include "MFCC.h"

// // Arena size for TFLite - reduced to fit ESP32 PSRAM better
// const int kArenaSize = 200*1024;

// // Static error reporter (needed for TFLite Micro)
// static tflite::MicroErrorReporter micro_error_reporter;

// // Constructorin
// NeuralNetwork::NeuralNetwork()
// {
//     error_reporter = &micro_error_reporter;

//     // Get the model
//     Serial.println("[NN] Loading model from embedded data...");
//     model = tflite::GetModel(modelo_tesis_binario_Angry_finalv5_tflite);
    
//     if (model == nullptr) {
//         TF_LITE_REPORT_ERROR(error_reporter, "Model loading failed - nullptr");
//         return;
//     }

//     // Verify schema version
//     if (model->version() != TFLITE_SCHEMA_VERSION) {
//         TF_LITE_REPORT_ERROR(error_reporter, 
//             "Model schema version %d != supported %d\n", 
//             model->version(), TFLITE_SCHEMA_VERSION);
//         return;
//     }

//     Serial.println("[NN] Model schema verified");

//     // Create resolver with adequate operator support
//     resolver = new tflite::MicroMutableOpResolver<12>();
//     if (resolver == nullptr) {
//         TF_LITE_REPORT_ERROR(error_reporter, "Failed to create resolver");
//         return;
//     }


// resolver->AddConv2D();
// resolver->AddMaxPool2D();
// resolver->AddFullyConnected();
// resolver->AddReshape();
// resolver->AddRelu();
// resolver->AddSoftmax();

// // REQUIRED for INT8
// resolver->AddQuantize();
// resolver->AddDequantize();

// // Optional (only if model uses them)
// resolver->AddLogistic();   // sigmoid
// resolver->AddL2Normalization();



//     Serial.println("[NN] Operators added to resolver");

//     // Allocate tensor arena in SPIRAM (if available) or regular heap
//     Serial.print("[NN] Allocating tensor arena: ");
//     Serial.print(kArenaSize);
//     Serial.println(" bytes");
//     Serial.print("Free heap before alloc:");
//     Serial.println(ESP.getFreeHeap());
    

    
//     tensor_arena = (uint8_t *) heap_caps_malloc(kArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
//     if (!tensor_arena) {
//         Serial.println("[NN] SPIRAM allocation failed, trying internal memory...");
//         tensor_arena = (uint8_t *) heap_caps_malloc(kArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        
//         if (!tensor_arena) {
//             TF_LITE_REPORT_ERROR(error_reporter, "Failed to allocate tensor arena");
//             return;
//         }
//     }

//     Serial.println("[NN] Tensor arena allocated successfully");

//     // Create interpreter
//     Serial.println("[NN] Creating interpreter...");
//     interpreter = new tflite::MicroInterpreter(
//         model, *resolver, tensor_arena, kArenaSize, error_reporter);

//     if (interpreter == nullptr) {
//         TF_LITE_REPORT_ERROR(error_reporter, "Failed to create interpreter");
//         return;
//     }

//     // Allocate tensors
//     Serial.println("[NN] Allocating tensors...");
//     TfLiteStatus allocate_status = interpreter->AllocateTensors();
    
//     if (allocate_status != kTfLiteOk) {
//         TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed with status %d", allocate_status);
//         return;
//     }

//     size_t used_bytes = interpreter->arena_used_bytes();
//     Serial.print("[NN] Arena used: ");
//     Serial.print(used_bytes);
//     Serial.println(" bytes");

//     // Get input and output tensors
//     input = interpreter->input(0);

    
 


//     output = interpreter->output(0);

//     if (input == nullptr || output == nullptr) {
//         TF_LITE_REPORT_ERROR(error_reporter, "Failed to get input/output tensors");
//         return;
//     }

//     Serial.println("[NN] Input and output tensors obtained");
//     Serial.print("[NN] Input shape: [1, ");
//     Serial.print(input->dims->data[1]);
//     Serial.print(", ");
//     Serial.print(input->dims->data[2]);
//     Serial.println("]");
//     Serial.println("[NN] Neural Network initialized successfully!");

//     Serial.println("==== MODEL IO INFO ====");
//     Serial.print("Input tensor type: ");
//     Serial.println(input->type);

//     Serial.print("Output tensor type: ");
//     Serial.println(output->type);


//     Serial.print("Input scale: ");
//     Serial.println(input->params.scale, 10);
//     Serial.print("Input zero point: ");
//     Serial.println(input->params.zero_point);
//     Serial.println("=======================");

// }

// // Return input buffer pointer
// float *NeuralNetwork::getInputBuffer()
// {
    
//     if (input == nullptr || input->data.f == nullptr) {
//         Serial.println("[ERROR] Input buffer is nullptr");
//         return nullptr;
//     }
    
//     return input->data.f;
// }

// // Return output tensor
// TfLiteTensor* NeuralNetwork::getOutputTensor() {
//     if (output == nullptr) {
//         Serial.println("[ERROR] Output tensor is nullptr");
//         return nullptr;
//     }
//     return output;
// }

// // Perform inference
// void NeuralNetwork::predict()
// {
//     if (interpreter == nullptr) {
//         Serial.println("[ERROR] Interpreter is nullptr");
//         return;
//     }
    
//     TfLiteStatus invoke_status = interpreter->Invoke();
    
//     if (invoke_status != kTfLiteOk) {
//         Serial.print("[ERROR] Invoke failed with status: ");
//         Serial.println(invoke_status);
//         return;
//     }
// }

// #include "NeuralNetwork.h"
// #include <Arduino.h>
// #include <esp_heap_caps.h>

// NeuralNetwork::NeuralNetwork() 
//     : model(nullptr), interpreter(nullptr), input(nullptr), output(nullptr), 
//       tensor_arena(nullptr), resolver(nullptr) {
//     Serial.println("[NN] NeuralNetwork constructor called");
// }

// NeuralNetwork::~NeuralNetwork() {
//     if (tensor_arena) {
//         free(tensor_arena);
//         tensor_arena = nullptr;
//     }
//     if (resolver) {
//         delete resolver;
//         resolver = nullptr;
//     }
//     if (interpreter) {
//         delete interpreter;
//         interpreter = nullptr;
//     }
// }

// bool NeuralNetwork::init() {
//     Serial.println("[NN] Initializing Neural Network with Chirale TensorFlowLite...");
    
//     // Get model from model_data.h
//     model = tflite::GetModel(modelo_tesis_binario_Angry_finalv5_tflite);
//     if (model->version() != TFLITE_SCHEMA_VERSION) {
//         Serial.printf("[NN] Model schema version %d != TFLite schema version %d\n",
//                       model->version(), TFLITE_SCHEMA_VERSION);
//         return false;
//     }
    
//     Serial.println("[NN] Model loaded successfully");
//     Serial.printf("[NN] Model size: %u bytes\n", modelo_tesis_binario_Angry_finalv5_tflite_len);
    
//     // Create resolver with operations needed for the model
//     resolver = new tflite::MicroMutableOpResolver<12>();
    
//     if (!resolver) {
//         Serial.println("[NN][ERROR] Failed to create resolver");
//         return false;
//     }
    
//     // Add operations
//     resolver->AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED, 
//                         tflite::ops::micro::Register_FULLY_CONNECTED());
//     resolver->AddBuiltin(tflite::BuiltinOperator_RELU, 
//                         tflite::ops::micro::Register_RELU());
//     resolver->AddBuiltin(tflite::BuiltinOperator_SOFTMAX, 
//                         tflite::ops::micro::Register_SOFTMAX());
//     resolver->AddBuiltin(tflite::BuiltinOperator_CONV_2D, 
//                         tflite::ops::micro::Register_CONV_2D());
//     resolver->AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D, 
//                         tflite::ops::micro::Register_MAX_POOL_2D());
//     resolver->AddBuiltin(tflite::BuiltinOperator_RESHAPE, 
//                         tflite::ops::micro::Register_RESHAPE());
//     resolver->AddBuiltin(tflite::BuiltinOperator_ADD, 
//                         tflite::ops::micro::Register_ADD());
//     resolver->AddBuiltin(tflite::BuiltinOperator_MUL, 
//                         tflite::ops::micro::Register_MUL());
//     resolver->AddBuiltin(tflite::BuiltinOperator_QUANTIZE, 
//                         tflite::ops::micro::Register_QUANTIZE());
//     resolver->AddBuiltin(tflite::BuiltinOperator_DEQUANTIZE, 
//                         tflite::ops::micro::Register_DEQUANTIZE());
//     resolver->AddBuiltin(tflite::BuiltinOperator_LOGISTIC, 
//                         tflite::ops::micro::Register_LOGISTIC());
//     resolver->AddBuiltin(tflite::BuiltinOperator_AVERAGEPOOL_2D, 
//                         tflite::ops::micro::Register_AVERAGEPOOL_2D());
    
//     Serial.println("[NN] Resolver created with 12 operations");
    
//     // Allocate tensor arena in PSRAM if available
//     tensor_arena = (uint8_t *)heap_caps_malloc(
//         kArenaSize, 
//         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
//     );
    
//     if (!tensor_arena) {
//         Serial.println("[NN][WARN] PSRAM allocation failed, trying internal memory...");
//         tensor_arena = (uint8_t *)malloc(kArenaSize);
//         if (!tensor_arena) {
//             Serial.println("[NN][ERROR] Failed to allocate tensor arena");
//             return false;
//         }
//     }
    
//     Serial.printf("[NN] Tensor arena allocated: %d bytes\n", kArenaSize);
    
//     // Create interpreter
//     interpreter = new tflite::MicroInterpreter(
//         model, *resolver, tensor_arena, kArenaSize
//     );
    
//     if (!interpreter) {
//         Serial.println("[NN][ERROR] Failed to create interpreter");
//         return false;
//     }
    
//     // Allocate tensors
//     TfLiteStatus allocate_status = interpreter->AllocateTensors();
//     if (allocate_status != kTfLiteOk) {
//         Serial.println("[NN][ERROR] Failed to allocate tensors");
//         return false;
//     }
    
//     Serial.println("[NN] Tensors allocated successfully");
    
//     // Get input and output tensors
//     input = interpreter->input(0);
//     output = interpreter->output(0);
    
//     if (!input || !output) {
//         Serial.println("[NN][ERROR] Failed to get input/output tensors");
//         return false;
//     }
    
//     Serial.printf("[NN] Input shape: ");
//     for (int i = 0; i < input->dims->size; i++) {
//         Serial.printf("%d ", input->dims->data[i]);
//     }
//     Serial.println();
    
//     Serial.printf("[NN] Output shape: ");
//     for (int i = 0; i < output->dims->size; i++) {
//         Serial.printf("%d ", output->dims->data[i]);
//     }
//     Serial.println();
    
//     Serial.println("[NN] ✅ Neural Network initialized successfully!");
//     return true;
// }

// float *NeuralNetwork::getInputBuffer() {
//     if (!input || !input->data.f) {
//         return nullptr;
//     }
//     return input->data.f;
// }

// TfLiteTensor *NeuralNetwork::getOutputTensor() {
//     return output;
// }

// void NeuralNetwork::predict() {
//     if (!interpreter) {
//         Serial.println("[NN][ERROR] Interpreter not initialized");
//         return;
//     }
    
//     TfLiteStatus invoke_status = interpreter->Invoke();
    
//     if (invoke_status != kTfLiteOk) {
//         Serial.println("[NN][ERROR] Inference failed");
//         return;
//     }
    
//     Serial.println("[NN] Inference completed successfully");
// }

// bool NeuralNetwork::isInitialized() const {
//     return interpreter != nullptr && model != nullptr;
// }

#include "NeuralNetwork.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

NeuralNetwork::NeuralNetwork() 
    : model(nullptr), interpreter(nullptr), input(nullptr), output(nullptr), 
      tensor_arena(nullptr), resolver(nullptr) {
    Serial.println("[NN] NeuralNetwork constructor called");
}

NeuralNetwork::~NeuralNetwork() {
    if (tensor_arena) {
        free(tensor_arena);
        tensor_arena = nullptr;
    }
    if (resolver) {
        delete resolver;
        resolver = nullptr;
    }
    if (interpreter) {
        delete interpreter;
        interpreter = nullptr;
    }
}

bool NeuralNetwork::init() {
    Serial.println("[NN] Initializing Neural Network with Chirale TensorFlowLite...");
    
    // Get model from model_data.h
    model = tflite::GetModel(modelo_tesis_binario_Angry_finalv5_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[NN] Model schema version %d != TFLite schema version %d\n",
                      model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    
    Serial.println("[NN] Model loaded successfully");
    Serial.printf("[NN] Model size: %u bytes\n", modelo_tesis_binario_Angry_finalv5_tflite_len);
    
    // Create resolver with operations needed for the model
    resolver = new tflite::MicroMutableOpResolver<12>();
    
    if (!resolver) {
        Serial.println("[NN][ERROR] Failed to create resolver");
        return false;
    }
    
    // Add operations - Store registrations as const referencesresolver->AddBuiltin(
   // Add operations using public methods (not private AddBuiltin)
    resolver->AddFullyConnected();
    resolver->AddRelu();
    resolver->AddSoftmax();
    resolver->AddConv2D();
    resolver->AddMaxPool2D();
    resolver->AddAdd();
    resolver->AddMul();
    resolver->AddQuantize();
    resolver->AddDequantize();
    resolver->AddLogistic();
    resolver->AddL2Normalization();
    resolver->AddReshape();

    
    Serial.println("[NN] Resolver created with 12 operations");
    
    // Allocate tensor arena in PSRAM if available
    tensor_arena = (uint8_t *)heap_caps_malloc(
        kArenaSize, 
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    
    if (!tensor_arena) {
        Serial.println("[NN][WARN] PSRAM allocation failed, trying internal memory...");
        tensor_arena = (uint8_t *)malloc(kArenaSize);
        if (!tensor_arena) {
            Serial.println("[NN][ERROR] Failed to allocate tensor arena");
            return false;
        }
    }
    
    Serial.printf("[NN] Tensor arena allocated: %d bytes\n", kArenaSize);
    
    // Create interpreter
    interpreter = new tflite::MicroInterpreter(
        model, *resolver, tensor_arena, kArenaSize
    );
    
    if (!interpreter) {
        Serial.println("[NN][ERROR] Failed to create interpreter");
        return false;
    }
    
    // Allocate tensors
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("[NN][ERROR] Failed to allocate tensors");
        return false;
    }
    
    Serial.println("[NN] Tensors allocated successfully");
    
    // Get input and output tensors
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    if (!input || !output) {
        Serial.println("[NN][ERROR] Failed to get input/output tensors");
        return false;
    }
    
    Serial.printf("[NN] Input shape: ");
    for (int i = 0; i < input->dims->size; i++) {
        Serial.printf("%d ", input->dims->data[i]);
    }
    Serial.println();
    
    Serial.printf("[NN] Output shape: ");
    for (int i = 0; i < output->dims->size; i++) {
        Serial.printf("%d ", output->dims->data[i]);
    }
    Serial.println();
    
    Serial.println("[NN] ✅ Neural Network initialized successfully!");
    return true;
}
TfLiteTensor* NeuralNetwork::getInputTensor() {
    return input;
}
float *NeuralNetwork::getInputBuffer() {
    if (!input || !input->data.f) {
        return nullptr;
    }
    return input->data.f;
}

TfLiteTensor *NeuralNetwork::getOutputTensor() {
    return output;
}

void NeuralNetwork::predict() {
    if (!interpreter) {
        Serial.println("[NN][ERROR] Interpreter not initialized");
        return;
    }
    
    TfLiteStatus invoke_status = interpreter->Invoke();
    
    if (invoke_status != kTfLiteOk) {
        Serial.println("[NN][ERROR] Inference failed");
        return;
    }
    
    Serial.println("[NN] Inference completed successfully");
}

bool NeuralNetwork::isInitialized() const {
    return interpreter != nullptr && model != nullptr;
}