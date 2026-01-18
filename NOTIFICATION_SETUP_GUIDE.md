# HearMe Notification & ESP32 Integration Setup Guide

This guide will help you set up the complete notification system with ESP32 integration.

## Architecture Overview

**Flutter App → Firebase → Render Backend → ESP32**

When you send a voice recording:
1. Flutter uploads to Firebase Storage
2. Metadata saved to Firestore
3. Push notification sent to mentee's device
4. Render backend listens to Firestore changes
5. Backend forwards data to ESP32
6. ESP32 downloads and processes the audio

---

## Part 1: Flutter App Setup

### 1.1 Install Dependencies

```bash
cd flutter_app
flutter pub get
```

### 1.2 Firebase Cloud Messaging Setup

#### Android Setup:
1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Select your project
3. Go to Project Settings → Cloud Messaging
4. Copy your Server Key (you'll need this later)
5. Update `android/app/build.gradle`:
   ```gradle
   defaultConfig {
       minSdkVersion 21  // Make sure it's at least 21
   }
   ```

6. Add to `android/app/src/main/AndroidManifest.xml` (inside `<application>` tag):
   ```xml
   <meta-data
       android:name="com.google.firebase.messaging.default_notification_channel_id"
       android:value="hearme_channel" />
   ```

#### iOS Setup:
1. Enable Push Notifications in Xcode capabilities
2. Upload your APNs certificate to Firebase Console
3. Update `ios/Runner/Info.plist`:
   ```xml
   <key>UIBackgroundModes</key>
   <array>
       <string>fetch</string>
       <string>remote-notification</string>
   </array>
   ```

### 1.3 Initialize Notification Service

Update `lib/main.dart`:

```dart
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'services/notification_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  await Firebase.initializeApp();
  
  // Initialize notifications
  await NotificationService().initialize();
  
  // Save FCM token for logged-in user
  final user = FirebaseAuth.instance.currentUser;
  if (user != null) {
    await NotificationService().saveTokenToFirestore(user.uid);
  }
  
  runApp(MyApp());
}
```

---

## Part 2: Render Backend Setup

### 2.1 Get Firebase Service Account Key

1. Go to Firebase Console → Project Settings → Service Accounts
2. Click "Generate New Private Key"
3. Save the JSON file

### 2.2 Deploy to Render

1. Create account at [render.com](https://render.com)
2. Create new "Web Service"
3. Connect your GitHub repository or upload code
4. Configure:
   - **Name**: hearme-backend
   - **Environment**: Node
   - **Build Command**: `npm install`
   - **Start Command**: `npm start`
   - **Instance Type**: Free

5. Add Environment Variables in Render dashboard:
   - `FIREBASE_SERVICE_ACCOUNT`: Paste entire JSON content from step 2.1
   - `PORT`: 3000
   - `ESP32_DEVICES`: (leave empty for now, will update after ESP32 setup)

6. Deploy and copy your Render URL (e.g., `https://hearme-backend.onrender.com`)

### 2.3 Local Testing (Optional)

```bash
cd render_backend
npm install
cp .env.example .env
# Edit .env with your Firebase credentials
npm run dev
```

---

## Part 3: ESP32 Setup

### 3.1 Hardware Requirements
- ESP32 XIAO S3
- WiFi connection
- Micro SD card (optional, for storing audio)

### 3.2 Install Arduino Libraries

Open Arduino IDE and install:
- ArduinoJson (v6.x)
- WiFi (built-in)
- HTTPClient (built-in)

### 3.3 Configure ESP32

1. Copy `ESP32/SECRETS_EXAMPLE.h` to `ESP32/SECRETS.h`
2. Edit `SECRETS.h`:
   ```cpp
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASSWORD "YourWiFiPassword"
   #define MENTEE_ID "mentee_doc_id_from_firebase"  // Get from Firestore
   ```

3. Update `esp32_receiver.ino`:
   ```cpp
   const char* backendUrl = "https://your-app.onrender.com";  // Your Render URL
   ```

### 3.4 Upload to ESP32

1. Connect ESP32 to computer
2. Select Board: "ESP32S3 Dev Module"
3. Upload `esp32_receiver.ino`
4. Open Serial Monitor (115200 baud)
5. Note the IP address shown

### 3.5 Register ESP32 with Backend

Update Render environment variable:
```
ESP32_DEVICES=mentee123:192.168.1.100
```
(Format: menteeId:esp32_ip_address)

Or the ESP32 will auto-register when it starts up.

---

## Part 4: Testing

### 4.1 Test Flutter Notifications

1. Run your Flutter app on a device (not emulator for notifications)
2. Log in with a mentor account
3. Go to a mentee's details page
4. Record and send a voice message
5. Check:
   - Firebase Storage for uploaded file
   - Firestore `voice_recordings` collection
   - Firestore `notifications` collection

### 4.2 Test Backend

Check Render logs:
```
Listening to Firestore notifications...
New notification detected: [doc_id]
FCM sent to: [token]
Sending to ESP32 at 192.168.1.100
ESP32 response: {"success":true}
```

### 4.3 Test ESP32

Check Serial Monitor:
```
WiFi connected!
IP Address: 192.168.1.100
HTTP server started on port 80
New client connected
=== New Recording Received ===
Recording ID: abc123
File Name: mentee_123_1234567890.wav
Download URL: https://...
Downloading audio from: https://...
Downloaded 45678 bytes
```

---

## Part 5: Firebase Security Rules

Update Firestore security rules:

```javascript
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    // Voice recordings
    match /voice_recordings/{recording} {
      allow read: if request.auth != null;
      allow write: if request.auth != null;
    }
    
    // Notifications
    match /notifications/{notification} {
      allow read: if request.auth != null;
      allow create: if request.auth != null;
      allow update: if request.auth != null || 
                      resource.data.menteeId == request.auth.uid;
    }
    
    // ESP32 devices
    match /esp32_devices/{device} {
      allow read: if request.auth != null;
      allow write: if request.auth != null;
    }
  }
}
```

---

## Troubleshooting

### Flutter Issues:
- **No notifications**: Check FCM token in Firestore, verify Google Services files
- **Build errors**: Run `flutter clean && flutter pub get`

### Backend Issues:
- **Not receiving Firestore updates**: Check Firebase credentials
- **Can't reach ESP32**: Verify ESP32 IP address and network

### ESP32 Issues:
- **WiFi won't connect**: Check SSID/password, ensure 2.4GHz network
- **Not receiving data**: Check Serial Monitor for errors, verify IP registration
- **Download fails**: Ensure Firebase Storage CORS is configured

### Firebase Storage CORS:
Create `cors.json`:
```json
[
  {
    "origin": ["*"],
    "method": ["GET"],
    "maxAgeSeconds": 3600
  }
]
```

Deploy:
```bash
gsutil cors set cors.json gs://your-bucket-name.appspot.com
```

---

## Next Steps

1. **Enhance ESP32 Processing**:
   - Add emotion detection
   - Store results in Firebase
   - Trigger alerts based on emotions

2. **Improve Notifications**:
   - Add custom notification sounds
   - Include emotion indicators
   - Add notification history

3. **Add Features**:
   - Real-time status updates
   - Battery monitoring
   - Offline queueing

---

## File Structure

```
flutter_app/
├── lib/
│   ├── services/
│   │   └── notification_service.dart  ← NEW
│   └── screens/
│       └── mentee_details_page.dart   ← UPDATED
└── pubspec.yaml                        ← UPDATED

render_backend/
├── server.js                           ← NEW
├── package.json                        ← NEW
├── .env.example                        ← NEW
└── .gitignore                          ← NEW

ESP32/
├── esp32_receiver.ino                  ← NEW
└── SECRETS_EXAMPLE.h                   ← NEW
```

---

## Support

For issues:
1. Check Render logs
2. Check ESP32 Serial Monitor
3. Check Firebase Console logs
4. Verify all environment variables

Good luck! 🚀
