#include "MFCC.h"
#include "FFT.h"
#include <Arduino.h>
#include <math.h>

/* ===================== GLOBAL STATIC BUFFERS ===================== */

static float window[SIZE_WINDOW];
static float fft_output[SIZE_WINDOW/2 + 1];
static float frame[SIZE_WINDOW];
static float tri_mat[MEL_BANDS][SIZE_WINDOW/2 + 1]; // precompute once

/* ===================== HELPER FUNCTIONS ===================== */

void hamming_window(float window[SIZE_WINDOW], int size) {
    for (int i = 0; i < size; i++)
        window[i] = 0.53836 - 0.46164 * cos((2 * PI * i) / (size - 1));
}

void preemphasis(float *input_signal) {
    const float coeff = 0.97f;
    for (int i = SHAPE_INPUT - 1; i >= 1; i--)
        input_signal[i] -= input_signal[i - 1] * coeff;
}

void fft_power_spectrum(float *input_signal, float *output_signal) {
    static float fft_input[SIZE_WINDOW];
    static float fft_output_local[SIZE_WINDOW];

    for (int k = 0; k < SIZE_WINDOW; k++)
        fft_input[k] = input_signal[k];

    fft_config_t *fft_plan =
        fft_init(SIZE_WINDOW, FFT_REAL, FFT_FORWARD, fft_input, fft_output_local);

    fft_execute(fft_plan);

    // DC
   // output_signal[0] = fft_plan->output[0] * fft_plan->output[0];
output_signal[0] = fabsf(fft_plan->output[0]);
    // Positive frequencies
    for (int k = 1; k < SIZE_WINDOW / 2; k++) {
        float re = fft_plan->output[2 * k];
        float im = fft_plan->output[2 * k + 1];
        //output_signal[k] = re * re + im * im;
        output_signal[k] = sqrtf(re * re + im * im);

    }

    // Nyquist
    //output_signal[SIZE_WINDOW / 2] =     fft_plan->output[1] * fft_plan->output[1];
output_signal[SIZE_WINDOW / 2] = fabsf(fft_plan->output[1]);
    fft_destroy(fft_plan);
}


int freq_to_bin(float freq) {
    return int((SIZE_WINDOW / 2 + 1) * freq / SAMPLING_RATE);
}

void triangular_filters() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    // Step 1: compute mel-scale limits
    float limits[NUM_BANDS_LIMITS];
    float frec_min_mel = 2595.0f * log10(1.0f + float(FREC_MIN) / 700.0f);
    float frec_max_mel = 2595.0f * log10(1.0f + float(FREC_MAX) / 700.0f);
    float separation_mel = (frec_max_mel - frec_min_mel) / float(NUM_BANDS_LIMITS - 1);

    limits[0] = frec_min_mel;
    for (int i = 1; i < NUM_BANDS_LIMITS; i++) limits[i] = limits[i - 1] + separation_mel;

    // Convert back to Hz
    for (int i = 0; i < NUM_BANDS_LIMITS; i++)
        limits[i] = 700.0f * (pow(10.0f, limits[i] / 2595.0f) - 1.0f);

    // Step 2: convert frequencies to FFT bins
    int bin_limits[NUM_BANDS_LIMITS];
    for (int i = 0; i < NUM_BANDS_LIMITS; i++)
        bin_limits[i] = freq_to_bin(limits[i]);

    // Step 3: build triangular filters
    for (int i = 2; i < NUM_BANDS_LIMITS; i++) {
        for (int j = 0; j < SIZE_WINDOW / 2 + 1; j++) {
            if (j >= bin_limits[i - 2] && j <= bin_limits[i - 1])
                tri_mat[i - 2][j] = float(j - bin_limits[i - 2]) / float(bin_limits[i - 1] - bin_limits[i - 2]);
            else if (j > bin_limits[i - 1] && j <= bin_limits[i])
                tri_mat[i - 2][j] = float(bin_limits[i] - j) / float(bin_limits[i] - bin_limits[i - 1]);
            else
                tri_mat[i - 2][j] = 0.0f;
        }
    }
}



// void dct1d(float *in, float *out) {
//     for (int k = 0; k < MEL_BANDS; k++) {
//         float sum = 0.0f;
//         for (int n = 0; n < MEL_BANDS; n++) {
//             sum += in[n] * cosf(PI * k * (2*n + 1) / (2.0f * MEL_BANDS));
//         }
//         out[k] = sum;
//     }
// }
void dct1d(float *in, float *out) {
    const float scale0 = sqrtf(1.0f / MEL_BANDS);
    const float scale  = sqrtf(2.0f / MEL_BANDS);

    for (int k = 0; k < MEL_BANDS; k++) {
        float sum = 0.0f;
        for (int n = 0; n < MEL_BANDS; n++) {
            sum += in[n] * cosf(PI * k * (2*n + 1) / (2.0f * MEL_BANDS));
        }
        out[k] = (k == 0) ? sum * scale0 : sum * scale;
    }
}


void dct_mat(float **mfcc_matrix) {
    float vec_in[MEL_BANDS], vec_out[MEL_BANDS];
    for (int i = 0; i < NUMBER_OF_WINDOWS; i++) {
        for (int j = 0; j < MEL_BANDS; j++) vec_in[j] = mfcc_matrix[j][i];
        dct1d(vec_in, vec_out);
        for (int j = 0; j < MEL_BANDS; j++) mfcc_matrix[j][i] = vec_out[j];
    }
}

/* ===================== MAIN MFCC FUNCTION ===================== */

void mfccs(float *input_signal, float **mfcc_matrix) {
    static bool initialized = false;
    if (!initialized) {
        hamming_window(window, SIZE_WINDOW);
        triangular_filters();
        initialized = true;
    }

    preemphasis(input_signal);

    int cont = 0;
    for (int w = 0; w < NUMBER_OF_WINDOWS; w++) {
        for (int j = 0; j < SIZE_WINDOW; j++) {
            frame[j] = input_signal[cont] * window[j];
            cont++;
        }
        cont -= (SIZE_WINDOW - WINDOW_STEP);

        fft_power_spectrum(frame, fft_output);

        for (int m = 0; m < MEL_BANDS; m++) {
            mfcc_matrix[m][w] = 0.0f;
            for (int k = 0; k < SIZE_WINDOW / 2 + 1; k++)
                mfcc_matrix[m][w] += tri_mat[m][k] * fft_output[k];
            mfcc_matrix[m][w] = 13.0f * logf(mfcc_matrix[m][w] + 1e-10f);


        }
    }

    dct_mat(mfcc_matrix);
}
