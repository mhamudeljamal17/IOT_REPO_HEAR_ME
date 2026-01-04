#pragma once
#include <Arduino.h>

bool audio_init();
bool audio_record_2s(float* out_pcm_f32, int n_samples);
