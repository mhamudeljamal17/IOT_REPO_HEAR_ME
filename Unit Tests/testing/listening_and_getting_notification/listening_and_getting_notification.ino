#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <queue>

/* ===== AUDIO (ESP32-audioI2S) ===== */
#include "Audio.h"

/* ================= WIFI ================= */
#define WIFI_SSID "Mahmuds_iphone"
#define WIFI_PASS "mahmudja"

/* ================= FIREBASE ================= */
#define API_KEY "AIzaSyDfyfrXwwzTDYPMcO4KyfBoO2ySoFe_lgY"
#define DATABASE_URL "https://hearme-a5f10-default-rtdb.europe-west1.firebasedatabase.app/"
#define USER_EMAIL "admin@admin.com"
#define USER_PASSWORD "admin1"

/* ================= ESP CONFIG ================= */
#define MENTEE_NUMBER 2761
#define ESP_COMMAND_PATH "/esp_commands/2761"

/* ================= I2S (DAC) ================= */
#define I2S_BCLK  7
#define I2S_LRC   8
#define I2S_DOUT  9

/* ================= FIREBASE OBJECTS ================= */
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

/* ================= AUDIO OBJECT ================= */
Audio audio;
bool audioPlaying = false;

/* ================= AUDIO QUEUE ================= */
std::queue<String> audioQueue;

/* ===================================================== */
/* AUDIO CALLBACKS */
void audio_info(const char *info) {
  Serial.print("[AUDIO] ");
  Serial.println(info);
}

// Called when MP3 finishes
void audio_eof_mp3(const char *info) {
  Serial.println("[AUDIO] MP3 finished");
  audioPlaying = false;
  Serial.println("[AUDIO] ✅ Ready for next audio");
  Firebase.RTDB.setString(&fbdo, ESP_COMMAND_PATH "/status", "done");
}

// Called when WAV finishes
void audio_eof_wav(const char *info) {
  Serial.println("[AUDIO] WAV finished");
  audioPlaying = false;
  Serial.println("[AUDIO] ✅ Ready for next audio");
  Firebase.RTDB.setString(&fbdo, ESP_COMMAND_PATH "/status", "done");
}

/* ================= PLAY NEXT AUDIO ================= */
void playNextAudio() {
  if (audioPlaying || audioQueue.empty()) return;

  String url = audioQueue.front();
  audioQueue.pop();

  Serial.println("[AUDIO] Starting HTTPS playback");
  Serial.println("[AUDIO] URL: " + url);

  // Stop previous audio just in case
  if (audio.isRunning()) audio.stopSong();
  delay(50);

  audioPlaying = true;
  if (!audio.connecttohost(url.c_str())) {
    Serial.println("[AUDIO][ERROR] Failed to start audio");
    audioPlaying = false;
  }
}

/* ================= RTDB STREAM CALLBACK ================= */
void streamCallback(FirebaseStream data) {
  Serial.println("\n[RTDB] Data received");

  if (data.dataType() != "json") {
    Serial.println("[RTDB] Not JSON, ignored");
    return;
  }

  FirebaseJson *json = data.to<FirebaseJson*>();
  FirebaseJsonData result;

  String status;
  String audioUrl;

  if (json->get(result, "status"))   status   = result.stringValue;
  if (json->get(result, "audioUrl")) audioUrl = result.stringValue;

  Serial.println("[RTDB] status   = " + status);
  Serial.println("[RTDB] audioUrl = " + audioUrl);

  // ✅ Only handle new audio commands
  if (status == "new" && audioUrl.length() > 10) {
    Serial.println("[RTDB] 🔊 New audio command detected, adding to queue");

    // Add to queue
    audioQueue.push(audioUrl);

    // ✅ Immediately update status to "old" so it won't trigger again
    if (Firebase.RTDB.setString(&fbdo, ESP_COMMAND_PATH "/status", "old")) {
      Serial.println("[RTDB] Status updated to 'old'");
    } else {
      Serial.println("[RTDB][ERROR] Failed to update status: " + fbdo.errorReason());
    }
  }
}


void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("[RTDB][WARN] Stream timeout, reconnecting...");
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected");

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(15); // 0..21

  
  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.RTDB.beginStream(&fbdo, ESP_COMMAND_PATH)) {
    Serial.println("[RTDB][ERROR] Stream begin failed");
    Serial.println(fbdo.errorReason());
  }
  Firebase.RTDB.setStreamCallback(&fbdo, streamCallback, streamTimeoutCallback);
  Serial.println("[RTDB] Listening for ESP commands...");
}

/* ================= LOOP ================= */
void loop() {
  audio.loop();        // Must be called frequently

  // Check if audio finished
  if (audioPlaying && !audio.isRunning()) {
    audioPlaying = false;
    Serial.println("[AUDIO] Playback finished, ready for next audio");
    Firebase.RTDB.setString(&fbdo, ESP_COMMAND_PATH "/status", "done");
  }

  // Play next in queue if available
  playNextAudio();

  delay(2);
}



// }
// #include <Arduino.h>
// #include <WiFi.h>
// #include "Audio.h"

// /* ================= WIFI ================= */
// #define WIFI_SSID "Mahmuds_iphone"
// #define WIFI_PASS "mahmudja"

// /* ================= I2S DAC (SAME AS YOUR PROJECT) ================= */
// #define I2S_BCLK  7
// #define I2S_LRC   8
// #define I2S_DOUT  9

// /* ================= AUDIO ================= */
// Audio audio;

// /* ===================================================== */

// void audio_info(const char *info) {
//   Serial.print("[AUDIO] ");
//   Serial.println(info);
// }

// void audio_eof_mp3(const char *info) {
//   Serial.println("[AUDIO] Playback finished");
// }

// /* ===================================================== */

// void setup() {
//   Serial.begin(115200);
//   delay(500);

//   /* ---- WiFi ---- */
//   Serial.print("[WIFI] Connecting");
//   WiFi.begin(WIFI_SSID, WIFI_PASS);
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(300);
//     Serial.print(".");
//   }
//   Serial.println("\n[WIFI] Connected");
//   Serial.println("[WIFI] IP: " + WiFi.localIP().toString());

//   /* ---- Audio ---- */
//   audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
//   audio.setVolume(18);               // 0–21

//   Serial.println("[AUDIO] Starting HTTPS audio test...");

//   /* ---- TEST HTTPS AUDIO URL ---- */
//   audio.connecttohost(
//     //"https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3"
//     "https://firebasestorage.googleapis.com/v0/b/hearme-a5f10.firebasestorage.app/o/voice_recordings%2FrGO8rDnkl5YHgHHUfTAx%2Fmentee_2761_1769140907204.wav?alt=media&token=ec0865d3-b00b-4d6e-b155-0ea93c7bb5ca"
//   );
// }

// /* ===================================================== */

// void loop() {
//   audio.loop();   // MUST be called frequently
//   delay(2);
// }
