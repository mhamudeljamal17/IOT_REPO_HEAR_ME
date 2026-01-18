import 'package:cloud_firestore/cloud_firestore.dart';
import 'dart:math';

class MenteeService {
  static final FirebaseFirestore _firestore = FirebaseFirestore.instance;
  static final Random _random = Random();

  /// Generates a random unique 4-digit mentee number (1000-9999)
  /// Checks Firestore to ensure no duplicates
  static Future<int> generateMenteeNumber() async {
    int attempts = 0;
    const maxAttempts = 20;

    while (attempts < maxAttempts) {
      // Generate random 4-digit number (1000-9999)
      final randomNumber = 1000 + _random.nextInt(9000);

      // Check if this number already exists
      final exists = await menteeNumberExists(randomNumber);

      if (!exists) {
        print('✅ Generated unique mentee number: $randomNumber');
        return randomNumber;
      }

      attempts++;
      print('⚠️ Number $randomNumber already exists, trying again...');
    }

    // Fallback: use timestamp-based number if too many collisions
    final fallback = 1000 + (DateTime.now().millisecondsSinceEpoch % 8999);
    print('⚠️ Using fallback number: $fallback');
    return fallback;
  }

  /// Check if a mentee number already exists
  static Future<bool> menteeNumberExists(int number) async {
    final query = await _firestore
        .collection('mentees')
        .where('menteeNumber', isEqualTo: number)
        .limit(1)
        .get();

    return query.docs.isNotEmpty;
  }

  /// Get mentee by their unique number
  static Future<DocumentSnapshot?> getMenteeByNumber(int menteeNumber) async {
    final query = await _firestore
        .collection('mentees')
        .where('menteeNumber', isEqualTo: menteeNumber)
        .limit(1)
        .get();

    if (query.docs.isEmpty) return null;
    return query.docs.first;
  }
}
