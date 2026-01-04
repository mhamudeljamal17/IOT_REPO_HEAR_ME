#pragma once
#include <Arduino.h>
#include "config.h"

// Output MFCC shape: [kNumMel][kNumFrames]  => 20 x 63 (paper)
void compute_mfcc_20x63(const float* pcm_2s, float out_mfcc[kNumMel][kNumFrames]);
