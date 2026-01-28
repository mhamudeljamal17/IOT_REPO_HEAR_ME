#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "driver/i2s.h"
#include "soc/i2s_reg.h"
#include <esp_wifi.h>
#include <esp_bt.h>
#include "SD.h"
#include "SPI.h"
#include "MFCC.h"
#include "emotions.h"
#include "esp32-hal-timer.h"
#include "NeuralNetwork.h"
#include "tensorflow/lite/c/common.h"

//Chip select de uSD
#define CS 5
//Define pines SPI de Matriz
#define MOSI 23 //MOSI
#define SCK 18  //SCK
#define SS 4  //Chip Select de Matrices
#define NUM_MAT 1 //Numero de matrices led
//Define pines I2S para micrófono
#define I2S_LRCL 25
#define I2S_DOUT 33
#define I2S_BCLK 32
#define BUFLEN SHAPE_INPUT
#define MAX 138316/50
#define MIN -123828/50

//Variable de número de puerto I2S
static const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number
//Define variable para almacenar número de bytes leidos en el buffer I2S
size_t number_bytes_read;
//Instancia red neuronal
NeuralNetwork *nn;
//Instancia objeto matriz de leds
MD_MAX72XX mxObj = MD_MAX72XX(MD_MAX72XX::GENERIC_HW, SS, NUM_MAT);
//Mapea matrices a utilizar
int Mat[1] = {0};

//Lista de emociones a predecir
//----------------------------------------------------------------------------------------
String emotion_vec[] = {"Other","Angry"};

//Funciones de configuración de driver del protocolo I2S
//----------------------------------------------------------------------------------------
static const i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = 4096,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
  .intr_alloc_flags = 0, // default interrupt priority
  .dma_buf_count = 64, //Número de buffers
  .dma_buf_len = 512,   //Longitud de cada buffer (en bytes)
  .use_apll = false
};

static const i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_BCLK,
  .ws_io_num = I2S_LRCL,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num = I2S_DOUT
};

//----------------------------------------------------------------------------------------

//Prototipos
void audio_normalized(int32_t *raw_signal, float *normalized_signal);
void print_matrix(float **matrix, int col, int row);
void create_arr_raw(int32_t *&raw_signal);
void create_arr_nor(float *&inputAudio);
void create_mat(float **&mfcc_mat);
void free_memory_raw(int32_t *&raw_signal);
void free_memory_nor(float *&inputAudio);
void free_memory_mat(float **&mfcc_mat);
void copy_mat_inputBuffer(float **matrix, float *inputBuffer);
int interprets_output(TfLiteTensor *outputTensor);

//Setup
//******************************************************************************************
void setup()
{
  //Inicializa puerto serial
  Serial.begin(115200);

  // Apagar el Bluetooth
  esp_bt_controller_disable();
  // Apagar el Wi-Fi  
  esp_wifi_stop(); 

  //Estado de la memoria
  Serial.println("heap_caps_get_largest_free_block:");
  Serial.println("MALLOC_CAP_INTERNAL:");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  Serial.println("MALLOC_CAP_SPIRAM:");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  Serial.println("MALLOC_CAP_DEFAULT:");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT));

  //Instancia red neuronal
  nn = new NeuralNetwork();

  //Inicializa objeto
  mxObj.begin();

  //Configurar protocolo I2S
  pinMode(I2S_DOUT, INPUT);
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);   //install and start i2s driver
  REG_SET_BIT(  I2S_TIMING_REG(i2s_num),BIT(9));   /*  #include "soc/i2s_reg.h"   I2S_NUM -> 0 or 1*/
  REG_SET_BIT( I2S_CONF_REG(i2s_num), I2S_RX_MSB_SHIFT);
  i2s_set_pin(i2s_num, &pin_config);
}

//Ciclo principal
//******************************************************************************************
//******************************************************************************************
void loop()
{
  //Crea arreglo dinámico para almacenar la señal de entrada
  int32_t *raw_signal = nullptr;
  create_arr_raw(raw_signal);    
  
  //Revisa buffer DMA para ver número de bytes leídos
  esp_err_t err = i2s_read(i2s_num, raw_signal, BUFLEN*sizeof(int32_t), &number_bytes_read, portMAX_DELAY);

  //Si no hay error y el número de byes es mayor a cero
  if(err == ESP_OK && number_bytes_read>BUFLEN)
  {
    //Crea arreglo dinámico para almacenar la señal de entrada
    float *inputAudio = nullptr;
    create_arr_nor(inputAudio);

    //Crea matriz dinámica para almacenar entrada de la red neuronal
    float **mfcc_mat = nullptr;
    create_mat(mfcc_mat);       

    //Toma muestras de audio en buffer y las normaliza para obtener el audio de entrada
    audio_normalized(raw_signal, inputAudio);

    //Liberar memoria de vector "raw_signal"
    free_memory_raw(raw_signal);

    // Función para calcular MFCCs de una señal
    long int t1 = micros(); //Tiempo en momento 1
    mfccs(inputAudio, mfcc_mat);
    long int t2 = micros(); //Tiempo en momento 2

    // Liberar memoria de vector "inputAudio"
    free_memory_nor(inputAudio);

    // Obtener un puntero al buffer de entrada
    float *inputBuffer = nn->getInputBuffer();

    // Copiar los datos de matriz al buffer de entrada
    copy_mat_inputBuffer(mfcc_mat, inputBuffer);

    // Libera memoria de matriz dinámica "mfcc_mat"
    free_memory_mat(mfcc_mat); 

    //Realiza Inferencia en Red Neuronal
    long int t3 = micros(); //Tiempo en momento 3  
    nn->predict();
    long int t4 = micros(); //Tiempo en momento 4 

    //Obtener el tensor de salida
    TfLiteTensor *outputTensor = nn->getOutputTensor();

    //Calcula en base al "outputTensor" el sentimiento de salida
    int feeling = interprets_output(outputTensor);

    //Función para dibujar la emoción en la matriz
    emotion(feeling, Mat, 0, mxObj);

    //Imprime resultados
    Serial.print("Tiempo en calculo de MFCC: ");Serial.print((t2-t1)*1.0/1000);Serial.println(" Milisegundos");
    Serial.print("Tiempo en inferir: ");Serial.print((t4-t3)*1.0/1000);Serial.println(" Milisegundos");
    Serial.print("Tiempo de calculo completo: ");Serial.print((t4-t1)*1.0/1000);Serial.println(" Milisegundos");
    Serial.print("Emocion predicha: ");Serial.println(emotion_vec[feeling]);
    Serial.print("Total heap: ");Serial.println(ESP.getHeapSize());
    Serial.print("Free heap: ");Serial.println(ESP.getFreeHeap());
    Serial.print("Total SPRAM: ");Serial.println(heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    Serial.print("Free SPRAM: ");Serial.println(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  }
  else
  {
    //Liberar memoria de vector "raw_signal"
    free_memory_raw(raw_signal);
  }
  
}

//Funciones
//******************************************************************************************
//******************************************************************************************

//Función para normalizar audio de entrada
// ------------------------------------------------------------------------------------------
void audio_normalized(int32_t *raw_signal, float *normalized_signal)
{
  int32_t aux = 0;

  for(int i=0; i<BUFLEN; i++)
  {
    aux = (raw_signal[i] >> 14)+7245;
    normalized_signal[i] =  float(aux-MIN)/float(MAX-MIN)*2.0-1.0;
  //   // Serial.println(normalized_signal[i], 7);
  }
}

//Copia matriz de entrada al buffer de entrada
// ------------------------------------------------------------------------------------------
void copy_mat_inputBuffer(float **matrix, float *inputBuffer)
{
  int index = 0;
  for (int i = 0; i < MEL_BANDS; i++) {
    for (int j = 0; j < NUMBER_OF_WINDOWS; j++) {
      inputBuffer[index] = matrix[i][j];
      index++;
    }
  }
}

//Interpreta tensor de salida y regresa el número de sentimiento correspondiente
// ------------------------------------------------------------------------------------------
int interprets_output(TfLiteTensor *outputTensor)
{
  float umbral = 0.5; // Inicializa la posición del máxima
  int result;
  // Acceder a los 5 valores de salida
  Serial.println("\n-----------------------------------------------------------\n");
  
  if (outputTensor->data.f[0] >= umbral)
    result = 1;
  else
    result = 0;

  Serial.print("Salida ");
  Serial.print(": ");
  Serial.print(outputTensor->data.f[0], 8); // Mostrar el valor con dos decimales
  Serial.print(" - ");
  Serial.println(emotion_vec[result]);

  return result;
}

// Función para imprimir una matriz dinámica
// ------------------------------------------------------------------------------------------
void print_matrix(float **matrix, int col, int row)
{
  for(int i=0; i < row; i++)
  {
    for(int j=0; j < col; j++)
      Serial.println(matrix[i][j], 5);
  }  
}

//Crea vector dinámico para almacenar la señal de entrada (RAW)
// ------------------------------------------------------------------------------------------
void create_arr_raw(int32_t *&raw_signal)
{
  raw_signal = new int32_t[BUFLEN];
}

//Crea vector dinámico para almacenar la señal de entrada (Normalizada)
// ------------------------------------------------------------------------------------------
void create_arr_nor(float *&inputAudio)
{
  inputAudio = new float[SHAPE_INPUT];
}

//Crear una matriz dinámica usando punteros
// ------------------------------------------------------------------------------------------
void create_mat(float **&mfcc_mat)
{
  //Matriz de MFCCs
  mfcc_mat = new float*[MEL_BANDS];  // Primera dimensión
  for (int i = 0; i < MEL_BANDS; ++i) {
      mfcc_mat[i] = new float[NUMBER_OF_WINDOWS];  // Segunda dimensión
  }
}

// Libera la memoria de arreglo raw cuando sea necesario (fuera de setup y loop)
// ------------------------------------------------------------------------------------------
void free_memory_raw(int32_t *&raw_signal)
{
  delete[] raw_signal;
}

// Libera la memoria de arreglo normalizado cuando sea necesario (fuera de setup y loop)
// ------------------------------------------------------------------------------------------
void free_memory_nor(float *&inputAudio)
{
  delete[] inputAudio;
}

// Libera la memoria de matriz cuando sea necesario (fuera de setup y loop)
// ------------------------------------------------------------------------------------------
void free_memory_mat(float **&mfcc_mat) 
{
  // Liberar la memoria para cada fila de la matriz
  for (int i = 0; i < MEL_BANDS; ++i) {
      delete[] mfcc_mat[i];
  }
  // Liberar la memoria para la matriz de punteros a filas
  delete[] mfcc_mat;
}