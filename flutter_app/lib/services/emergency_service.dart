import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_storage/firebase_storage.dart';

class EmergencyEvent {
  final String id;
  final String menteeId;
  final String menteeName;
  final int menteeNumber;
  final DateTime timestamp;
  final String? audioUrl;
  final List<String> imageUrls;
  final String emotionDetected;
  final String status; // 'new', 'viewed', 'resolved'
  final String mentorId;
  final DateTime? acknowledgedAt;
  final DateTime? resolvedAt;
  final String? notes;
  final int? audioDuration;

  EmergencyEvent({
    required this.id,
    required this.menteeId,
    required this.menteeName,
    required this.menteeNumber,
    required this.timestamp,
    this.audioUrl,
    required this.imageUrls,
    required this.emotionDetected,
    required this.status,
    required this.mentorId,
    this.acknowledgedAt,
    this.resolvedAt,
    this.notes,
    this.audioDuration,
  });

  factory EmergencyEvent.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return EmergencyEvent(
      id: doc.id,
      menteeId: data['menteeId'] ?? '',
      menteeName: data['menteeName'] ?? '',
      menteeNumber: data['menteeNumber'] ?? 0,
      timestamp: (data['timestamp'] as Timestamp?)?.toDate() ?? DateTime.now(),
      audioUrl: data['audioUrl'],
      imageUrls: List<String>.from(data['imageUrls'] ?? []),
      emotionDetected: data['emotionDetected'] ?? 'unknown',
      status: data['status'] ?? 'new',
      mentorId: data['mentorId'] ?? '',
      acknowledgedAt: (data['acknowledgedAt'] as Timestamp?)?.toDate(),
      resolvedAt: (data['resolvedAt'] as Timestamp?)?.toDate(),
      notes: data['notes'],
      audioDuration: data['audioDuration'],
    );
  }

  Map<String, dynamic> toFirestore() {
    return {
      'menteeId': menteeId,
      'menteeName': menteeName,
      'menteeNumber': menteeNumber,
      'timestamp': Timestamp.fromDate(timestamp),
      'audioUrl': audioUrl,
      'imageUrls': imageUrls,
      'emotionDetected': emotionDetected,
      'status': status,
      'mentorId': mentorId,
      'acknowledgedAt': acknowledgedAt != null ? Timestamp.fromDate(acknowledgedAt!) : null,
      'resolvedAt': resolvedAt != null ? Timestamp.fromDate(resolvedAt!) : null,
      'notes': notes,
      'audioDuration': audioDuration,
    };
  }
}

class EmergencyService {
  static final EmergencyService _instance = EmergencyService._internal();
  factory EmergencyService() => _instance;
  EmergencyService._internal();

  final FirebaseFirestore _firestore = FirebaseFirestore.instance;
  final FirebaseStorage _storage = FirebaseStorage.instance;

  // Create a new emergency event
  Future<String> createEmergencyEvent({
    required String menteeId,
    required String menteeName,
    required int menteeNumber,
    required String mentorId,
    String? audioUrl,
    List<String>? imageUrls,
    String emotionDetected = 'panic',
    int? audioDuration,
  }) async {
    try {
      final docRef = await _firestore.collection('emergency_events').add({
        'menteeId': menteeId,
        'menteeName': menteeName,
        'menteeNumber': menteeNumber,
        'timestamp': FieldValue.serverTimestamp(),
        'audioUrl': audioUrl,
        'imageUrls': imageUrls ?? [],
        'emotionDetected': emotionDetected,
        'status': 'new',
        'mentorId': mentorId,
        'acknowledgedAt': null,
        'resolvedAt': null,
        'notes': null,
        'audioDuration': audioDuration,
      });

      print('✅ Emergency event created: ${docRef.id}');
      return docRef.id;
    } catch (e) {
      print('❌ Error creating emergency event: $e');
      rethrow;
    }
  }

  // Get all emergency events for a specific mentee
  Stream<List<EmergencyEvent>> getEmergencyEventsForMentee(String menteeId) {
    return _firestore
        .collection('emergency_events')
        .where('menteeId', isEqualTo: menteeId)
        .snapshots()
        .map((snapshot) {
          final events = snapshot.docs.map((doc) => EmergencyEvent.fromFirestore(doc)).toList();
          events.sort((a, b) => b.timestamp.compareTo(a.timestamp)); // Sort by timestamp descending
          return events;
        });
  }

  // Get all emergency events for a mentor
  Stream<List<EmergencyEvent>> getEmergencyEventsForMentor(String mentorId) {
    return _firestore
        .collection('emergency_events')
        .where('mentorId', isEqualTo: mentorId)
        .snapshots()
        .map((snapshot) {
          final events = snapshot.docs.map((doc) => EmergencyEvent.fromFirestore(doc)).toList();
          events.sort((a, b) => b.timestamp.compareTo(a.timestamp)); // Sort by timestamp descending
          return events;
        });
  }

  // Update emergency status
  Future<void> updateEmergencyStatus(String eventId, String status) async {
    try {
      final updates = <String, dynamic>{
        'status': status,
      };

      if (status == 'viewed') {
        updates['acknowledgedAt'] = FieldValue.serverTimestamp();
      } else if (status == 'resolved') {
        updates['resolvedAt'] = FieldValue.serverTimestamp();
      }

      await _firestore.collection('emergency_events').doc(eventId).update(updates);
      print('✅ Emergency status updated: $status');
    } catch (e) {
      print('❌ Error updating emergency status: $e');
      rethrow;
    }
  }

  // Add notes to an emergency event
  Future<void> addNotes(String eventId, String notes) async {
    try {
      await _firestore.collection('emergency_events').doc(eventId).update({
        'notes': notes,
      });
      print('✅ Notes added to emergency event');
    } catch (e) {
      print('❌ Error adding notes: $e');
      rethrow;
    }
  }

  // Get count of unresolved emergencies for a mentor
  Future<int> getUnresolvedCount(String mentorId) async {
    try {
      final snapshot = await _firestore
          .collection('emergency_events')
          .where('mentorId', isEqualTo: mentorId)
          .where('status', whereIn: ['new', 'viewed'])
          .get();
      return snapshot.docs.length;
    } catch (e) {
      print('❌ Error getting unresolved count: $e');
      return 0;
    }
  }
}
