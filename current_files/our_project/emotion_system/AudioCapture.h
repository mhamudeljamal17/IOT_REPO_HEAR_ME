#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <stdint.h>
#include <driver/i2s_pdm.h>

class AudioCapture {
private:
    uint8_t *audio_buffer;
    size_t audio_size;
    bool initialized;

    i2s_chan_handle_t rx_chan;

public:
    AudioCapture();
    ~AudioCapture();

    bool init();
    bool startRecording(uint32_t duration_ms);
    void stop();

    uint8_t* getBuffer() const;
    size_t getSize() const;

    void applyGain(uint8_t gain);
    void freeBuffer();
};

#endif // AUDIO_CAPTURE_H
