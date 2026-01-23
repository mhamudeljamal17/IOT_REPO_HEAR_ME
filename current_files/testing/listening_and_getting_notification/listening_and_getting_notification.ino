#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

/* ===== AUDIO ===== */
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioOutputI2S.h>

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

/* ================= AUDIO OBJECTS ================= */
AudioGeneratorWAV *wav = nullptr;
AudioFileSourceHTTPStream *file = nullptr; // fallback to HTTP stream
AudioOutputI2S *out = nullptr;

bool audioPlaying = false;

/* ===================================================== */

void playAudioFromUrl(const String &url) {
  Serial.println("[AUDIO] Starting playback...");
  Serial.println("[AUDIO] URL: " + url);

  // Use HTTPSStream if available:
  // file = new AudioFileSourceHTTPSStream(url.c_str());
  file = new AudioFileSourceHTTPStream(url.c_str());

  out  = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(0.9);

  wav = new AudioGeneratorWAV();

  if (!wav->begin(file, out)) {
    Serial.println("[AUDIO][ERROR] WAV begin failed");
    delete wav;
    delete file;
    delete out;
    return;
  }

  audioPlaying = true;
  while (wav->isRunning()) {
    wav->loop();
    delay(1);
  }
  wav->stop();

  delete wav;
  delete file;
  delete out;

  wav = nullptr;
  file = nullptr;
  out  = nullptr;

  Serial.println("[AUDIO] Playback finished");
  audioPlaying = false;
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

  if (json->get(result, "status")) status = result.stringValue;
  if (json->get(result, "audioUrl")) audioUrl = result.stringValue;

  Serial.println("[RTDB] status = " + status);
  Serial.println("[RTDB] audioUrl = " + audioUrl);

  if (status == "new" && !audioPlaying && audioUrl.length() > 10) {
    Serial.println("[RTDB] 🔊 New audio command detected");
    playAudioFromUrl(audioUrl);

    // ACK
    Firebase.RTDB.setString(&fbdo, ESP_COMMAND_PATH "/status", "done");
    Serial.println("[RTDB] ✅ ACK sent (status=done)");
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

  /* ---- WiFi ---- */
  Serial.print("[WIFI] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected");
  Serial.println("[WIFI] IP: " + WiFi.localIP().toString());

  /* ---- Firebase ---- */
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[FIREBASE] Connected");

  /* ---- Start RTDB stream ---- */
  if (!Firebase.RTDB.beginStream(&fbdo, ESP_COMMAND_PATH)) {
    Serial.println("[RTDB][ERROR] Stream begin failed");
    Serial.println(fbdo.errorReason());
  }

  Firebase.RTDB.setStreamCallback(
    &fbdo,
    streamCallback,
    streamTimeoutCallback
  );

  Serial.println("[RTDB] Listening for ESP commands...");
}

/* ================= LOOP ================= */
void loop() {
  // Firebase stream runs asynchronously.
  delay(1000);
}
