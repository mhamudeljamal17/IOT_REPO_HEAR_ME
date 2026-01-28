#include "FirebaseManager.h"
#include "config.h"
#include <Arduino.h>

FirebaseManager::FirebaseManager() : initialized(false), authenticated(false) {}

FirebaseManager::~FirebaseManager() {}

bool FirebaseManager::init() {
    Serial.println("[FIREBASE] Initializing Firebase...");
    
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    delay(2000);  // Wait for authentication
    
    if (Firebase.ready()) {
        authenticated = true;
        initialized = true;
        Serial.println("[FIREBASE] Connected and authenticated!");
        return true;
    }
    
    Serial.println("[FIREBASE][ERROR] Authentication failed");
    return false;
}

// ------------------- UPLOAD AUDIO -------------------
bool FirebaseManager::uploadAudio(const String &detectID, uint8_t *audio_data, size_t audio_size) {
    if (!initialized) {
        Serial.println("[FIREBASE][ERROR] Not initialized");
        return false;
    }
    
    String audPath = "/detections/" + detectID + "/audio.wav";
    Serial.println("[FIREBASE] Uploading audio...");
    
    if (!Firebase.Storage.upload(&fbdoStorage, STORAGE_BUCKET_ID, audio_data, audio_size, audPath.c_str(), "audio/wav")) {
        Serial.println("[FIREBASE][ERROR] Audio upload failed: " + fbdoStorage.errorReason());
        return false;
    }
    
    Serial.println("[FIREBASE] Audio uploaded successfully");
    return true;
}

// ------------------- UPLOAD IMAGE -------------------
bool FirebaseManager::uploadImage(const String &detectID, uint8_t *image_data, size_t image_size) {
    if (!initialized) {
        Serial.println("[FIREBASE][ERROR] Not initialized");
        return false;
    }
    
    String imgPath = "/detections/" + detectID + "/image.jpg";
    Serial.println("[FIREBASE] Uploading image...");
    
    if (!Firebase.Storage.upload(&fbdoStorage, STORAGE_BUCKET_ID, image_data, image_size, imgPath.c_str(), "image/jpeg")) {
        Serial.println("[FIREBASE][ERROR] Image upload failed: " + fbdoStorage.errorReason());
        return false;
    }
    
    Serial.println("[FIREBASE] Image uploaded successfully");
    return true;
}

// ------------------- CREATE FIRESTORE DOCUMENT -------------------
bool FirebaseManager::createDetectionDocument(const String &detectID, const String &imgPath, 
                                               const String &audPath, const String &timestamp) {
    if (!initialized) {
        Serial.println("[FIREBASE][ERROR] Not initialized");
        return false;
    }
    
    Serial.println("[FIREBASE] Creating Firestore document...");
    
    FirebaseJson json;
    json.set("fields/menteeNumber/integerValue", String(MENTEE_NUMBER));
    json.set("fields/image/stringValue", imgPath);
    json.set("fields/audio/stringValue", audPath);
    json.set("fields/timestamp/stringValue", timestamp);
    json.set("fields/message/stringValue", "New detection available – tap to view");
    
    String jsonStr;
    json.toString(jsonStr, true);
    
    if (!Firebase.Firestore.createDocument(&fbdoStorage, FIRESTORE_PROJECT_ID, "", "detections", detectID.c_str(), jsonStr.c_str(), "")) {
        Serial.println("[FIREBASE][ERROR] Document creation failed: " + fbdoStorage.errorReason());
        return false;
    }
    
    Serial.println("[FIREBASE] Firestore document created");
    return true;
}

// ------------------- GET MENTOR ID FROM RTDB -------------------
String FirebaseManager::getMentorIdFromRTDB(int menteeNumber) {
    String path = "/mentees/" + String(menteeNumber) + "/mentorId";
    Serial.println("[RTDB] Reading mentorId from: " + path);

    // Use a separate FirebaseData object for RTDB
    FirebaseData fbdoRTDB;

    if (!Firebase.RTDB.getString(&fbdoRTDB, path.c_str())) {
        Serial.println("[RTDB][ERROR] " + fbdoRTDB.errorReason());
        return "";
    }

    String mentorId = fbdoRTDB.stringData();
    Serial.println("[RTDB] ✅ mentorId = " + mentorId);
    return mentorId;
}

// ------------------- NOTIFY MENTOR -------------------
bool FirebaseManager::notifyMentorOfDetection(int menteeNumber, const String &imageUrl, 
                                              const String &audioUrl, const String &detectionTime) {
    if (!initialized) {
        Serial.println("[NOTIFY][ERROR] Firebase not initialized");
        return false;
    }
    
    Serial.printf("[NOTIFY] Creating notification for menteeNumber: %d\n", menteeNumber);
    
    String mentorId = getMentorIdFromRTDB(menteeNumber);
    if (mentorId.isEmpty()) {
        Serial.println("[NOTIFY][ERROR] No mentor ID found, skipping notification");
        return false;
    }
    
    FirebaseJson json;
    json.set("fields/menteeNumber/integerValue", String(menteeNumber));
    json.set("fields/mentorId/stringValue", mentorId);
    json.set("fields/imagePath/stringValue", imageUrl);
    json.set("fields/audioPath/stringValue", audioUrl);
    json.set("fields/timestamp/stringValue", detectionTime);
    json.set("fields/title/stringValue", "New Detection - Mentee Alert");
    json.set("fields/message/stringValue", "Mentee #" + String(menteeNumber) + " triggered a detection");
    json.set("fields/status/stringValue", "pending");
    json.set("fields/type/stringValue", "mentor_alert");
    
    String jsonStr;
    json.toString(jsonStr, true);
    
    String notificationDocId = "notification_" + detectionTime;
    
    if (!Firebase.Firestore.createDocument(&fbdoStorage, FIRESTORE_PROJECT_ID, "", "notifications_from_esp", notificationDocId.c_str(), jsonStr.c_str(), "")) {
        Serial.println("[NOTIFY][ERROR] " + fbdoStorage.errorReason());
        return false;
    }
    
    Serial.printf("[NOTIFY] ✅ Notification created for mentor: %s\n", mentorId.c_str());
    return true;
}
bool FirebaseManager::notifyMentorOfCancellation(int menteeNumber, const String &detectionId, const String &cancelTime) {
    if (!initialized) {
        Serial.println("[CANCEL][ERROR] Firebase not initialized");
        return false;
    }

    Serial.printf("[CANCEL] Creating cancellation notification for menteeNumber: %d\n", menteeNumber);

    // Get the mentor ID from your RTDB or Firestore
    String mentorId = getMentorIdFromRTDB(menteeNumber);
    if (mentorId.isEmpty()) {
        Serial.println("[CANCEL][ERROR] No mentor ID found, skipping notification");
        return false;
    }

    FirebaseJson json;
    json.set("fields/menteeNumber/integerValue", String(menteeNumber));
    json.set("fields/mentorId/stringValue", mentorId);
    json.set("fields/detectionId/stringValue", detectionId);
    json.set("fields/timestamp/stringValue", cancelTime);
    json.set("fields/title/stringValue", "Detection Canceled - Mentee Alert");
    json.set("fields/message/stringValue", "Mentee #" + String(menteeNumber) + " canceled a previous detection");
    json.set("fields/status/stringValue", "canceled");
    json.set("fields/type/stringValue", "mentor_alert");

    String jsonStr;
    json.toString(jsonStr, true);

    String notificationDocId = "cancel_" + detectionId;

    if (!Firebase.Firestore.createDocument(&fbdoStorage, FIRESTORE_PROJECT_ID, "", "notifications_from_esp", notificationDocId.c_str(), jsonStr.c_str(), "")) {
        Serial.println("[CANCEL][ERROR] " + fbdoStorage.errorReason());
        return false;
    }

    Serial.printf("[CANCEL] ✅ Cancellation notification created for mentor: %s\n", mentorId.c_str());
    return true;
}

// ------------------- CONNECTION CHECK -------------------
bool FirebaseManager::isConnected() const {
    return authenticated && Firebase.ready();
}

// ------------------- GET STORAGE FBDO -------------------
FirebaseData& FirebaseManager::getStorageFBDO() {
    return fbdoStorage;
}
bool FirebaseManager::markDetectionCanceled(
    const String& detectID,
    
    const String& timestamp
) {
    if (!initialized) {
        Serial.println("[FIREBASE][ERROR] Not initialized");
        return false;
    }

    // Build JSON for patch
    FirebaseJson json;
    json.set("fields/canceled/booleanValue", true);
    
    json.set("fields/cancel_timestamp/stringValue", timestamp);

    String docPath = "detections/" + detectID;
    FirebaseData fbdo;

    if (!Firebase.Firestore.patchDocument(
            &fbdo,
            FIRESTORE_PROJECT_ID,
            "",               // default database
            docPath.c_str(),
            json.raw(),
            "canceled,cancel_reason,cancel_timestamp"
        )) {
        Serial.println("[FIREBASE][ERROR] Cancel update failed:");
        Serial.println(fbdo.errorReason());
        return false;
    }

    Serial.println("[FIREBASE] Detection marked as canceled");
    return true;
}
bool FirebaseManager::sendHelpRequest(int menteeNumber, const String &timestamp) {
    if (!initialized) {
        Serial.println("[HELP][ERROR] Firebase not initialized");
        return false;
    }
    
    Serial.printf("[HELP] Creating help request for menteeNumber: %d\n", menteeNumber);
    
    String mentorId = getMentorIdFromRTDB(menteeNumber);
    if (mentorId.isEmpty()) {
        Serial.println("[HELP][ERROR] No mentor ID found");
        return false;
    }
    
    // ✅ Same structure as detection notifications
    FirebaseJson json;
    json.set("fields/menteeNumber/integerValue", String(menteeNumber));
    json.set("fields/mentorId/stringValue", mentorId);
    json.set("fields/timestamp/stringValue", timestamp);
    json.set("fields/title/stringValue", "🆘 URGENT HELP REQUEST");
    json.set("fields/message/stringValue", "Mentee #" + String(menteeNumber) + " needs immediate assistance!");
    json.set("fields/status/stringValue", "pending");  // ✅ Same as detections
    json.set("fields/type/stringValue", "help_request");
    
    // ✅ Add empty paths (same as detections have imagePath/audioPath)
    json.set("fields/imagePath/stringValue", "");
    json.set("fields/audioPath/stringValue", "");
    
    String jsonStr;
    json.toString(jsonStr, true);
    
    String notificationDocId = "help_" + timestamp;
    
    // ✅ Same collection as all other notifications
    if (!Firebase.Firestore.createDocument(&fbdoStorage, FIRESTORE_PROJECT_ID, "", "notifications_from_esp", notificationDocId.c_str(), jsonStr.c_str(), "")) {
        Serial.println("[HELP][ERROR] " + fbdoStorage.errorReason());
        return false;
    }
    
    Serial.printf("[HELP] ✅ Help request created for mentor: %s\n", mentorId.c_str());
    return true;
}