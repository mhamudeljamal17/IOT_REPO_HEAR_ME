# Mentee Number System - How It Works

## 🎯 Overview

Your HearMe app now uses **auto-generated unique mentee numbers** instead of manual IDs. This prevents duplicates and makes it easy for mentees to identify themselves on the shared ESP32 device.

---

## 📱 How It Works

### 1. **Mentor Adds a New Mentee**

1. Mentor clicks "Add Mentee" button
2. Fills in: Name, Phone (optional), Age
3. System auto-generates unique number (1001, 1002, 1003...)
4. Mentor sees a dialog showing the mentee's number
5. Mentor gives this number to the mentee

**Example:**
```
✅ Mentee Added
Name: John Doe

┌─────────────────┐
│ Mentee Number   │
│      1001       │
└─────────────────┘

Give this number to the mentee. They will use it to identify themselves on the ESP32 device.
```

---

### 2. **Mentee Uses the ESP32**

Since you have **ONE ESP32 shared by all mentees**:

1. Mentee goes to your website
2. Enters their unique number (e.g., 1001)
3. Website updates Firestore with active mentee
4. ESP32 knows which mentee is currently using it

**Firestore Structure:**
```
esp32_status/
  - current_mentee (document)
    - menteeNumber: 1001
    - timestamp: [when they logged in]
    - active: true
```

---

### 3. **Mentor Sends Voice Recording**

1. Mentor selects mentee from list
2. Records voice message
3. Sends → Creates notification with mentee number
4. Backend forwards to ESP32
5. ESP32 checks: Is this for the current active mentee?
6. If yes → Download and play audio

---

## 🗄️ Firestore Data Structure

### mentees Collection:
```json
{
  "id": "auto_doc_id",
  "menteeNumber": 1001,          // 👈 Auto-generated unique number
  "name": "John Doe",
  "age": 25,
  "phone": "+1234567890",
  "mentorId": "mentor_uid",
  "createdAt": "2026-01-18..."
}
```

### voice_recordings Collection:
```json
{
  "menteeDocId": "doc_id",
  "menteeNumber": 1001,          // 👈 Used to route to correct mentee
  "menteeName": "John Doe",
  "fileName": "mentee_1001_1737...",
  "downloadUrl": "https://...",
  "timestamp": "...",
  "processed": false
}
```

### notifications Collection:
```json
{
  "menteeId": "doc_id",
  "menteeNumber": 1001,          // 👈 ESP32 checks this
  "title": "New Voice Recording",
  "data": {
    "type": "voice_recording",
    "recordingId": "...",
    "downloadUrl": "...",
    "menteeNumber": 1001
  },
  "status": "pending",
  "type": "esp32_audio"
}
```

---

## 🔢 Number Generation System

### How Numbers Are Generated:

**Counter Document** (`mentee_counter/current`):
```json
{
  "value": 1001  // Current highest number
}
```

**Process:**
1. New mentee added
2. Transaction reads counter
3. Increments by 1 (1001 → 1002)
4. Saves new number to counter
5. Assigns 1002 to new mentee

**Why Transaction?**
- Prevents duplicates even if two mentors add mentees simultaneously
- Atomic operation - guaranteed unique numbers

---

## 🌐 Website for Mentee Login

You'll need to create a simple website where mentees enter their number:

### Example HTML/JavaScript:

```html
<!DOCTYPE html>
<html>
<head>
    <title>HearMe - Mentee Login</title>
</head>
<body>
    <h1>Enter Your Mentee Number</h1>
    <input type="number" id="menteeNumber" placeholder="1001">
    <button onclick="login()">Activate ESP32</button>
    
    <script src="https://www.gstatic.com/firebasejs/9.0.0/firebase-app.js"></script>
    <script src="https://www.gstatic.com/firebasejs/9.0.0/firebase-firestore.js"></script>
    <script>
        // Initialize Firebase
        const firebaseConfig = { /* your config */ };
        firebase.initializeApp(firebaseConfig);
        const db = firebase.firestore();
        
        async function login() {
            const menteeNumber = parseInt(document.getElementById('menteeNumber').value);
            
            // Verify mentee exists
            const menteeQuery = await db.collection('mentees')
                .where('menteeNumber', '==', menteeNumber)
                .limit(1)
                .get();
            
            if (menteeQuery.empty) {
                alert('Invalid mentee number');
                return;
            }
            
            // Set as active mentee on ESP32
            await db.collection('esp32_status').doc('current_mentee').set({
                menteeNumber: menteeNumber,
                timestamp: firebase.firestore.FieldValue.serverTimestamp(),
                active: true
            });
            
            alert(`✅ ESP32 activated for mentee ${menteeNumber}`);
        }
    </script>
</body>
</html>
```

---

## 🎛️ ESP32 Updates Needed

Update your ESP32 code to check which mentee is active:

```cpp
int currentActiveMenteeNumber = 0;

void checkActiveMentee() {
  // Query Firebase for current active mentee
  // This would use Firebase ESP32 library or HTTP request to your backend
  // For now, you can have the backend send this info
}

void handleRecording(WiFiClient& client, String jsonBody) {
  StaticJsonDocument<512> doc;
  deserializeJson(doc, jsonBody);
  
  int menteeNumber = doc["data"]["menteeNumber"];
  
  // Check if this recording is for the current active mentee
  if (menteeNumber != currentActiveMenteeNumber) {
    Serial.println("Recording not for current mentee - ignoring");
    sendErrorResponse(client, "Not for active mentee");
    return;
  }
  
  // Process recording...
  currentRecordingUrl = doc["downloadUrl"].as<String>();
  downloadAudioFile(currentRecordingUrl);
  playAudio();
}
```

---

## ✅ Summary

**What Changed:**
- ❌ Manual mentee ID entry (risk of duplicates)
- ✅ Auto-generated unique numbers (1001, 1002, 1003...)

**Benefits:**
1. No duplicate mentee IDs
2. Easy to remember 4-digit numbers
3. One ESP32 can serve multiple mentees
4. Simple website login for mentees
5. Secure - only active mentee receives messages

**Next Steps:**
1. ✅ Flutter app updated (done)
2. 🔄 Create mentee login website
3. 🔄 Update ESP32 code to check active mentee
4. 🔄 Deploy Render backend
5. 🔄 Test full flow

---

Ready to deploy! 🚀
