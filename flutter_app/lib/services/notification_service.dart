import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_database/firebase_database.dart';

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

  void _handleForegroundMessage(RemoteMessage message) {
    print('📬 Foreground message: ${message.notification?.title}');
    
    if (message.notification != null) {
      _showLocalNotification(message);
    }
  }

  void _handleBackgroundNotificationTap(RemoteMessage message) {
    print('🔔 Notification tapped: ${message.notification?.title}');
    print('Data: ${message.data}');
    
    // Handle navigation based on notification data
    if (message.data.containsKey('menteeNumber')) {
      String menteeNumber = message.data['menteeNumber'] ?? '';
      print('Navigating to mentee: $menteeNumber');
      // TODO: Implement navigation to mentee details page
      // You can use GetX, Navigator, or your routing solution
    }
  }

  void _onNotificationTap(NotificationResponse response) {
    print('📲 Local notification tapped: ${response.payload}');
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

    await _localNotifications.show(
      message.hashCode,
      message.notification?.title ?? 'New Notification',
      message.notification?.body ?? '',
      details,
      payload: message.data.toString(),
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

}
