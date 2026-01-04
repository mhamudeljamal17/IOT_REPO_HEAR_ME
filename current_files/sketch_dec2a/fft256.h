#pragma once
#include <Arduino.h>

// In-place complex FFT, N must be 256.
// Input: real[i], imag[i]
// Output: FFT(real, imag)
void fft256(float* real, float* imag);
