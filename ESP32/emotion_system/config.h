#ifndef CONFIG_H
#define CONFIG_H

/* ============== WIFI CONFIGURATION ============== */
#define WIFI_SSID "Mahmuds_iphone"
#define WIFI_PASS "mahmudja"

/* ============== FIREBASE CONFIGURATION ============== */
#define API_KEY "AIzaSyDfyfrXwwzTDYPMcO4KyfBoO2ySoFe_lgY"
#define STORAGE_BUCKET_ID "hearme-a5f10.firebasestorage.app"
#define FIRESTORE_PROJECT_ID "hearme-a5f10"
#define DATABASE_URL "https://hearme-a5f10-default-rtdb.europe-west1.firebasedatabase.app/"
#define USER_EMAIL "admin@admin.com"
#define USER_PASSWORD "admin1"

/* ============== MENTEE CONFIGURATION ============== */

#define MENTEE_NUMBER 2761
#define ESP_COMMAND_PATH "/esp_commands/2761"

/* ============== AUDIO RECORDING CONFIGURATION ============== */
#define SAMPLE_RATE 16000
#define RECORD_SECONDS 2
#define BUFFER_SIZE 1024
#define AUDIO_BUFFER_SIZE (SAMPLE_RATE * RECORD_SECONDS * 2)  // 16-bit mono

/* ============== I2S PDM MICROPHONE PINS (IDF 4.x) ============== */
#define I2S_PORT I2S_NUM_0
#define I2S_MIC_SERIAL_CLOCK GPIO_NUM_42    // BCLK
#define I2S_MIC_LEFT_RIGHT_CLOCK GPIO_NUM_1  // LRC (not used in PDM)
#define I2S_MIC_SERIAL_DATA GPIO_NUM_41     // DIN

/* ============== I2S DAC (Speaker) PINS ============== */
#define I2S_SPEAKER_PORT I2S_NUM_1
#define I2S_SPEAKER_BCLK GPIO_NUM_7
#define I2S_SPEAKER_LRC GPIO_NUM_8
#define I2S_SPEAKER_DOUT GPIO_NUM_9

/* ============== CAMERA CONFIGURATION ============== */
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39
#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13

/* ============== AUDIO PROCESSING CONFIGURATION ============== */
#define MIC_GAIN 3  // 2 = safe, 3 = good, 4 = louder
#define ANGER_DETECTION_THRESHOLD 0.5f
#define RECORDING_TIMEOUT_MS 10000  // 10 seconds max per recording
#define NOTIFICATION_TIMEOUT_MS 60000  // 1 minute to wait for response


/* ================= I2S (DAC) ================= */
#define I2S_BCLK  7
#define I2S_LRC   8
#define I2S_DOUT  9


/* ============== SYSTEM STATES ============== */
enum SystemState {
  STATE_IDLE,
  STATE_RECORDING,
  STATE_DETECTING,
  STATE_UPLOADING,
  STATE_LISTENING_FOR_NOTIFICATION,
  STATE_PLAYING_AUDIO
};

#endif