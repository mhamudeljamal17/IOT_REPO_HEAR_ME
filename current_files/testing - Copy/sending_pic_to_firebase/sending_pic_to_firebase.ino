#include <WiFi.h>
#include "esp_camera.h"

// Mobizt Firebase library
#include <Firebase_ESP_Client.h>
#define USER_EMAIL "admin@admin.com"
#define USER_PASSWORD "admin1"

// ---------- WiFi and Firebase config ----------
#define WIFI_SSID "Mahmuds_iphone"
#define WIFI_PASS "mahmudja"

#define API_KEY "AIzaSyDfyfrXwwzTDYPMcO4KyfBoO2ySoFe_lgY"
#define FIRESTORE_PROJECT_ID "hearme-a5f10"
#define STORAGE_BUCKET_ID "hearme-a5f10.firebasestorage.app"

// ---------- Camera pins for Xiao ESP32-S3 Sense ----------
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

// ---------- Firebase objects ----------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // ---------- Camera config ----------
  camera_config_t camera_config;
  camera_config.ledc_channel = LEDC_CHANNEL_0;
  camera_config.ledc_timer   = LEDC_TIMER_0;
  camera_config.pin_d0 = Y2_GPIO_NUM; camera_config.pin_d1 = Y3_GPIO_NUM;
  camera_config.pin_d2 = Y4_GPIO_NUM; camera_config.pin_d3 = Y5_GPIO_NUM;
  camera_config.pin_d4 = Y6_GPIO_NUM; camera_config.pin_d5 = Y7_GPIO_NUM;
  camera_config.pin_d6 = Y8_GPIO_NUM; camera_config.pin_d7 = Y9_GPIO_NUM;
  camera_config.pin_xclk = XCLK_GPIO_NUM;
  camera_config.pin_pclk = PCLK_GPIO_NUM;
  camera_config.pin_vsync = VSYNC_GPIO_NUM;
  camera_config.pin_href = HREF_GPIO_NUM;
  camera_config.pin_sscb_sda = SIOD_GPIO_NUM;
  camera_config.pin_sscb_scl = SIOC_GPIO_NUM;
  camera_config.pin_pwdn = PWDN_GPIO_NUM;
  camera_config.pin_reset = RESET_GPIO_NUM;
  camera_config.xclk_freq_hz = 20000000;
  camera_config.pixel_format = PIXFORMAT_JPEG;
  camera_config.frame_size   = FRAMESIZE_QVGA;
  camera_config.jpeg_quality = 12;
  camera_config.fb_count     = 2;

  if (esp_camera_init(&camera_config) != ESP_OK) {
    Serial.println("Camera init failed!");
    while (true);
  }

  // ---------- Firebase config ----------
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = ""; // Not needed for Firestore
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("READY to capture!");
}

// ---------- Capture and upload ----------
void captureAndUpload() {
  // ---------- Get current date & time ----------
  configTime(0, 0, "pool.ntp.org"); // UTC
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char dateTime[30];
  strftime(dateTime, sizeof(dateTime), "%Y%m%d_%H%M%S", &timeinfo);
  String timestamp = String(dateTime);

  // ---------- Capture image ----------
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed!");
    return;
  }

  // ---------- Use timestamp in storage path ----------
  String storage_path = "/images/image_" + timestamp + ".jpg";

  // Upload image to Firebase Storage
  if (!Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, fb->buf, fb->len, storage_path.c_str(), "image/jpeg")) {
    Serial.println("Storage upload failed: " + fbdo.errorReason());
    esp_camera_fb_return(fb);
    return;
  }

  esp_camera_fb_return(fb);

  // Construct public download URL
  String url = "https://firebasestorage.googleapis.com/v0/b/" + String(STORAGE_BUCKET_ID) +
               "/o/images%2Fimage_" + timestamp + ".jpg?alt=media";
  Serial.println("Download URL: " + url);

  // Save URL and timestamp to Firestore
  char readableTime[30];
  strftime(readableTime, sizeof(readableTime), "%Y-%m-%d %H:%M:%S", &timeinfo);
  String currentTime = String(readableTime);

  String doc_path = "image_" + timestamp; // unique document for each image
  FirebaseJson json;
  json.set("fields/url/stringValue", url);
  json.set("fields/datetime/stringValue", currentTime);

  String jsonStr;
  json.toString(jsonStr, true);

  if (!Firebase.Firestore.createDocument(&fbdo, FIRESTORE_PROJECT_ID, "", doc_path.c_str(), jsonStr.c_str())) {
    Serial.println("Firestore write failed: " + fbdo.errorReason());
    return;
  }

  Serial.println("Image uploaded successfully and URL + timestamp saved to Firestore!");
}

void loop() {
  static bool done = false;
  if (!done) {
    captureAndUpload();
    done = true;  // Only capture once
  }
}
