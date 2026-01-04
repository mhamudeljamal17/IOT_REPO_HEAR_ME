#include "fft256.h"
#include <math.h>

static inline void swapf(float& a, float& b) { float t=a; a=b; b=t; }

// Bit-reversal for N=256
static void bit_reverse(float* re, float* im) {
  uint8_t j = 0;
  for (uint16_t i = 0; i < 256; i++) {
    if (i < j) { swapf(re[i], re[j]); swapf(im[i], im[j]); }
    uint8_t bit = 128;
    while (j & bit) { j ^= bit; bit >>= 1; }
    j ^= bit;
  }
}

void fft256(float* re, float* im) {
  bit_reverse(re, im);

  for (int len = 2; len <= 256; len <<= 1) {
    float ang = -2.0f * (float)M_PI / (float)len;
    float wlen_re = cosf(ang);
    float wlen_im = sinf(ang);

    for (int i = 0; i < 256; i += len) {
      float w_re = 1.0f, w_im = 0.0f;

      for (int j = 0; j < len/2; j++) {
        int u = i + j;
        int v = i + j + len/2;

        float v_re = re[v]*w_re - im[v]*w_im;
        float v_im = re[v]*w_im + im[v]*w_re;

        float u_re = re[u];
        float u_im = im[u];

        re[u] = u_re + v_re;
        im[u] = u_im + v_im;
        re[v] = u_re - v_re;
        im[v] = u_im - v_im;

        // w *= wlen
        float next_w_re = w_re*wlen_re - w_im*wlen_im;
        float next_w_im = w_re*wlen_im + w_im*wlen_re;
        w_re = next_w_re;
        w_im = next_w_im;
      }
    }
  }
}
