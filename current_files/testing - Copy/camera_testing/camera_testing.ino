#include "esp_camera.h"

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sscb_sda = SIOD_GPIO_NUM; c.pin_sscb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;

  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size   = FRAMESIZE_QVGA;  // 320x240 → good for PSRAM
  c.jpeg_quality = 12;              // smaller number = better quality
  c.fb_count     = 2;               // needs PSRAM
  c.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  if (esp_camera_init(&c) != ESP_OK) {
    Serial.println("ERR Camera init failed");
    while (true);
  }

  Serial.println("READY");
}

void loop() {
  if (!Serial.available()) return;
  char ch = Serial.read();
  if (ch != 'c') return;

  camera_fb_t *fb = nullptr;
  for (int i = 0; i < 5; i++) {
    fb = esp_camera_fb_get();
    if (fb) break;
    delay(200);
  }

  if (!fb) {
    Serial.println("ERR FB NULL");
    return;
  }

  Serial.printf("JPG %u\n", (unsigned)fb->len);

  const uint8_t *p = fb->buf;
  size_t left = fb->len;
  while (left > 0) {
    size_t n = (left > 1024) ? 1024 : left;
    Serial.write(p, n);
    p += n;
    left -= n;
    delay(1);  // very important: avoids stack/USB overflow
  }

  esp_camera_fb_return(fb);
  Serial.println("\nDONE");
}
