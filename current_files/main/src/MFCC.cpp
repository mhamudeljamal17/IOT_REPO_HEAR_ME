#include "MFCC.h"
#include "FFT.h"
#include "arduino.h"
#include <math.h>
// #include "FFT_signal.h"

//Funciones
//*********************************************************************************
float **win_mat = nullptr; // Declaración global de puntero de matriz dinámica
float **fft_mat = nullptr; // Declaración global de puntero de matriz dinámica
float **tri_mat = nullptr; // Declaración global de puntero de matriz dinámica

//Función para cacular una ventana de tipo Hamming
//------------------------------------------------------------------------------------------
void hamming_window(float window[SIZE_WINDOW], int size)
{
  for(int i=0; i < size; i++)
    window[i] = 0.53836-0.46164*(cos((2*PI*i)/(size-1)));
}

//Función para cacular el preénfasis de una señal
//------------------------------------------------------------------------------------------
void preemphasis(float *input_signal)
{
  const float coeff = 0.97;

  //Ciclo donde se le resta al dato actual el dato anterior multiplicado por el coeficiente
  for(int i=SHAPE_INPUT-1; i>=1; i--)
    input_signal[i] = input_signal[i]-input_signal[i-1]*coeff;
}

//Función para hacer el ventaneo de una señal
//------------------------------------------------------------------------------------------
void windowing(float *input_signal, float **output_mat, float window[SIZE_WINDOW])
{
  //Inicializa contador
  int cont = 0;
  
  //Ciclos aninados para realizar ventaneo
  for(int i=0; i<NUMBER_OF_WINDOWS; i++)
  {
    for(int j=0; j<SIZE_WINDOW; j++)
    {
      output_mat[j][i] = window[j]*input_signal[cont];        
      cont++;
    }
    cont = cont-(SIZE_WINDOW-WINDOW_STEP);
  }
}

//Función para calcular el espectro de potencia con fft de una señal
//------------------------------------------------------------------------------------------
void fft_power_spectrum(float input_signal[SIZE_WINDOW], float output_signal[int(SIZE_WINDOW/2)+1])
{
  //Inizcialización de variables para uso de librería fft
  float fft_input[SIZE_WINDOW];
  float fft_output[SIZE_WINDOW];

  //Inicializa la fft
  fft_config_t *real_fft_plan = fft_init(SIZE_WINDOW, FFT_REAL, FFT_FORWARD, fft_input, fft_output);

  //Llena buffer de entrada con datos de FFT
  for (int k = 0; k < SIZE_WINDOW; k++)
    real_fft_plan->input[k] = (float)input_signal[k];

  //Ejecuta transformada de Fourier
  fft_execute(real_fft_plan);

  //Calcula el absoluto de la componente de frecuencia directa (DC component)
  output_signal[0] = abs(real_fft_plan->output[0]);

  //Ciclo para calcular magnitud de los coeficientes (son la mitad del tamaño original)
  for (int k = 1 ; k < real_fft_plan->size/2; k++)
    //Saca la magnitud
    /*NOTA: The real part of a magnitude at a frequency is followed by the corresponding imaginary part in the output*/
    output_signal[k] = sqrt(pow(real_fft_plan->output[2*k],2) + pow(real_fft_plan->output[2*k+1],2));

  //Crea variable contador en dato central
  int cont = real_fft_plan->size/2;

  //Calcula el absoluto del coeficiente central (Center Coefficient) o frecuencia Nyquist 
  output_signal[cont] = abs(real_fft_plan->output[1]);
  
  fft_destroy(real_fft_plan); // Clean up at the end to free the memory allocated
}

//Función para calcular el espectro de potencia con fft de una señal
//------------------------------------------------------------------------------------------
void spectrogram(float **input_matrix, float **output_matrix, int col, int row)
{
  //Vectores auxiliares
  float vector_c[row];
  float vector_fft[int(row/2)+1];

  //Ciclo para recorrer la matriz columna por columna
  for(int i=0; i<col; i++)
  {
    //Ciclo para formar vector columna
    for(int j=0; j<row; j++)
      vector_c[j] = input_matrix[j][i];

    //Función para sacar FFT de vector columna
    fft_power_spectrum(vector_c, vector_fft);

    //Ciclo para asignar vector fft a columna correspondiente de matriz
    for(int j=0; j<int(row/2)+1; j++)
      output_matrix[j][i] = vector_fft[j];
  }
}

// Función para crear filtros triangulares 
// ------------------------------------------------------------------------------------------
void triangular_filters(float **triangular_matrix)
{
  //Vector de límites de bandas
  float limits[NUM_BANDS_LIMITS];
  
  //Frecuencia mínima y máxima en Mels
  float frec_min_mel = 2595.0 * log10(1.0 + float(FREC_MIN) / 700.0);
  float frec_max_mel = 2595.0 * log10(1.0 + float(FREC_MAX) / 700.0);
  //Calcula la distancia de separación en Mels de las bandas a formar
  float separation_mel = (frec_max_mel-frec_min_mel)/float(NUM_BANDS_LIMITS-1);

  //Crea vector de límites en Mels
  limits[0] = frec_min_mel;
  for(int i=1; i<NUM_BANDS_LIMITS; i++)
    limits[i] = limits[i-1]+separation_mel;

  //Crea vector de límites en Hertz
  for(int i=0; i<NUM_BANDS_LIMITS; i++)
    limits[i] = 700.0 * (pow(10.0,(limits[i]/2595.0)) - 1.0);

  //Crea vector de límites de bandas en bins fft (dependen de la longitud de la ventana fft)
  for(int i=0; i<NUM_BANDS_LIMITS; i++)
    limits[i] = floor(float(int(SIZE_WINDOW/2)+1)*limits[i]/FREC_MAX);

  // Cilco para crear filtros triangulares y ponerlos en matriz contenedora
  for(int i=2; i<NUM_BANDS_LIMITS; i++)
  {
    for(int j=0; j<int(SIZE_WINDOW/2)+1; j++)
    {
      if(j>=limits[i-2] && j<=limits[i-1])
        triangular_matrix[i-2][j] = (j - limits[i-2]) / (limits[i-1] - limits[i-2]);
      else if(j>limits[i-1] && j<=limits[i])
        triangular_matrix[i-2][j] = (limits[i] - j) / (limits[i] - limits[i-1]);
      else
        triangular_matrix[i-2][j] = 0;
    }
  }
}

// Función para aplicar filtros triangulares mediante multiplicación matricial y obtener espectrograma de Mel
// ------------------------------------------------------------------------------------------
void mel_spectrogram(float **triangular_matrix, float **fft_matrix, float **mfcc_matrix)
{
  //Inicializar elementos de matriz mfcc
  for (int i = 0; i<MEL_BANDS; i++)
    for (int j = 0; j<NUMBER_OF_WINDOWS; j++)
      mfcc_matrix[i][j] = 0;

  // Multiplicación de matrices (forma espectrograma de mel)
  for(int i=0; i<MEL_BANDS; i++)
  {
    for(int j=0; j<NUMBER_OF_WINDOWS; j++)
    {
      for(int k=0; k<int(SIZE_WINDOW/2)+1; k++)
        mfcc_matrix[i][j] += triangular_matrix[i][k] * fft_matrix[k][j];

    //Aplica transformación logaritmica
    mfcc_matrix[i][j] = 13*log(mfcc_matrix[i][j]);
    }
  }
}

// Función para calcular la transformada discreta del coseno (DCT) en 1 dimensión
// ------------------------------------------------------------------------------------------
void dct1d(float vec_input[MEL_BANDS], float vec_output[MEL_BANDS]) 
{
  for (int k = 0; k < MEL_BANDS; k++) 
  {    
    float sum = 0.0;
      for (int n = 0; n < MEL_BANDS; n++)
          sum += vec_input[n] * cos((PI * k * (2.0 * n + 1.0)) / (2.0 * MEL_BANDS));
    
    float f = (k == 0) ? sqrt(1/(4.0*MEL_BANDS)) : sqrt(1/(2.0*MEL_BANDS));
    vec_output[k] = 2*sum*f;
  }
}

//Función para aplicar la transformada discreta del coseno en las columnas de una matriz
// ------------------------------------------------------------------------------------------
void dct_mat(float **mfcc_matrix)
{
  float vector_in[MEL_BANDS], vector_out[MEL_BANDS];
  
  for(int i = 0; i < NUMBER_OF_WINDOWS; i++)
  {  
    for(int j = 0; j < MEL_BANDS; j++)
      vector_in[j] = mfcc_matrix[j][i];

    //Aplica DCT
    dct1d(vector_in, vector_out);
    
    for(int j = 0; j < MEL_BANDS; j++)
      mfcc_matrix[j][i] = vector_out[j];
  }
}

//Función para calcular MFCC de una señal
//*********************************************************************************************
//*********************************************************************************************
void mfccs(float *input_signal, float **mfcc_matrix)
{
  //Inicialización de vectores y matrices
  //------------------------------------------------------------------------------
  
  //Inicializa vector ventana
  float window[SIZE_WINDOW];

  // Crear una matriz dinámica para Matriz de Ventaneos
  win_mat = new float*[SIZE_WINDOW];  // Primera dimensión
  for (int i = 0; i < SIZE_WINDOW; ++i) {
      win_mat[i] = new float[NUMBER_OF_WINDOWS];  // Segunda dimensión
  }

  // Crear una matriz dinámica para Matriz de Espectrograma
  fft_mat = new float*[int(SIZE_WINDOW/2)+1];  // Primera dimensión
  for (int i = 0; i < int(SIZE_WINDOW/2)+1; ++i) {
      fft_mat[i] = new float[NUMBER_OF_WINDOWS];  // Segunda dimensión
  }

  //------------------------------------------------------------------------------

  //Función para calcular ventana Hamming
  hamming_window(window, SIZE_WINDOW);
  
  //Se aplica preénfasis a la señal
  preemphasis(input_signal);

  // Se aplica el ventaneo a la señal
  windowing(input_signal, win_mat, window);

  //Forma espectrograma
  spectrogram(win_mat, fft_mat, NUMBER_OF_WINDOWS, SIZE_WINDOW);

  //Liberar memoria de matriz "win_mat"
  for (int i = 0; i < SIZE_WINDOW; i++) {
      delete[] win_mat[i];
  }
  delete[] win_mat;

  // Crear una matriz dinámica para Matriz de Filtros triangulares
  tri_mat = new float*[MEL_BANDS];  // Primera dimensión
  for (int i = 0; i < MEL_BANDS; ++i) {
      tri_mat[i] = new float[int(SIZE_WINDOW/2)+1];  // Segunda dimensión
  }

  //Crea Filtros triangulares
  triangular_filters(tri_mat);

  //Aplica filtros triangulares para obtener espectrograma de Mel
  mel_spectrogram(tri_mat, fft_mat, mfcc_matrix);

  //Liberar memoria de matriz "fft_mat"
  for (int i = 0; i < int(SIZE_WINDOW/2)+1; i++) {
      delete[] fft_mat[i];
  }
  delete[] fft_mat;

  //Liberar memoria de matriz "tri_mat"
  for (int i = 0; i < MEL_BANDS; i++) {
      delete[] tri_mat[i];
  }
  delete[] tri_mat;

  //Aplica transformada discreta de coseno para obtener MFCCs
  dct_mat(mfcc_matrix);
}
//*********************************************************************************************
//*********************************************************************************************