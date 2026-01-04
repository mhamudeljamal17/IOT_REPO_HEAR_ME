#ifndef __NeuralNetwork__
#define __NeuralNetwork__

#include <stdint.h>

//Crea un espacio de nombres para diferenciar
//Los nombres de las clases que se usarán de la librería Tflite
namespace tflite
{
    //El template permite escribir código genérico que puede funcionar con diferentes 
    //tipos de datos o valores sin tener que escribir el mismo código múltiples veces. 
    template <unsigned int tOpCount>
    class MicroMutableOpResolver;
    class ErrorReporter;
    class Model;
    class MicroInterpreter;
} // namespace tflite

struct TfLiteTensor;

class NeuralNetwork
{
private:
    tflite::MicroMutableOpResolver<10> *resolver;
    tflite::ErrorReporter *error_reporter;
    const tflite::Model *model;
    tflite::MicroInterpreter *interpreter;
    TfLiteTensor *input;
    TfLiteTensor *output;
    uint8_t *tensor_arena;

public:
    //Devuelve un puntero a un búfer de entrada
    float *getInputBuffer();
    //Devuelve un puntero a un búfer de salida
    TfLiteTensor *getOutputTensor();
    //Constructor para inicializar objetos de la clase
    NeuralNetwork();
    //Para realizar una predicción utilizando una red neuronal
    void predict();
};

#endif