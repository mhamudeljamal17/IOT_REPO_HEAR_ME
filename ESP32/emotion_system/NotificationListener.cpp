#include "NotificationListener.h"
#include "config.h"
#include <Arduino.h>

// Global callback functions for Firebase streaming
NotificationListener *g_notificationListener = nullptr;

void streamCallback(FirebaseStream data) {
    if (g_notificationListener) {
        g_notificationListener->handleStreamCallback(data);
    }
}

void streamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.println("[LISTENER][WARN] Stream timeout, reconnecting...");
    }
}

// ============================================

NotificationListener::NotificationListener() 
    : isPlaying(false), playbackStartTime(0) {}

NotificationListener::~NotificationListener() {}

bool NotificationListener::init(FirebaseData &fb_ref) {
    g_notificationListener = this;
    fbdo = fb_ref;
    return true;
}

bool NotificationListener::startListening(FirebaseAuth &auth, FirebaseConfig &config) {
    Serial.println("[LISTENER] Starting to listen for notifications...");
    
    if (!Firebase.RTDB.beginStream(&fbdo, ESP_COMMAND_PATH)) {
        Serial.println("[LISTENER][ERROR] Stream begin failed");
        Serial.println(fbdo.errorReason());
        return false;
    }
    
    Firebase.RTDB.setStreamCallback(&fbdo, streamCallback, streamTimeoutCallback);
    Serial.println("[LISTENER] Ready for notifications");
    return true;
}

void NotificationListener::handleStreamCallback(FirebaseStream data) {
    Serial.println("\n[LISTENER] Data received from Firebase");
    
    if (data.dataType() != "json") {
        Serial.println("[LISTENER] Not JSON, ignored");
        return;
    }
    
    FirebaseJson *json = data.to<FirebaseJson*>();
    FirebaseJsonData result;
    
    String status;
    String audioUrl;
    
    if (json->get(result, "status"))   status   = result.stringValue;
    if (json->get(result, "audioUrl")) audioUrl = result.stringValue;
    
    Serial.println("[LISTENER] status   = " + status);
    Serial.println("[LISTENER] audioUrl = " + audioUrl);
    
    // Only handle new audio commands
    if (status == "new" && audioUrl.length() > 10) {
        Serial.println("[LISTENER] 🔊 New audio command detected, adding to queue");
        
        // Add to queue
        audioQueue.push(audioUrl);
        
        // Update status to 'old' so it won't trigger again
        if (Firebase.RTDB.setString(&fbdo, ESP_COMMAND_PATH "/status", "old")) {
            Serial.println("[LISTENER] Status updated to 'old'");
        } else {
            Serial.println("[LISTENER][ERROR] Failed to update status: " + fbdo.errorReason());
        }
    }
}

void NotificationListener::updateAudioQueue(const String &audioUrl) {
    if (audioUrl.length() > 10) {
        audioQueue.push(audioUrl);
        Serial.println("[LISTENER] Added audio to queue: " + audioUrl);
    }
}

bool NotificationListener::isAudioPlaying() const {
    return isPlaying;
}

void NotificationListener::setPlaybackActive(bool active) {
    isPlaying = active;
    if (active) {
        playbackStartTime = millis();
    }
}

bool NotificationListener::hasQueuedAudio() const {
    return !audioQueue.empty();
}

String NotificationListener::getNextAudio() {
    if (audioQueue.empty()) return "";
    
    String url = audioQueue.front();
    audioQueue.pop();
    return url;
}

void NotificationListener::checkAndPlayNextAudio() {
    // Check for timeout (1 minute)
    if (isPlaying && (millis() - playbackStartTime > NOTIFICATION_TIMEOUT_MS)) {
        Serial.println("[LISTENER] Notification timeout, stopping");
        isPlaying = false;
    }
}