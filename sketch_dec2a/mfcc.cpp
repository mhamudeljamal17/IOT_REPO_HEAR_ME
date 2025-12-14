#include "mfcc.h"
#include "fft256.h"
#include <math.h>

// Hamming window Eq. (5) in paper :contentReference[oaicite:13]{index=13}
static void make_hamming(float* w, int N) {
  for (int n = 0; n < N; n++) {
    w[n] = 0.53836f - 0.46164f * cosf(2.0f * (float)M_PI * n / (float)(N - 1));
  }
}

// Mel conversion Eq. (7) in paper :contentReference[oaicite:14]{index=14}
static float hz_to_mel(float f) { return 2595.0f * log10f(1.0f + f / 700.0f); }
static float mel_to_hz(float m) { return 700.0f * (powf(10.0f, m / 2595.0f) - 1.0f); }

// Build triangular mel filterbank: (kNumMel x (kNfft/2+1)) = 20 x 129
static void build_mel_fb(float fb[kNumMel][(kNfft/2)+1]) {
  const float fmin = 20.0f;               // paper uses 20 Hz min :contentReference[oaicite:15]{index=15}
  const float fmax = (float)kSampleRate / 2.0f;

  float mel_min = hz_to_mel(fmin);
  float mel_max = hz_to_mel(fmax);

  float mel_points[kNumMel + 2];
  float hz_points[kNumMel + 2];
  int   bins[kNumMel + 2];

  for (int i = 0; i < kNumMel + 2; i++) {
    mel_points[i] = mel_min + (mel_max - mel_min) * ((float)i / (float)(kNumMel + 1));
    hz_points[i]  = mel_to_hz(mel_points[i]);
    bins[i] = (int)floorf(((float)kNfft + 1.0f) * hz_points[i] / (float)kSampleRate);
    if (bins[i] < 0) bins[i] = 0;
    if (bins[i] > kNfft/2) bins[i] = kNfft/2;
  }

  for (int m = 0; m < kNumMel; m++) {
    for (int k = 0; k <= kNfft/2; k++) fb[m][k] = 0.0f;

    int left  = bins[m];
    int center= bins[m+1];
    int right = bins[m+2];

    for (int k = left; k < center; k++) {
      fb[m][k] = (center == left) ? 0.0f : ((float)(k - left) / (float)(center - left));
    }
    for (int k = center; k < right; k++) {
      fb[m][k] = (right == center) ? 0.0f : ((float)(right - k) / (float)(right - center));
    }
  }
}

// DCT Eq. (8) in paper :contentReference[oaicite:16]{index=16}
static void dct_type2_20(const float* E_20, float* C_20) {
  const int N = kNumMel; // 20
  for (int n = 0; n < N; n++) {
    float sum = 0.0f;
    for (int k = 0; k < N; k++) {
      sum += E_20[k] * cosf(((float)M_PI / (float)N) * ((float)k + 0.5f) * (float)n);
    }
    C_20[n] = sum;
  }
}

void compute_mfcc_20x63(const float* pcm_2s, float out_mfcc[kNumMel][kNumFrames]) {
  // 1) Pre-emphasis Eq. (4) :contentReference[oaicite:17]{index=17}
  static float x[kNumSamples];
  x[0] = pcm_2s[0];
  for (int i = 1; i < kNumSamples; i++) {
    x[i] = pcm_2s[i] - kPreEmphAlpha * pcm_2s[i - 1];
  }

  // 2) Hamming window
  static float ham[kFrameLen];
  make_hamming(ham, kFrameLen);

  // 3) Mel filterbank
  static float mel_fb[kNumMel][(kNfft/2)+1];
  static bool fb_ready = false;
  if (!fb_ready) { build_mel_fb(mel_fb); fb_ready = true; }

  // Buffers per frame
  float re[kNfft];
  float im[kNfft];
  float power[(kNfft/2)+1]; // 129 bins
  float melE[kNumMel];
  float mfcc20[kNumMel];

  // 4) Framing with hop=128 => 63 frames total
  for (int frame = 0; frame < kNumFrames; frame++) {
    int start = frame * kHopLen;

    // Copy + window into FFT buffers
    for (int n = 0; n < kNfft; n++) {
      float s = x[start + n] * ham[n];
      re[n] = s;
      im[n] = 0.0f;
    }

    // 5) FFT
    fft256(re, im);

    // 6) Power spectrum (positive half)
    for (int k = 0; k <= kNfft/2; k++) {
      power[k] = re[k]*re[k] + im[k]*im[k];
    }

    // 7) Apply mel filters + log
    for (int m = 0; m < kNumMel; m++) {
      float acc = 0.0f;
      for (int k = 0; k <= kNfft/2; k++) acc += power[k] * mel_fb[m][k];
      melE[m] = logf(acc + 1e-9f);
    }

    // 8) DCT → MFCC(20)
    dct_type2_20(melE, mfcc20);

    // Store as [mel][frame] => 20 x 63
    for (int m = 0; m < kNumMel; m++) out_mfcc[m][frame] = mfcc20[m];
  }
}
