
// I2S configuration
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26


// I2S configuration
i2s_config_t i2s_default_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = 22050,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 8,
  .dma_buf_len = 64,
  .use_apll = false,
  .tx_desc_auto_clear = true,
  .fixed_mclk = 0
};

// I2S pin configuration
i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
};