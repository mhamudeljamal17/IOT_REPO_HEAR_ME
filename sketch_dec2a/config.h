#pragma once

// ===== Paper-aligned audio/MFCC parameters =====
// Paper uses fs=4096, audio length 2s, WS=256, hop=128, Mel filters M=20. :contentReference[oaicite:2]{index=2}
static constexpr int kSampleRate = 4096;
static constexpr int kAudioSeconds = 2;
static constexpr int kNumSamples = kSampleRate * kAudioSeconds;   // 8192

static constexpr int kFrameLen = 256;   // WS=256 :contentReference[oaicite:3]{index=3}
static constexpr int kHopLen   = 128;   // OS=128 :contentReference[oaicite:4]{index=4}
static constexpr int kNumFrames = (kNumSamples - kFrameLen) / kHopLen + 1; // = 63
static constexpr int kNumMel = 20;      // M=20 :contentReference[oaicite:5]{index=5}
static constexpr int kNfft = 256;
static constexpr float kPreEmphAlpha = 0.97f; // Eq. (4) :contentReference[oaicite:6]{index=6}
