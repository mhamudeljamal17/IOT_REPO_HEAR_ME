#include <Chirale_TensorFlowLite.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_camera.h"
#include <Firebase_ESP_Client.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "AudioCapture.h"
#include "AngerDetector.h"
#include "FirebaseManager.h"
#include "NotificationListener.h"

#define PIXEL_PIN    3
#define PIXEL_COUNT  5
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

#define BUTTON_PIN 4       // Cancel button
#define HELP_BUTTON_PIN 5  // Help button

// Cancel button variables
bool cancelRequested = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 30;

// ✅ Help button interrupt variables
volatile bool helpButtonPressed = false;
unsigned long lastHelpRequestTime = 0;
const unsigned long HELP_COOLDOWN = 5000; // 5 second cooldown between help requests

int counter_sd_files=0;

/* ============== GLOBAL OBJECTS ============== */
AudioCapture *audioCapture = nullptr;
AngerDetector *angerDetector = nullptr;
FirebaseManager *firebaseManager = nullptr;
NotificationListener *notificationListener = nullptr;

SystemState currentState = STATE_IDLE;

/* ============== WAV HEADER STRUCTURE ============== */
struct WAVHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t fileSize;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;      // PCM
    uint16_t numChannels = 1;      // mono
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize;
};

#define SD_CS 21

/* ============== FUNCTION DECLARATIONS ============== */
void setupWiFi();
void setupCamera();
void setupNTP();
String getTimestamp();
void printSystemStatus();
void systemLoop();
bool checkCancelButtonPressed();
void sendHelpNotification();
void IRAM_ATTR helpButtonISR();  // ✅ Interrupt handler

/* ============== INTERRUPT HANDLER ============== */
// ✅ This function runs immediately when button is pressed
void IRAM_ATTR helpButtonISR() {
    // Set flag - keep ISR short and fast
    helpButtonPressed = true;
}

/* ============== SETUP ============== */
void setup() {
    Serial.begin(115200);
    delay(1500);
    
    Serial.println("\n\n===============================================");
    Serial.println("    HEAR ME - Emotion Detection System");
    Serial.println("    ESP32-S3 Xiao Sense");
    Serial.println("===============================================\n");
    
    // ✅ Setup buttons FIRST (before anything else)
    pinMode(BUTTON_PIN, INPUT_PULLUP);      // Cancel button
    pinMode(HELP_BUTTON_PIN, INPUT_PULLUP); // Help button
    
    // ✅ Attach interrupt to help button (FALLING = HIGH to LOW transition)
    attachInterrupt(digitalPinToInterrupt(HELP_BUTTON_PIN), helpButtonISR, FALLING);
    
    Serial.println("[SETUP] Buttons configured:");
    Serial.printf("  - Cancel button: Pin %d (polled)\n", BUTTON_PIN);
    Serial.printf("  - Help button: Pin %d (interrupt-driven)\n", HELP_BUTTON_PIN);
    

    
    // Initialize objects
    audioCapture = new AudioCapture();
    angerDetector = new AngerDetector();
    firebaseManager = new FirebaseManager();
    notificationListener = new NotificationListener();
    strip.begin();
    strip.show(); 
    
    // Setup WiFi
    Serial.println("[SETUP] Connecting to WiFi...");
    setupWiFi();
    
    // Setup NTP for timestamps
    Serial.println("[SETUP] Setting up NTP...");
    setupNTP();
    
    // Initialize modules
    Serial.println("[SETUP] Initializing modules...");
    
    if (!audioCapture->init()) {
        Serial.println("[SETUP][ERROR] Failed to initialize audio capture");
        while (true);
    }
    
    if (!angerDetector->init(&strip)) {
        Serial.println("[SETUP][ERROR] Failed to initialize anger detector");
        while (true);
    }
    
    if (!firebaseManager->init()) {
        Serial.println("[SETUP][ERROR] Failed to initialize Firebase");
        while (true);
    }

    if (!SD.begin(SD_CS)) {
        Serial.println("SD init failed!");
        while (1);
    }

    // Setup camera
    Serial.println("[SETUP] Initializing camera...");
    setupCamera();

    Serial.println("\n[SETUP] System initialized successfully!");
    Serial.println("[SETUP] Ready to detect emotions...\n");
    Serial.println("[SETUP] 🆘 Press HELP button ANYTIME to request assistance\n");
    
    currentState = STATE_IDLE;
}

/* ============== SETUP FUNCTIONS ============== */
void setupWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int attempts = 0;
    
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected!");
        Serial.print("[WIFI] IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n[WIFI][ERROR] Failed to connect");
    }
}

void setupCamera() {
    Serial.println("[CAM] Initializing OV2640 camera...");
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    
    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("[CAM][ERROR] Camera init failed!");
        while (true);
    }
    
    Serial.println("[CAM] Camera initialized");
}

void setupNTP() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[NTP] Waiting for NTP time sync...");
    
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < 24 * 3600 && attempts < 30) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        attempts++;
    }
    Serial.println();
    
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.print("[NTP] Current time: ");
    Serial.println(asctime(&timeinfo));
}

String getTimestamp() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    
    char ts[30];
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &t);
    return String(ts);
}

void writeWavHeader(File &file, uint32_t dataSize) {
    uint32_t sampleRate = SAMPLE_RATE;
    uint16_t bitsPerSample = 16;
    uint16_t numChannels = 1;
    uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;
    uint32_t chunkSize = dataSize + 36;
    uint16_t audioFormat = 1;
    uint32_t subchunk1Size = 16;

    const uint8_t riff[] = {'R','I','F','F'};
    const uint8_t wave[] = {'W','A','V','E'};
    const uint8_t fmt[]  = {'f','m','t',' '};
    const uint8_t data[] = {'d','a','t','a'};

    file.write(riff, 4);
    file.write((uint8_t*)&chunkSize, 4);
    file.write(wave, 4);

    file.write(fmt, 4);
    file.write((uint8_t*)&subchunk1Size, 4);
    file.write((uint8_t*)&audioFormat, 2);
    file.write((uint8_t*)&numChannels, 2);
    file.write((uint8_t*)&sampleRate, 4);
    file.write((uint8_t*)&byteRate, 4);
    file.write((uint8_t*)&blockAlign, 2);
    file.write((uint8_t*)&bitsPerSample, 2);

    file.write(data, 4);
    file.write((uint8_t*)&dataSize, 4);
}

/* ============== BUTTON HANDLERS ============== */

bool checkCancelButtonPressed() {
    bool reading = digitalRead(BUTTON_PIN);
    
    // Detect any change in button state
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
        lastButtonState = reading;
    }
    
    // After debounce period, check if button is currently LOW (pressed)
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading == LOW) {
            // Wait for button release to avoid multiple triggers
            while (digitalRead(BUTTON_PIN) == LOW) {
                delay(10);
            }
            lastDebounceTime = millis();
            Serial.println("[BUTTON] Cancel button pressed!");
            return true;
        }
    }
    
    return false;
}

// ✅ Process help button request (called from main loop)
void processHelpButton() {
    // Check cooldown to prevent spam
    if (millis() - lastHelpRequestTime < HELP_COOLDOWN) {
        Serial.printf("[HELP] Please wait %lu more seconds before next request\n", 
                     (HELP_COOLDOWN - (millis() - lastHelpRequestTime)) / 1000);
        
        // Flash red to indicate cooldown
        strip.setPixelColor(0, strip.Color(255, 0, 0));
        strip.show();
        delay(200);
        strip.setPixelColor(0, strip.Color(0, 0, 0));
        strip.show();
        
        return;
    }
    
    lastHelpRequestTime = millis();
    Serial.println("[HELP] ✅ Help button pressed! Processing...");
    
    sendHelpNotification();
}

// ✅ Send help notification
void sendHelpNotification() {
    Serial.println("\n[HELP] ========== HELP REQUEST ==========");
    
    // Visual feedback - Blue pulsing LED
    for (int i = 0; i < 3; i++) {
        strip.setPixelColor(0, strip.Color(0, 0, 255));
        strip.show();
        delay(200);
        strip.setPixelColor(0, strip.Color(0, 0, 0));
        strip.show();
        delay(200);
    }
    
    String timestamp = getTimestamp();
    
    // Check Firebase connection
    if (!firebaseManager->isConnected()) {
        Serial.println("[HELP][WARN] Not connected, trying to reconnect...");
        firebaseManager->init();
    }
    
    // Send help notification
    if (firebaseManager->sendHelpRequest(MENTEE_NUMBER, timestamp)) {
        Serial.println("[HELP] ✅ Help notification sent successfully!");
        
        // Success feedback - Green LED
        strip.setPixelColor(0, strip.Color(0, 255, 0));
        strip.show();
        delay(1000);
        strip.setPixelColor(0, strip.Color(0, 0, 0));
        strip.show();
    } else {
        Serial.println("[HELP] ❌ Failed to send help notification");
        
        // Error feedback - Red LED
        strip.setPixelColor(0, strip.Color(255, 0, 0));
        strip.show();
        delay(1000);
        strip.setPixelColor(0, strip.Color(0, 0, 0));
        strip.show();
    }
    
    Serial.println("[HELP] ========================================\n");
}

/* ============== MAIN DETECTION CYCLE ============== */
void performEmotionDetection() {
    currentState = STATE_RECORDING;
    
    String detectID = "detection_" + getTimestamp();
    Serial.println("\n[SYSTEM] ========== Starting Detection Cycle ==========");
    Serial.println("[SYSTEM] Detection ID: " + detectID);
    
    /* ---------- RECORD AUDIO ---------- */
    Serial.println("\n[SYSTEM] Phase 1: Recording audio...");
    if (!audioCapture->startRecording(RECORD_SECONDS * 1000)) {
        Serial.println("[SYSTEM][ERROR] Audio recording failed");
        currentState = STATE_IDLE;
        return;
    }
    
    uint8_t *audio_buffer = audioCapture->getBuffer();
    size_t audio_size = audioCapture->getSize();
    
    audioCapture->applyGain(MIC_GAIN);

    /* ---------- SAVE TO SD ---------- */
    Serial.println("[SYSTEM] Phase 1b: Saving audio to SD...");
    File wavFile = SD.open("/record" + String(counter_sd_files) + ".wav", FILE_WRITE);
    if (!wavFile) {
        Serial.println("[SYSTEM][ERROR] Failed to open WAV file on SD");
    } else {
        writeWavHeader(wavFile, audio_size);
        wavFile.write(audio_buffer, audio_size);
        wavFile.close();
        Serial.println("[SYSTEM] ✅ Audio saved to SD");
        counter_sd_files++;
    }
    
    
    /* ---------- DETECT ANGER ---------- */
    currentState = STATE_DETECTING;
    Serial.println("\n[SYSTEM] Phase 2: Analyzing emotion...");

    int anger_result = angerDetector->detectAnger(audio_buffer, audio_size);

    if (anger_result == -1) {
        Serial.println("[SYSTEM][ERROR] Anger detection failed");
        audioCapture->freeBuffer();
        currentState = STATE_IDLE;
        return;
    }
    

    /* ---------- CHECK IF ANGRY ---------- */
    if (anger_result == 0) {
        Serial.println("[SYSTEM] ✅ Not angry - no action needed");
        audioCapture->freeBuffer();
        currentState = STATE_IDLE;
        return;
    }
    
    Serial.println("[SYSTEM] 🔴 ANGER DETECTED - Initiating upload sequence...");
    
    /* ---------- FREE NEURAL NETWORK MEMORY ---------- */
    Serial.println("[SYSTEM] Freeing Neural Network memory...");
    if (angerDetector) {
        delete angerDetector;
        angerDetector = nullptr;
    }
    Serial.printf("[SYSTEM] Free heap after NN cleanup: %u bytes\n", ESP.getFreeHeap());

    /* ---------- CAPTURE IMAGE ---------- */
    Serial.println("\n[SYSTEM] Phase 3: Capturing image...");
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[SYSTEM][ERROR] Image capture failed");
        audioCapture->freeBuffer();
        currentState = STATE_IDLE;
        return;
    }
    
    /* ---------- UPLOAD TO FIREBASE ---------- */
    currentState = STATE_UPLOADING;
    Serial.println("\n[SYSTEM] Phase 4: Uploading to Firebase...");
    
    WAVHeader header;
    header.dataSize = audio_size;
    header.byteRate = SAMPLE_RATE * 2;
    header.blockAlign = 2;
    header.fileSize = 36 + audio_size;
    
    size_t wavSize = audio_size + sizeof(WAVHeader);
    uint8_t *wavBuffer = (uint8_t *)heap_caps_malloc(wavSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (!wavBuffer) {
        Serial.println("[SYSTEM][ERROR] WAV buffer allocation failed");
        esp_camera_fb_return(fb);
        audioCapture->freeBuffer();
        currentState = STATE_IDLE;
        return;
    }
    
    memcpy(wavBuffer, &header, sizeof(WAVHeader));
    memcpy(wavBuffer + sizeof(WAVHeader), audio_buffer, audio_size);
    
    String imgPath = "/detections/" + detectID + "/image.jpg";
    String audPath = "/detections/" + detectID + "/audio.wav";
    String timestamp = getTimestamp();
    
    if (!firebaseManager->isConnected()) {
        Serial.println("[FIREBASE][WARN] Not connected, trying to reconnect...");
        firebaseManager->init();
    }

    if (!firebaseManager->notifyMentorOfDetection(MENTEE_NUMBER, imgPath, audPath, timestamp)) {
        Serial.println("[SYSTEM][WARN] Mentor notification failed");
    } else {
        Serial.println("[SYSTEM] ✅ Mentor notified successfully");
    }

    if (!firebaseManager->uploadImage(detectID, fb->buf, fb->len)) {
        Serial.println("[SYSTEM][WARN] Image upload failed, continuing...");
    }
    
    if (!firebaseManager->uploadAudio(detectID, wavBuffer, wavSize)) {
        Serial.println("[SYSTEM][WARN] Audio upload failed, continuing...");
    }
    
    if (!firebaseManager->createDetectionDocument(detectID, imgPath, audPath, timestamp)) {
        Serial.println("[SYSTEM][WARN] Firestore document creation failed");
    }
    
    esp_camera_fb_return(fb);
    audioCapture->freeBuffer();
    free(wavBuffer);
    
    Serial.println("\n[SYSTEM] ✅ Upload complete!");
    Serial.println("[SYSTEM] Detection ID: " + detectID);
    
    /* ---------- LISTEN FOR NOTIFICATION ---------- */
    currentState = STATE_LISTENING_FOR_NOTIFICATION;
    Serial.println("\n[SYSTEM] Phase 5: Waiting for notification response (60 seconds)...");
    
    unsigned long notificationWaitStart = millis();
    cancelRequested = false;

    while (millis() - notificationWaitStart < NOTIFICATION_TIMEOUT_MS) {

        // Check cancel button
        if (checkCancelButtonPressed()) {
            cancelRequested = true;
            String cancelTime = getTimestamp();

            Serial.println("[SYSTEM] ❌ Cancel button pressed during 60s window!");

            firebaseManager->markDetectionCanceled(detectID, cancelTime);
            firebaseManager->notifyMentorOfCancellation(MENTEE_NUMBER, detectID, cancelTime);
            
            break;
        }

        // Update LED
        if (!cancelRequested) {
            strip.setPixelColor(0, strip.Color(150, 150, 0)); // Yellow
            strip.show();
        } else {
            strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red
            strip.show();
        }

        notificationListener->checkAndPlayNextAudio();
        delay(20);
    }
 
    Serial.println("\n[SYSTEM] Notification timeout reached");
    Serial.println("[SYSTEM] ========== Detection Cycle Complete ==========\n");
    
    currentState = STATE_IDLE;
}


/* ============== MAIN LOOP ============== */
void loop() {
    // ✅ CRITICAL: Check help button flag first (set by interrupt)
    if (helpButtonPressed) {
        helpButtonPressed = false;  // Reset flag immediately
        processHelpButton();        // Process the request
    }
    
    if (currentState == STATE_IDLE) {
        // Reinitialize anger detector if needed
        if (!angerDetector) {
            angerDetector = new AngerDetector();
            if (!angerDetector->init(&strip)) {
                Serial.println("[SETUP][ERROR] Failed to reinitialize anger detector");
                while (true);
            }
        }
        
        performEmotionDetection();
        delay(5000);
    } else {
        delay(100);
    }
}

void printSystemStatus() {
    Serial.println("\n[STATUS] System Information:");
    Serial.printf("[STATUS] Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("[STATUS] PSRAM free: %u bytes\n", ESP.getFreePsram());
    Serial.printf("[STATUS] Current state: %d\n", currentState);
}