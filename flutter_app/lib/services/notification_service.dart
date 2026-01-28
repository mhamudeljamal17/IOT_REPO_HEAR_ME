import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import '../main.dart';
import '../screens/mentee_details_page.dart';

// Top-level function for background messages
Future<void> _firebaseMessagingBackgroundHandler(RemoteMessage message) async {
  print('📬 Background message: ${message.notification?.title}');
  
  // Store notification in Firestore with proper timestamp
  try {
    final timestampStr = message.data['timestamp'];
    DateTime notificationTime;
    
    if (timestampStr != null && timestampStr.isNotEmpty) {
      notificationTime = DateTime.parse(timestampStr);
    } else {
      notificationTime = DateTime.now();
    }
    
    await FirebaseFirestore.instance.collection('notifications').add({
      'title': message.notification?.title ?? 'Alert',
      'body': message.notification?.body ?? '',
      'menteeNumber': int.tryParse(message.data['menteeNumber'] ?? '0') ?? 0,
      'menteeId': message.data['menteeId'] ?? '',
      'mentorId': message.data['mentorId'] ?? '',
      'timestamp': Timestamp.fromDate(notificationTime),
      'isRead': false,
      'type': message.data['type'] ?? 'mentor_alert',
      'imagePath': message.data['imagePath'] ?? '',
      'audioPath': message.data['audioPath'] ?? '',
    });
    
    // Create emergency record if emotion is angry or panic
    final emotion = message.data['emotion'] ?? '';
    if (emotion == 'angry' || emotion == 'panic') {
      try {
        final menteeNumber = int.tryParse(message.data['menteeNumber'] ?? '0') ?? 0;
        
        if (menteeNumber > 0) {
          await FirebaseFirestore.instance.collection('emergencies').add({
            'menteeNumber': menteeNumber,
            'mentorId': message.data['mentorId'] ?? '',
            'emotion': emotion,
            'timestamp': Timestamp.fromDate(notificationTime),
            'audioPath': message.data['audioPath'],
            'imagePath': message.data['imagePath'],
            'detectionId': message.data['detectionId'],
            'status': 'new',
          });
          print('✅ Emergency record created for mentee #$menteeNumber');
        }
      } catch (emergencyError) {
        print('❌ Error creating emergency record: $emergencyError');
      }
    }
    
    print('✅ Background notification saved to Firestore with timestamp: $notificationTime');
  } catch (e) {
    print('❌ Error saving background notification to Firestore: $e');
  }
}

class NotificationService {
  static final NotificationService _instance = NotificationService._internal();
  factory NotificationService() => _instance;
  NotificationService._internal();

  final FirebaseMessaging _messaging = FirebaseMessaging.instance;
  final FlutterLocalNotificationsPlugin _localNotifications =
      FlutterLocalNotificationsPlugin();
  final FirebaseDatabase _realtimeDb = FirebaseDatabase.instance;

  bool _initialized = false;

  Future<void> initialize() async {
    if (_initialized) return;

    // Request notification permissions
    NotificationSettings settings = await _messaging.requestPermission(
      alert: true,
      badge: true,
      sound: true,
      provisional: false,
    );

    if (settings.authorizationStatus == AuthorizationStatus.authorized) {
      print('✅ User granted notification permission');
    } else {
      print('⚠️ User declined or has not accepted permission');
    }

    // Initialize local notifications
    const AndroidInitializationSettings androidSettings =
        AndroidInitializationSettings('@mipmap/ic_launcher');
    
    const DarwinInitializationSettings iosSettings =
        DarwinInitializationSettings(
      requestAlertPermission: true,
      requestBadgePermission: true,
      requestSoundPermission: true,
    );

    const InitializationSettings initSettings = InitializationSettings(
      android: androidSettings,
      iOS: iosSettings,
    );

    await _localNotifications.initialize(
      initSettings,
      onDidReceiveNotificationResponse: _onNotificationTap,
    );

    // Create Android notification channel
    await _createNotificationChannel();

    // Handle foreground messages (app is open)
    FirebaseMessaging.onMessage.listen(_handleForegroundMessage);

    // Handle background messages (app is in background)
    FirebaseMessaging.onBackgroundMessage(_firebaseMessagingBackgroundHandler);

    // Handle notification tap when app is in background or terminated
    FirebaseMessaging.onMessageOpenedApp.listen(_handleBackgroundNotificationTap);

    _initialized = true;
    print('✅ Notification service initialized');
  }

  Future<void> _createNotificationChannel() async {
    const AndroidNotificationChannel channel = AndroidNotificationChannel(
      'hearme_channel',
      'HearMe Notifications',
      description: 'Notifications for HearMe app',
      importance: Importance.max,
      playSound: true,
      
    );

    await _localNotifications
        .resolvePlatformSpecificImplementation<
            AndroidFlutterLocalNotificationsPlugin>()
        ?.createNotificationChannel(channel);
  }

  Future<String?> getDeviceToken() async {
    return await _messaging.getToken();
  }

  Future<void> saveTokenToFirestore(String userId) async {
    String? token = await getDeviceToken();
    if (token != null) {
      await FirebaseFirestore.instance
          .collection('users')
          .doc(userId)
          .set({
        'fcmToken': token,
        'lastTokenUpdate': FieldValue.serverTimestamp(),
      }, SetOptions(merge: true));
      
      print('✅ FCM token saved for user: $userId');
    }
  }

  void _handleForegroundMessage(RemoteMessage message) async {
    print('📬 Foreground message: ${message.notification?.title}');
    
    if (message.notification != null) {
      _showLocalNotification(message);
      
      // Store notification in Firestore with proper timestamp
      try {
        final timestampStr = message.data['timestamp'];
        DateTime notificationTime;
        
        if (timestampStr != null && timestampStr.isNotEmpty) {
          notificationTime = DateTime.parse(timestampStr);
        } else {
          notificationTime = DateTime.now();
        }
        
        await FirebaseFirestore.instance.collection('notifications').add({
          'title': message.notification?.title ?? 'Alert',
          'body': message.notification?.body ?? '',
          'menteeNumber': int.tryParse(message.data['menteeNumber'] ?? '0') ?? 0,
          'menteeId': message.data['menteeId'] ?? '',
          'mentorId': message.data['mentorId'] ?? '',
          'timestamp': Timestamp.fromDate(notificationTime),
          'isRead': false,
          'type': message.data['type'] ?? 'mentor_alert',
          'imagePath': message.data['imagePath'] ?? '',
          'audioPath': message.data['audioPath'] ?? '',
        });
        
        // Create emergency record if emotion is angry or panic
        final emotion = message.data['emotion'] ?? '';
        if (emotion == 'angry' || emotion == 'panic') {
          try {
            final menteeNumber = int.tryParse(message.data['menteeNumber'] ?? '0') ?? 0;
            
            if (menteeNumber > 0) {
              await FirebaseFirestore.instance.collection('emergencies').add({
                'menteeNumber': menteeNumber,
                'mentorId': message.data['mentorId'] ?? '',
                'emotion': emotion,
                'timestamp': Timestamp.fromDate(notificationTime),
                'audioPath': message.data['audioPath'],
                'imagePath': message.data['imagePath'],
                'detectionId': message.data['detectionId'],
                'status': 'new',
              });
              print('✅ Emergency record created for mentee #$menteeNumber');
            }
          } catch (e) {
            print('❌ Error creating emergency record: $e');
          }
        }
        
        print('✅ Notification saved to Firestore with timestamp: $notificationTime');
      } catch (e) {
        print('❌ Error saving notification to Firestore: $e');
      }
    }
  }

  void _handleBackgroundNotificationTap(RemoteMessage message) async {
    print('🔔 Notification tapped: ${message.notification?.title}');
    print('Data: ${message.data}');
    
    // Handle navigation based on notification data
    if (message.data.containsKey('menteeNumber')) {
      int menteeNumber = int.tryParse(message.data['menteeNumber']?.toString() ?? '0') ?? 0;
      print('Navigating to mentee #$menteeNumber');
      await _navigateToMenteeDetails(menteeNumber);
    }
  }

  void _onNotificationTap(NotificationResponse response) async {
    print('📲 Local notification tapped: ${response.payload}');
    
    // Parse payload to get menteeNumber
    if (response.payload != null && response.payload!.isNotEmpty) {
      try {
        // The payload format is like: menteeNumber: xxx
        final payloadStr = response.payload!;
        final menteeNumberMatch = RegExp(r'menteeNumber[:\s]+(\d+)').firstMatch(payloadStr);
        
        if (menteeNumberMatch != null) {
          int menteeNumber = int.tryParse(menteeNumberMatch.group(1) ?? '0') ?? 0;
          print('Extracted menteeNumber: $menteeNumber');
          await _navigateToMenteeDetails(menteeNumber);
        }
      } catch (e) {
        print('Error parsing notification payload: $e');
      }
    }
  }

  Future<void> _showLocalNotification(RemoteMessage message) async {
    const AndroidNotificationDetails androidDetails =
        AndroidNotificationDetails(
      'hearme_channel',
      'HearMe Notifications',
      channelDescription: 'Notifications for HearMe app',
      importance: Importance.max,
      priority: Priority.high,
      showWhen: true,
      
    );

    const DarwinNotificationDetails iosDetails = DarwinNotificationDetails(
      presentAlert: true,
      presentBadge: true,
      presentSound: true,
    );

    const NotificationDetails details = NotificationDetails(
      android: androidDetails,
      iOS: iosDetails,
    );

    // Create payload with menteeNumber for navigation
    String payload = 'menteeNumber: ${message.data['menteeNumber'] ?? '0'}';

    await _localNotifications.show(
      message.hashCode,
      message.notification?.title ?? 'New Notification',
      message.notification?.body ?? '',
      details,
      payload: payload,
    );
  }

  Future<void> sendNotificationToMentee({
    required String menteeId,
    required String title,
    required String body,
    Map<String, dynamic>? data,
  }) async {
    try {
      print('🔔 Creating notification for ESP32 device (menteeId: $menteeId)');
      
      // Create notification document in Firestore
      // This will be picked up by your Render backend and forwarded to ESP32
      final notificationData = {
        'menteeId': menteeId,
        'title': title,
        'body': body,
        'data': data ?? {},
        'timestamp': FieldValue.serverTimestamp(),
        'status': 'pending',
        'type': 'esp32_audio', // Indicates this is for ESP32, not mobile
      };
      
      final docRef = await FirebaseFirestore.instance
          .collection('notifications')
          .add(notificationData);

      print('✅ Notification created for ESP32 with ID: ${docRef.id}');
      print('📡 Backend will forward to ESP32 device');
    } catch (e) {
      print('❌ Error creating notification: $e');
    }
  }



  Future<void> clearTokenOnSignOut(String userId) async {
  await FirebaseFirestore.instance
      .collection('users')
      .doc(userId)
      .update({'fcmToken': FieldValue.delete()});
}

Future<void> triggerEspAudio({
  required int menteeNumber,
  required String audioUrl,
  required int durationSeconds,
}) async {
  final ref = FirebaseDatabase.instance
      .ref()
      .child('esp_commands')
      .child(menteeNumber.toString());

  await ref.set({
    'status': 'new',
    'audioUrl': audioUrl,
    'duration': durationSeconds,
    'timestamp': ServerValue.timestamp,
  });

  print('✅ ESP trigger sent for mentee $menteeNumber');
}

  /// Navigate to mentee details page using menteeNumber
  Future<void> _navigateToMenteeDetails(int menteeNumber) async {
    try {
      if (menteeNumber <= 0) {
        print('⚠️ Cannot navigate: invalid menteeNumber ($menteeNumber)');
        return;
      }

      // Query mentee data from Firestore by menteeNumber
      final menteeQuery = await FirebaseFirestore.instance
          .collection('mentees')
          .where('menteeNumber', isEqualTo: menteeNumber)
          .limit(1)
          .get();

      if (menteeQuery.docs.isEmpty) {
        print('⚠️ Mentee not found with menteeNumber: $menteeNumber');
        return;
      }

      final menteeDoc = menteeQuery.docs.first;
      final menteeData = menteeDoc.data();
      final menteeDocId = menteeDoc.id;
      
      // Navigate using global navigator key
      navigatorKey.currentState?.push(
        MaterialPageRoute(
          builder: (context) => MenteeDetailsPage(
            menteeDocId: menteeDocId,
            name: menteeData['name'] ?? 'Unknown',
            age: menteeData['age'] ?? 0,
            phone: menteeData['phone'] ?? '',
            menteeNumber: menteeData['menteeNumber'] ?? 0,
            createdAt: (menteeData['createdAt'] as Timestamp?)?.toDate(),
          ),
        ),
      );

      print('✅ Navigated to mentee #$menteeNumber: ${menteeData['name']}');
    } catch (e) {
      print('❌ Error navigating to mentee details: $e');
    }
  }

}
