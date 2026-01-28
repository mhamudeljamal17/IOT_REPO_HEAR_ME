#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_camera.h"
#include <Firebase_ESP_Client.h>
#include <esp_heap_caps.h>
#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "AngerDetector.h"
#include "FirebaseManager.h"
#include "NotificationListener.h"

/* ============== LED & BUTTONS ============== */
#define PIXEL_PIN 3
#define PIXEL_COUNT 5
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

#define BUTTON_PIN 4
#define HELP_BUTTON_PIN 5

volatile bool helpButtonPressed = false;
unsigned long lastHelpRequestTime = 0;
const unsigned long HELP_COOLDOWN = 5000;

/* ============== GLOBAL OBJECTS ============== */
AngerDetector *angerDetector = nullptr;
FirebaseManager *firebaseManager = nullptr;
NotificationListener *notificationListener = nullptr;

SystemState currentState = STATE_IDLE;

/* ============== INTERRUPT ============== */
void IRAM_ATTR helpButtonISR() {
    helpButtonPressed = true;
}

/* ============== UTILITIES ============== */
String getTimestamp() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &t);
    return String(buf);
}

/* ============== SERIAL AUDIO RECEIVE ============== */
uint8_t* receiveAudioFromSerial(size_t &outSize) {
    Serial.println("[SERIAL] Waiting for START");

    while (true) {
        if (Serial.available()) {
            String line = Serial.readStringUntil('\n');
            line.trim();
            if (line == "START") break;
        }
        delay(10);
    }

    while (Serial.available() < 4) delay(1);

    uint32_t audioSize;
    Serial.readBytes((uint8_t*)&audioSize, 4);

    Serial.printf("[SERIAL] Receiving %lu bytes\n", audioSize);

    uint8_t *buffer = (uint8_t*)heap_caps_malloc(
        audioSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!buffer) {
        Serial.println("[ERROR] PSRAM alloc failed");
        return nullptr;
    }

    size_t received = 0;
    while (received < audioSize) {
        received += Serial.readBytes(
            buffer + received,
            audioSize - received
        );
    }

    while (true) {
        if (Serial.available()) {
            String line = Serial.readStringUntil('\n');
            line.trim();
            if (line == "END") break;
        }
        delay(1);
    }

    outSize = audioSize;
    Serial.println("[SERIAL] Audio received OK");
    return buffer;
}

/* ============== SETUP ============== */
void setup() {
    Serial.begin(115200);
    delay(1500);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(HELP_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(HELP_BUTTON_PIN),
        helpButtonISR,
        FALLING
    );

    strip.begin();
    strip.show();

    angerDetector = new AngerDetector();
    firebaseManager = new FirebaseManager();
    notificationListener = new NotificationListener();

    setupWiFi();
    setupNTP();

    if (!angerDetector->init(&strip)) while (true);
    if (!firebaseManager->init()) while (true);

    setupCamera();

    Serial.println("[SYSTEM] Ready for SERIAL audio input");
}

/* ============== MAIN LOGIC ============== */
void loop() {
    if (helpButtonPressed) {
        helpButtonPressed = false;
        firebaseManager->sendHelpRequest(MENTEE_NUMBER, getTimestamp());
    }

    size_t audioSize = 0;
    uint8_t *audioBuffer = receiveAudioFromSerial(audioSize);
    if (!audioBuffer) return;

    int anger = angerDetector->detectAnger(audioBuffer, audioSize);

    if (anger <= 0) {
        Serial.println("[SYSTEM] Not angry");
        free(audioBuffer);
        return;
    }

    Serial.println("[SYSTEM] 🔴 ANGER DETECTED");

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        free(audioBuffer);
        return;
    }

    String detectID = "detection_" + getTimestamp();

    firebaseManager->uploadImage(detectID, fb->buf, fb->len);
    firebaseManager->uploadAudio(detectID, audioBuffer, audioSize);
    firebaseManager->createDetectionDocument(
        detectID,
        "/image.jpg",
        "/audio.wav",
        getTimestamp()
    );

    esp_camera_fb_return(fb);
    free(audioBuffer);

    Serial.println("[SYSTEM] Detection complete\n");
}
