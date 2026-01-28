#ifndef NOTIFICATION_LISTENER_H
#define NOTIFICATION_LISTENER_H

#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <queue>

class NotificationListener {
private:
    FirebaseData fbdo;
    std::queue<String> audioQueue;
    bool isPlaying;
    unsigned long playbackStartTime;
    
public:
    NotificationListener();
    ~NotificationListener();
    
    bool init(FirebaseData &fb_ref);
    bool startListening(FirebaseAuth &auth, FirebaseConfig &config);
    void handleStreamCallback(FirebaseStream data);
    
    void updateAudioQueue(const String &audioUrl);
    bool isAudioPlaying() const;
    void checkAndPlayNextAudio();
    void setPlaybackActive(bool active);
    
    bool hasQueuedAudio() const;
    String getNextAudio();
};

#endif