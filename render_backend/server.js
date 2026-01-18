const express = require('express');
const admin = require('firebase-admin');
const cors = require('cors');
const axios = require('axios');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());

// Initialize Firebase Admin SDK
try {
  const serviceAccount = JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT || '{}');
  
  admin.initializeApp({
    credential: admin.credential.cert(serviceAccount),
  });
  
  console.log('Firebase Admin initialized successfully');
} catch (error) {
  console.error('Error initializing Firebase Admin:', error);
}

const db = admin.firestore();
const messaging = admin.messaging();

// Parse ESP32 devices from environment
const esp32Devices = new Map();
if (process.env.ESP32_DEVICES) {
  process.env.ESP32_DEVICES.split(',').forEach(device => {
    const [menteeId, ipAddress] = device.split(':');
    if (menteeId && ipAddress) {
      esp32Devices.set(menteeId.trim(), ipAddress.trim());
    }
  });
}

console.log('Registered ESP32 devices:', Array.from(esp32Devices.entries()));

// Health check endpoint
app.get('/', (req, res) => {
  res.json({
    status: 'running',
    service: 'HearMe Backend',
    timestamp: new Date().toISOString(),
    esp32Devices: Array.from(esp32Devices.keys())
  });
});

// Endpoint to register ESP32 device
app.post('/api/register-device', async (req, res) => {
  try {
    const { menteeId, ipAddress } = req.body;
    
    if (!menteeId || !ipAddress) {
      return res.status(400).json({ error: 'menteeId and ipAddress required' });
    }
    
    esp32Devices.set(menteeId, ipAddress);
    
    // Store in Firestore
    await db.collection('esp32_devices').doc(menteeId).set({
      ipAddress,
      lastUpdated: admin.firestore.FieldValue.serverTimestamp(),
      status: 'online'
    });
    
    res.json({ 
      success: true, 
      message: 'Device registered successfully',
      menteeId,
      ipAddress
    });
  } catch (error) {
    console.error('Error registering device:', error);
    res.status(500).json({ error: error.message });
  }
});

// Send data to ESP32
async function sendToESP32(menteeId, data) {
  const ipAddress = esp32Devices.get(menteeId);
  
  if (!ipAddress) {
    console.log(`No ESP32 found for mentee: ${menteeId}`);
    return false;
  }
  
  try {
    console.log(`Sending to ESP32 at ${ipAddress}:`, data);
    
    const response = await axios.post(`http://${ipAddress}/api/recording`, data, {
      timeout: 5000,
      headers: {
        'Content-Type': 'application/json'
      }
    });
    
    console.log(`ESP32 response:`, response.data);
    return true;
  } catch (error) {
    console.error(`Error sending to ESP32 ${ipAddress}:`, error.message);
    return false;
  }
}

// Endpoint to send notification (called by Firebase or manually)
app.post('/api/send-notification', async (req, res) => {
  try {
    const { fcmToken, title, body, data } = req.body;
    
    if (!fcmToken) {
      return res.status(400).json({ error: 'FCM token required' });
    }
    
    const message = {
      token: fcmToken,
      notification: {
        title: title || 'New Notification',
        body: body || '',
      },
      data: data || {},
      android: {
        priority: 'high',
      },
      apns: {
        payload: {
          aps: {
            sound: 'default',
          },
        },
      },
    };
    
    const response = await messaging.send(message);
    console.log('Notification sent:', response);
    
    res.json({ success: true, messageId: response });
  } catch (error) {
    console.error('Error sending notification:', error);
    res.status(500).json({ error: error.message });
  }
});

// Endpoint to forward recording to ESP32
app.post('/api/forward-to-esp32', async (req, res) => {
  try {
    const { menteeId, recordingId, downloadUrl, fileName } = req.body;
    
    if (!menteeId || !downloadUrl) {
      return res.status(400).json({ error: 'menteeId and downloadUrl required' });
    }
    
    const esp32Data = {
      recordingId,
      downloadUrl,
      fileName,
      timestamp: new Date().toISOString()
    };
    
    const success = await sendToESP32(menteeId, esp32Data);
    
    if (success) {
      // Update Firestore to mark as sent to ESP32
      if (recordingId) {
        await db.collection('voice_recordings').doc(recordingId).update({
          sentToESP32: true,
          sentToESP32At: admin.firestore.FieldValue.serverTimestamp()
        });
      }
      
      res.json({ success: true, message: 'Data forwarded to ESP32' });
    } else {
      res.status(500).json({ error: 'Failed to forward to ESP32' });
    }
  } catch (error) {
    console.error('Error forwarding to ESP32:', error);
    res.status(500).json({ error: error.message });
  }
});

// Listen to Firestore changes (notifications collection)
const notificationsRef = db.collection('notifications');
notificationsRef.where('status', '==', 'pending').onSnapshot(async (snapshot) => {
  snapshot.docChanges().forEach(async (change) => {
    if (change.type === 'added') {
      const doc = change.doc;
      const data = doc.data();
      
      console.log('New notification detected:', doc.id);
      
      try {
        // Send FCM notification
        if (data.to) {
          await messaging.send({
            token: data.to,
            notification: {
              title: data.title,
              body: data.body,
            },
            data: data.data || {},
          });
          console.log('FCM sent to:', data.to);
        }
        
        // Forward to ESP32 if recording data exists
        if (data.menteeId && data.data?.downloadUrl) {
          await sendToESP32(data.menteeId, {
            recordingId: data.data.recordingId,
            downloadUrl: data.data.downloadUrl,
            fileName: data.data.fileName,
            timestamp: new Date().toISOString()
          });
        }
        
        // Update status
        await doc.ref.update({
          status: 'sent',
          sentAt: admin.firestore.FieldValue.serverTimestamp()
        });
      } catch (error) {
        console.error('Error processing notification:', error);
        await doc.ref.update({
          status: 'failed',
          error: error.message
        });
      }
    }
  });
});

console.log('Listening to Firestore notifications...');

// Start server
app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
