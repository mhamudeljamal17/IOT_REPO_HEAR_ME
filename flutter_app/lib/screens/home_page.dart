import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'alerts_page.dart';
import 'settings_page.dart';
import 'mentee_details_page.dart';
import '../services/mentee_service.dart';

// Mentee data is stored in Firestore; no local model required here.

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  int _selectedFilter = 0; // 0=All,1=Alerts,2=Emergency
  final int _currentIndex = 0; // Home tab index

  String? get _currentMentorId => FirebaseAuth.instance.currentUser?.uid;

  Future<void> _showAddMenteeDialog() async {
    String name = '';
    String phone = '';
    String ageStr = '';

    final formKey = GlobalKey<FormState>();

    final result = await showDialog<bool>(
      context: context,
      builder:
          (context) => AlertDialog(
            title: const Text('Add Mentee'),
            content: Form(
              key: formKey,
              child: SingleChildScrollView(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    TextFormField(
                      decoration: const InputDecoration(labelText: 'Name'),
                      validator:
                          (v) =>
                              (v == null || v.trim().isEmpty)
                                  ? 'Enter a name'
                                  : null,
                      onSaved: (v) => name = v!.trim(),
                    ),
                    TextFormField(
                      decoration: const InputDecoration(
                        labelText: 'Phone (optional)',
                      ),
                      onSaved: (v) => phone = v?.trim() ?? '',
                    ),
                    TextFormField(
                      decoration: const InputDecoration(labelText: 'Age'),
                      keyboardType: TextInputType.number,
                      validator: (v) {
                        if (v == null || v.trim().isEmpty) return 'Enter age';
                        final n = int.tryParse(v);
                        if (n == null || n <= 0) return 'Enter a valid age';
                        return null;
                      },
                      onSaved: (v) => ageStr = v!.trim(),
                    ),
                  ],
                ),
              ),
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, false),
                child: const Text('Cancel'),
              ),
              ElevatedButton(
                onPressed: () async {
                  if (formKey.currentState?.validate() ?? false) {
                    formKey.currentState?.save();

                    // Get current mentor ID
                    final mentorId = _currentMentorId;
                    if (mentorId == null) {
                      Navigator.pop(context, false);
                      return;
                    }

                    // Generate unique mentee number
                    final menteeNumber = await MenteeService.generateMenteeNumber();

                    final col = FirebaseFirestore.instance.collection(
                      'mentees',
                    );
                    final docRef = col.doc();
                    await docRef.set({
                      'id': docRef.id,
                      'menteeNumber': menteeNumber, // Auto-generated unique number
                      'mentorId':
                          mentorId, // Associate mentee with current mentor
                      'name': name,
                      'phone': phone,
                      'age': int.parse(ageStr),
                      'createdAt': FieldValue.serverTimestamp(),
                    });
                    
                    // Show success dialog with mentee number
                    Navigator.pop(context, true);
                    if (context.mounted) {
                      showDialog(
                        context: context,
                        builder: (context) => AlertDialog(
                          title: const Text('✅ Mentee Added'),
                          content: Column(
                            mainAxisSize: MainAxisSize.min,
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text('Name: $name'),
                              const SizedBox(height: 16),
                              Container(
                                padding: const EdgeInsets.all(16),
                                decoration: BoxDecoration(
                                  color: Colors.green.shade50,
                                  borderRadius: BorderRadius.circular(8),
                                  border: Border.all(color: Colors.green),
                                ),
                                child: Column(
                                  children: [
                                    const Text(
                                      'Mentee Number',
                                      style: TextStyle(
                                        fontSize: 12,
                                        color: Colors.grey,
                                      ),
                                    ),
                                    const SizedBox(height: 4),
                                    Text(
                                      '$menteeNumber',
                                      style: const TextStyle(
                                        fontSize: 32,
                                        fontWeight: FontWeight.bold,
                                        color: Colors.green,
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                              const SizedBox(height: 12),
                              const Text(
                                'Give this number to the mentee. They will use it to identify themselves on the ESP32 device.',
                                style: TextStyle(
                                  fontSize: 12,
                                  color: Colors.grey,
                                ),
                              ),
                            ],
                          ),
                          actions: [
                            TextButton(
                              onPressed: () => Navigator.pop(context),
                              child: const Text('OK'),
                            ),
                          ],
                        ),
                      );
                    }
                  }
                },
                child: const Text('Add'),
              ),
            ],
          ),
    );

    if (result == true) setState(() => _selectedFilter = 0);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        leading: IconButton(
          icon: const Icon(Icons.notifications_none),
          onPressed: () {},
        ),
      ),
      body: Padding(
        padding: const EdgeInsets.all(12.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Card(
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(8),
              ),
              child: ListTile(
                leading: const CircleAvatar(child: Icon(Icons.check)),
                title: const Text('No Active Emergencies'),
              ),
            ),
            const SizedBox(height: 12),
            const Text('Your Mentees', style: TextStyle(fontSize: 16)),
            const SizedBox(height: 12),
            ToggleButtons(
              isSelected: [
                _selectedFilter == 0,
                _selectedFilter == 2,
              ],
              onPressed: (i) => setState(() => _selectedFilter = i == 0 ? 0 : 2),
              children: const [
                Padding(
                  padding: EdgeInsets.symmetric(horizontal: 18),
                  child: Text('All'),
                ),
                Padding(
                  padding: EdgeInsets.symmetric(horizontal: 18),
                  child: Text('Emergency'),
                ),
              ],
            ),
            const SizedBox(height: 20),
            Expanded(
              child: StreamBuilder<QuerySnapshot<Map<String, dynamic>>>(
                stream:
                    _selectedFilter == 0
                        ? FirebaseFirestore.instance
                            .collection('mentees')
                            .where('mentorId', isEqualTo: _currentMentorId)
                            .snapshots()
                        : FirebaseFirestore.instance
                            .collection('mentees')
                            .where('mentorId', isEqualTo: _currentMentorId)
                            .where('emergency', isEqualTo: true)
                            .snapshots(),
                builder: (context, snapshot) {
                  if (snapshot.hasError) {
                    return Center(child: Text('Error: ${snapshot.error}'));
                  }
                  if (snapshot.connectionState == ConnectionState.waiting) {
                    return const Center(child: CircularProgressIndicator());
                  }
                  final docs =
                      List<QueryDocumentSnapshot<Map<String, dynamic>>>.from(
                        snapshot.data?.docs ?? [],
                      );
                  // Sort by createdAt in the app instead of in the query
                  docs.sort((a, b) {
                    final aTs = a.data()['createdAt'] as Timestamp?;
                    final bTs = b.data()['createdAt'] as Timestamp?;
                    final aMillis = aTs?.millisecondsSinceEpoch ?? 0;
                    final bMillis = bTs?.millisecondsSinceEpoch ?? 0;
                    return bMillis.compareTo(aMillis);
                  });
                  if (docs.isEmpty) {
                    return Center(
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 24.0),
                        child: Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(
                              Icons.group_off,
                              size: 64,
                              color: Colors.grey[400],
                            ),
                            const SizedBox(height: 16),
                            const Text(
                              'No mentees yet',
                              style: TextStyle(
                                fontSize: 18,
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                            const SizedBox(height: 8),
                            const Text(
                              'You have not added any mentees. Tap "Add Mentee" to create one.',
                              textAlign: TextAlign.center,
                              style: TextStyle(color: Colors.black54),
                            ),
                            const SizedBox(height: 16),
                          ],
                        ),
                      ),
                    );
                  }

                  return ListView.builder(
                    padding: EdgeInsets.zero,
                    itemCount: docs.length,
                    itemBuilder: (context, index) {
                      final doc = docs[index];
                      final data = doc.data();
                      final docId = doc.id;
                      final name = data['name'] as String? ?? '';
                      final phone = data['phone'] as String? ?? '';
                      final age = data['age'] as int? ?? 0;
                      final menteeNumber = data['menteeNumber'] as int? ?? 0;
                      final createdAtTimestamp = data['createdAt'] as Timestamp?;
                      final createdAt = createdAtTimestamp?.toDate();
                      final subtitle =
                          phone.isNotEmpty
                              ? phone
                              : (age != 0 ? 'Age: $age' : '');
                      return MenteeTile(
                        menteeDocId: docId,
                        name: name,
                        subtitle: subtitle,
                        age: age,
                        phone: phone,
                        menteeNumber: menteeNumber,
                        createdAt: createdAt,
                      );
                    },
                  );
                },
              ),
            ),
          ],
        ),
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _showAddMenteeDialog,
        icon: const Icon(Icons.person_add),
        label: const Text('Add Mentee'),
      ),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _currentIndex,
        items: const [
          BottomNavigationBarItem(icon: Icon(Icons.home), label: 'Home'),
          BottomNavigationBarItem(
            icon: Icon(Icons.notifications),
            label: 'Alerts',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
        onTap: (index) {
          if (index == 0) {
            // Already on Home
            return;
          } else if (index == 1) {
            Navigator.pushReplacement(
              context,
              MaterialPageRoute(builder: (_) => const AlertsPage()),
            );
          } else if (index == 2) {
            Navigator.pushReplacement(
              context,
              MaterialPageRoute(builder: (_) => const SettingsPage()),
            );
          }
        },
      ),
    );
  }
}

class MenteeTile extends StatelessWidget {
  final String menteeDocId;
  final String name;
  final String subtitle;
  final int age;
  final String phone;
  final int menteeNumber;  // Changed from menteeId to menteeNumber
  final DateTime? createdAt;

  const MenteeTile({
    super.key,
    required this.menteeDocId,
    required this.name,
    required this.subtitle,
    required this.age,
    required this.phone,
    required this.menteeNumber,
    this.createdAt,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.symmetric(vertical: 8),
      child: ListTile(
        leading: ClipRRect(
          borderRadius: BorderRadius.circular(8),
          child: Container(
            width: 56,
            height: 56,
            color: Colors.grey[300],
            child: const Icon(Icons.person, size: 32),
          ),
        ),
        title: Text(
          name,
          style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
        ),
        subtitle: Text(subtitle),
        trailing: ElevatedButton(
          onPressed: () {
            Navigator.push(
              context,
              MaterialPageRoute(
                builder: (context) => MenteeDetailsPage(
                  menteeDocId: menteeDocId,
                  name: name,
                  age: age,
                  phone: phone,
                  menteeNumber: menteeNumber,
                  createdAt: createdAt,
                ),
              ),
            );
          },
          child: const Text('View'),
        ),
      ),
    );
  }
}
