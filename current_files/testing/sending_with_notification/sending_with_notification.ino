#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "driver/i2s_pdm.h"
#include <Firebase_ESP_Client.h>
#include <time.h>
#include "esp_heap_caps.h"

/* ================= WIFI ================= */
#define WIFI_SSID "Mahmuds_iphone"
#define WIFI_PASS "mahmudja"

/* ================= FIREBASE ================= */
#define API_KEY "AIzaSyDfyfrXwwzTDYPMcO4KyfBoO2ySoFe_lgY"
#define STORAGE_BUCKET_ID "hearme-a5f10.firebasestorage.app"
#define FIRESTORE_PROJECT_ID "hearme-a5f10"
#define DATABASE_URL "https://hearme-a5f10-default-rtdb.europe-west1.firebasedatabase.app/"



#define USER_EMAIL "admin@admin.com"
#define USER_PASSWORD "admin1"

/* ================= MENTEE CONFIG ================= */
#define MENTEE_NUMBER 2761  // Your mentee number

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

/* ================= CAMERA PINS ================= */
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

/* ================= AUDIO ================= */
#define SAMPLE_RATE     16000
#define RECORD_SECONDS  5
#define BUFFER_SIZE     1024
#define PDM_CLK         42
#define PDM_DATA        41

i2s_chan_handle_t rx_chan;

/* ================= AUDIO BUFFER ================= */
static uint8_t *audio_buffer = nullptr;
static size_t audio_size = 0;

/* ================================================= */

void setupCamera() {
  Serial.println("[CAM] Initializing camera...");

  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sscb_sda = SIOD_GPIO_NUM;
  c.pin_sscb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&c) != ESP_OK) {
    Serial.println("[CAM][ERROR] Camera init failed!");
    while (true);
  }

  Serial.println("[CAM] Camera ready");
}

void setupMic() {
  Serial.println("[MIC] Initializing PDM microphone...");

  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

  i2s_pdm_rx_config_t pdm_cfg = {
    .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .clk = (gpio_num_t)PDM_CLK,
      .din = (gpio_num_t)PDM_DATA,
      .invert_flags = { .clk_inv = false },
    },
  };

  ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

  Serial.println("[MIC] Microphone ready");
}

void recordAudio() {
  Serial.printf("[MIC] Recording audio (%d seconds)...\n", RECORD_SECONDS);

  audio_size = SAMPLE_RATE * RECORD_SECONDS * 2;

  audio_buffer = (uint8_t *)heap_caps_malloc(
      audio_size,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );

  if (!audio_buffer) {
    Serial.println("[MIC][ERROR] Audio buffer allocation failed");
    return;
  }

  size_t read_bytes = 0;
  size_t offset = 0;
  uint8_t temp[BUFFER_SIZE];

  while (offset < audio_size) {
    size_t to_read = min((size_t)BUFFER_SIZE, audio_size - offset);

    ESP_ERROR_CHECK(
      i2s_channel_read(rx_chan, temp, to_read, &read_bytes, portMAX_DELAY)
    );
    memcpy(audio_buffer + offset, temp, read_bytes);
    offset += read_bytes;
  }

  Serial.printf("[MIC] Recording complete (%d bytes)\n", audio_size);
}
String getMentorIdFromRTDB(int menteeNumber) {
  String path = "/mentees/" + String(menteeNumber) + "/mentorId";

  Serial.println("[RTDB] Reading mentorId from: " + path);

  if (Firebase.RTDB.getString(&fbdo, path.c_str())) {
    String mentorId = fbdo.stringData();
    Serial.println("[RTDB] ✅ mentorId = " + mentorId);
    return mentorId;
  } else {
    Serial.println("[RTDB][ERROR] " + fbdo.errorReason());
    return "";
  }
}



void notifyMentorOfDetection(int menteeNumber, const String &imageUrl, const String &audioUrl, const String &detectionTime) {
  Serial.printf("[NOTIFY] Creating notification for menteeNumber: %d\n", menteeNumber);
  
  String mentorId = getMentorIdFromRTDB(menteeNumber);

  if (mentorId.isEmpty()) {
    Serial.println("[NOTIFY][ERROR] No mentor ID found, skipping notification");
    return;
  }

  FirebaseJson json;
  json.set("fields/menteeNumber/integerValue", String(menteeNumber));
  json.set("fields/mentorId/stringValue", mentorId);
  json.set("fields/imagePath/stringValue", imageUrl);
  json.set("fields/audioPath/stringValue", audioUrl);
  json.set("fields/timestamp/stringValue", detectionTime);
  json.set("fields/title/stringValue", "New Detection - Mentee Alert");
  json.set("fields/message/stringValue", "Your mentee has triggered a detection. Tap to view.");
  json.set("fields/status/stringValue", "pending");
  json.set("fields/type/stringValue", "mentor_alert");

  String jsonStr;
  json.toString(jsonStr, true);
  
  String notificationDocId = "notification_" + detectionTime;

  if (!Firebase.Firestore.createDocument(
        &fbdo,
        FIRESTORE_PROJECT_ID,
        "",
        "notifications",
        notificationDocId.c_str(),
        jsonStr.c_str(),
        "")) {
    Serial.println("[NOTIFY][ERROR] " + fbdo.errorReason());
    return;
  }

  Serial.printf("[NOTIFY] ✅ Notification created for mentor: %s\n", mentorId.c_str());
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] ESP32-S3 starting...");

  Serial.print("[WIFI] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected! IP: " + WiFi.localIP().toString());

  setupCamera();
  setupMic();

  Serial.println("[FIREBASE] Signing in...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[FIREBASE] Ready");

  configTime(0, 0, "pool.ntp.org");
}

void loop() {
  struct tm t;
  if (!getLocalTime(&t)) {
    Serial.println("[TIME][ERROR] Failed to get time");
    return;
  }

  char ts[30];
  strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &t);
  String detectID = "detection_" + String(ts);

  Serial.println("\n[DETECTION] Starting new detection: " + detectID);

  /* ---------- IMAGE ---------- */
  Serial.println("[CAM] Capturing image...");
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM][ERROR] Capture failed");
    return;
  }

  String imgPath = "/detections/" + detectID + "/image.jpg";
  Serial.println("[UPLOAD] Uploading image...");

  if (!Firebase.Storage.upload(
        &fbdo,
        STORAGE_BUCKET_ID,
        fb->buf,
        fb->len,
        imgPath.c_str(),
        "image/jpeg")) {
    Serial.println("[UPLOAD][ERROR] " + fbdo.errorReason());
  } else {
    Serial.println("[UPLOAD] Image upload complete");
  }

  esp_camera_fb_return(fb);

  /* ---------- AUDIO ---------- */
  recordAudio();

  String audPath = "/detections/" + detectID + "/audio.raw";
  Serial.println("[UPLOAD] Uploading audio...");

  if (!Firebase.Storage.upload(
        &fbdo,
        STORAGE_BUCKET_ID,
        audio_buffer,
        audio_size,
        audPath.c_str(),
        "audio/pcm")) {
    Serial.println("[UPLOAD][ERROR] " + fbdo.errorReason());
  } else {
    Serial.println("[UPLOAD] Audio upload complete");
  }

  free(audio_buffer);
  audio_buffer = nullptr;

  /* ---------- FIRESTORE DETECTION DOCUMENT ---------- */
  Serial.println("[FIRESTORE] Writing detection document...");

  FirebaseJson json;
  json.set("fields/menteeNumber/integerValue", String(MENTEE_NUMBER));
  json.set("fields/image/stringValue", imgPath);
  json.set("fields/audio/stringValue", audPath);
  json.set("fields/timestamp/stringValue", String(ts));
  json.set("fields/message/stringValue", "New detection available – tap to view");

  String jsonStr;
  json.toString(jsonStr, true);

  if (!Firebase.Firestore.createDocument(
        &fbdo,
        FIRESTORE_PROJECT_ID,
        "",
        "detections",
        detectID.c_str(),
        jsonStr.c_str(),
        "")) {
    Serial.println("[FIRESTORE][ERROR] " + fbdo.errorReason());
  } else {
    Serial.println("[FIRESTORE] Detection document created");
  }

  /* ---------- NOTIFY MENTOR ---------- */
  notifyMentorOfDetection(MENTEE_NUMBER, imgPath, audPath, String(ts));

  Serial.println("[DONE] Detection and notification complete");

  while (true); // run once
}