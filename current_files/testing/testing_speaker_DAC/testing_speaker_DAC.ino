#include <WiFi.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// ---- WiFi ----
const char* ssid     = "Mahmuds_iphone";
const char* password = "mahmudja";

// ---- Audio objects ----
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioOutputI2S *out;

// Plain HTTP MP3 stream
const char* streamUrl = "http://ice1.somafm.com/u80s-128-mp3";

void setup() {
  Serial.begin(115200);
  delay(500);

  // ----- WiFi -----
  Serial.println("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // ----- I2S output → match your MAX98357A pins -----
  out = new AudioOutputI2S();
  out->SetPinout(0, 1, 2);    // BCLK, LRC, DIN
  out->SetGain(0.7);             // 0.0–1.0

  // ----- Start MP3 stream -----
  Serial.println("Connecting to stream...");
  file = new AudioFileSourceICYStream(streamUrl);

  mp3 = new AudioGeneratorMP3();
  if (!mp3->begin(file, out)) {
    Serial.println("mp3->begin FAILED");
  } else {
    Serial.println("Started MP3 stream.");
  }
}

void loop() {
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      Serial.println("Stream ended or error, stopping.");
      mp3->stop();
      delay(2000);
    }
  } else {
    delay(1000);
  }
}
