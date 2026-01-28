#ifndef FIREBASE_MANAGER_H
#define FIREBASE_MANAGER_H

#include <Arduino.h>
#include <Firebase_ESP_Client.h>



class FirebaseManager {
private:
    FirebaseData fbdoStorage;    // For Storage / Firestore uploads
    FirebaseAuth auth;
    FirebaseConfig config;
    bool initialized;
    bool authenticated;
    
    // Helper to get mentor ID from RTDB
    String getMentorIdFromRTDB(int menteeNumber);

   

public:
    FirebaseManager();
    ~FirebaseManager();
    
    bool init();
    bool uploadAudio(const String &detectID, uint8_t *audio_data, size_t audio_size);
    bool uploadImage(const String &detectID, uint8_t *image_data, size_t image_size);
    bool createDetectionDocument(const String &detectID, const String &imgPath, 
                                const String &audPath, const String &timestamp);
    
    // Mentor notification
    bool notifyMentorOfDetection(int menteeNumber, const String &imageUrl, 
                                 const String &audioUrl, const String &detectionTime);
    
    bool isConnected() const;
    bool notifyMentorOfCancellation(int menteeNumber, const String &detectionId, const String &cancelTime);
    FirebaseData& getStorageFBDO();
  bool  markDetectionCanceled(const String& detectID,
    const String& timestamp
);
bool sendHelpRequest(int menteeNumber, const String &timestamp);
};

#endif
