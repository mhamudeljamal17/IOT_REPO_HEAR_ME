#include "AudioCapture.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

/* ================= CONFIG ================= */
#define SAMPLE_RATE     16000
#define BUFFER_SIZE     1024
#define MIC_GAIN        4

// PDM pins
#define PDM_CLK   42
#define PDM_DATA  41


/* ========================================== */

AudioCapture::AudioCapture()
    : audio_buffer(nullptr),
      audio_size(0),
      initialized(false),
      rx_chan(nullptr) {}

AudioCapture::~AudioCapture() {
    stop();
    freeBuffer();
}

bool AudioCapture::init() {
    Serial.println("[AUDIO] Initializing PDM microphone (I2S PDM API)...");

    /* ---------- I2S CHANNEL ---------- */
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &rx_chan);
    if (ret != ESP_OK) {
        Serial.printf("[AUDIO][ERROR] i2s_new_channel failed: %s\n",
                      esp_err_to_name(ret));
        return false;
    }

    /* ---------- PDM RX CONFIG ---------- */
    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO
        ),
        .gpio_cfg = {
            .clk = (gpio_num_t)PDM_CLK,
            .din = (gpio_num_t)PDM_DATA,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    ret = i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_cfg);
    if (ret != ESP_OK) {
        Serial.printf("[AUDIO][ERROR] PDM RX init failed: %s\n",
                      esp_err_to_name(ret));
        return false;
    }

    ret = i2s_channel_enable(rx_chan);
    if (ret != ESP_OK) {
        Serial.printf("[AUDIO][ERROR] Channel enable failed: %s\n",
                      esp_err_to_name(ret));
        return false;
    }

    initialized = true;
    Serial.println("[AUDIO] PDM microphone ready");
    return true;
}

bool AudioCapture::startRecording(uint32_t duration_ms) {
    if (!initialized) {
        Serial.println("[AUDIO][ERROR] Not initialized");
        return false;
    }

    audio_size = (SAMPLE_RATE * duration_ms / 1000) * 2; // 16-bit mono
    Serial.printf("[AUDIO] Recording %lu ms (%u bytes)\n",
                  duration_ms, audio_size);

    audio_buffer = (uint8_t *)heap_caps_malloc(
        audio_size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!audio_buffer) {
        Serial.println("[AUDIO][ERROR] PSRAM allocation failed");
        return false;
    }

    size_t offset = 0;
    size_t read_bytes = 0;
    uint8_t temp[BUFFER_SIZE];

    while (offset < audio_size) {
        size_t to_read = min((size_t)BUFFER_SIZE, audio_size - offset);

        esp_err_t ret = i2s_channel_read(
            rx_chan,
            temp,
            to_read,
            &read_bytes,
            portMAX_DELAY
        );

        if (ret != ESP_OK) {
            Serial.printf("[AUDIO][ERROR] Read failed: %s\n",
                          esp_err_to_name(ret));
            return false;
        }

        /* ---------- DIGITAL GAIN ---------- */
        int16_t *samples = (int16_t *)temp;
        int sample_count = read_bytes / 2;

        for (int i = 0; i < sample_count; i++) {
            int32_t amplified = samples[i] * MIC_GAIN;
            if (amplified > 32767) amplified = 32767;
            if (amplified < -32768) amplified = -32768;
            samples[i] = (int16_t)amplified;
        }
        /* ---------------------------------- */

        memcpy(audio_buffer + offset, temp, read_bytes);
        offset += read_bytes;
    }

    Serial.printf("[AUDIO] Recording complete (%u bytes)\n", offset);
    return true;
}

void AudioCapture::stop() {
    if (rx_chan) {
        i2s_channel_disable(rx_chan);
    }
}

uint8_t* AudioCapture::getBuffer() const {
    return audio_buffer;
}

size_t AudioCapture::getSize() const {
    return audio_size;
}

void AudioCapture::applyGain(uint8_t gain) {
    if (!audio_buffer) return;

    int16_t *samples = (int16_t *)audio_buffer;
    int sample_count = audio_size / 2;

    for (int i = 0; i < sample_count; i++) {
        int32_t amplified = samples[i] * gain;
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        samples[i] = (int16_t)amplified;
    }

    Serial.printf("[AUDIO] Applied gain: %d\n", gain);
}

void AudioCapture::freeBuffer() {
    if (audio_buffer) {
        free(audio_buffer);
        audio_buffer = nullptr;
        audio_size = 0;
    }
}
