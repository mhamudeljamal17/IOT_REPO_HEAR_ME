#include <esp_heap_caps.h>
#include "NeuralNetwork.h"
#include "model_data.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include "MFCC.h"
// #include "tensorflow/lite/c/common.h"  // Esto puede variar según tu configuración


// Memoria es utilizada por TensorFlow Lite Micro para almacenar tensores 
// y otros datos temporales necesarios para realizar inferencias con modelos 
const int kArenaSize = 2000000;

//Constructor
//Se realizan varias tareas esenciales para configurar TensorFlow Lite Micro y cargar un modelo de aprendizaje automático.
NeuralNetwork::NeuralNetwork()
{
    //Se crea instancia para manejar errores y generar mensajes de error durante la ejecución
    error_reporter = new tflite::MicroErrorReporter();

    // Se obtiene el modelo de aprendizaje automático a partir de una variable llamada converted_model_tflite
    model = tflite::GetModel(modelo_tesis_binario_Angry_finalv5_tflite);
    // Se verifica si la versión del modelo es compatible con la versión de esquema de TensorFlow Lite Micro
    // Si no son compatibles, se genera un mensaje de error.
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        TF_LITE_REPORT_ERROR(error_reporter, "Model provided is schema version %d not equal to supported version %d.",
                             model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    // Se crea una instancia de tflite::MicroMutableOpResolver con capacidad para 10 operadores. 
    // This pulls in the operators implementations we need
    resolver = new tflite::MicroMutableOpResolver<10>();
    // Se agregan operadores al resolver utilizando las siguientes funciones:
    resolver->AddFullyConnected();
    resolver->AddLogistic();
    resolver->AddReshape();
    //-------------------------
    resolver->AddConv2D();
    resolver->AddMaxPool2D();
    resolver->AddL2Normalization();

    // Se reserva un área de memoria en la "arena" para que TensorFlow Lite Micro la utilice durante la inferencia.
    tensor_arena = (uint8_t *) heap_caps_malloc(kArenaSize+16, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!tensor_arena)
    {
        TF_LITE_REPORT_ERROR(error_reporter, "Could not allocate arena");
        return;
    }
    // Ajusta la dirección de inicio para que esté alineada en 16 bytes
    uintptr_t aligned_address = (uintptr_t)tensor_arena;
    aligned_address = (aligned_address + 15) & ~15;
    tensor_arena = (uint8_t *)aligned_address;


    // Se crea una instancia de tflite::MicroInterpreter para ejecutar el modelo. 
    // Se le proporciona el modelo, el solucionador de operadores (resolver), 
    // el área de memoria (tensor_arena) y el manejador de errores (error_reporter).
    // Build an interpreter to run the model with.
    interpreter = new tflite::MicroInterpreter(
        model, *resolver, tensor_arena, kArenaSize, error_reporter);

    // Se asigna memoria dentro de la "arena" para los tensores del modelo utilizando el método AllocateTensors() del intérprete. 
    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
        return;
    }
    size_t used_bytes = interpreter->arena_used_bytes();
    TF_LITE_REPORT_ERROR(error_reporter, "Used bytes %d\n", used_bytes);

    // Se obtienen los punteros a los tensores de entrada y salida del modelo. 
    // Obtain pointers to the model's input and output tensors.
    // Configura el tensor de entrada para una matriz 2D (ajusta M y N)
    input = interpreter->input(0); // Puedes seguir usando el índice 0 si es el primer tensor de entrada
    input->dims->data[0] = 1;      // Batch size (generalmente 1)
    input->dims->data[1] = MEL_BANDS;      // Número de filas de la matriz de entrada
    input->dims->data[2] = NUMBER_OF_WINDOWS;      // Número de columnas de la matriz de entrada
    input->type = kTfLiteFloat32;  // Tipo de dato según corresponda
    
    output = interpreter->output(0);
}

// Esta función devuelve un puntero a la ubicación de memoria del tensor de entrada del modelo. 
float *NeuralNetwork::getInputBuffer()
{
    return input->data.f;
}

// Implementación de la función getOutputTensor
TfLiteTensor* NeuralNetwork::getOutputTensor() {
    return output;
}

// Esta función realiza la inferencia del modelo.
void NeuralNetwork::predict()
{
    interpreter->Invoke();
}
