import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:intl/intl.dart';
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
  String _mentorName = 'Mentor';
  String _searchQuery = '';
  final TextEditingController _searchController = TextEditingController();

  String? get _currentMentorId => FirebaseAuth.instance.currentUser?.uid;

  @override
  void initState() {
    super.initState();
    _loadMentorName();
  }

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  Future<void> _loadMentorName() async {
    final user = FirebaseAuth.instance.currentUser;
    if (user != null) {
      try {
        final doc = await FirebaseFirestore.instance
            .collection('users')
            .doc(user.uid)
            .get();
        
        String name;
        if (doc.exists && doc.data()?['name'] != null) {
          name = doc.data()!['name'];
        } else if (user.displayName != null && user.displayName!.isNotEmpty) {
          name = user.displayName!;
        } else if (user.email != null) {
          name = user.email!.split('@')[0];
        } else {
          name = 'Mentor';
        }
        
        setState(() {
          _mentorName = name;
        });
      } catch (e) {
        print('Error loading mentor name: $e');
        if (user.email != null) {
          setState(() {
            _mentorName = user.email!.split('@')[0];
          });
        }
      }
    }
  }

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

  void _showNotifications() {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (context) => DraggableScrollableSheet(
        initialChildSize: 0.7,
        minChildSize: 0.5,
        maxChildSize: 0.95,
        expand: false,
        builder: (context, scrollController) => Column(
          children: [
            Container(
              padding: const EdgeInsets.all(16),
              child: Row(
                children: [
                  const Text(
                    'Notifications',
                    style: TextStyle(
                      fontSize: 20,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  const Spacer(),
                  IconButton(
                    icon: const Icon(Icons.close),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
            ),
            const Divider(height: 1),
            Expanded(
              child: StreamBuilder<QuerySnapshot>(
                stream: FirebaseFirestore.instance
                    .collection('notifications')
                    .orderBy('timestamp', descending: true)
                    .snapshots(),
                builder: (context, snapshot) {
                  if (snapshot.hasError) {
                    return Center(
                      child: Text('Error: ${snapshot.error}'),
                    );
                  }

                  if (snapshot.connectionState == ConnectionState.waiting) {
                    return const Center(
                      child: CircularProgressIndicator(),
                    );
                  }

                  final allNotifications = snapshot.data?.docs ?? [];
                  final notifications = allNotifications.where((doc) {
                    final data = doc.data() as Map<String, dynamic>;
                    return data['mentorId'] == _currentMentorId;
                  }).toList();

                  if (notifications.isEmpty) {
                    return const Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Icon(Icons.notifications_none, size: 64, color: Colors.grey),
                          SizedBox(height: 16),
                          Text('No notifications'),
                        ],
                      ),
                    );
                  }

                  return ListView.builder(
                    controller: scrollController,
                    padding: const EdgeInsets.all(16),
                    itemCount: notifications.length,
                    itemBuilder: (context, index) {
                      final notif = notifications[index].data() as Map<String, dynamic>;
                      
                      DateTime timestamp;
                      final timestampField = notif['timestamp'];
                      
                      if (timestampField is Timestamp) {
                        timestamp = timestampField.toDate();
                      } else if (timestampField is String) {
                        timestamp = DateTime.tryParse(timestampField) ?? DateTime.now();
                      } else if (timestampField is int) {
                        // Check if it's in seconds or milliseconds
                        if (timestampField > 10000000000) {
                          // Milliseconds
                          timestamp = DateTime.fromMillisecondsSinceEpoch(timestampField);
                        } else {
                          // Seconds
                          timestamp = DateTime.fromMillisecondsSinceEpoch(timestampField * 1000);
                        }
                      } else if (timestampField == null) {
                        // Try createdAt or other common timestamp fields
                        final createdAt = notif['createdAt'];
                        if (createdAt is Timestamp) {
                          timestamp = createdAt.toDate();
                        } else {
                          timestamp = DateTime.now();
                        }
                      } else {
                        timestamp = DateTime.now();
                      }
                      
                      final title = notif['title'] as String? ?? 'Notification';
                      final body = notif['body'] as String? ?? '';
                      final isRead = notif['isRead'] as bool? ?? false;
                      
                      // Try multiple possible fields for mentee ID
                      final menteeId = notif['menteeId'] as String? ?? 
                                      notif['menteeDocId'] as String? ?? 
                                      notif['userId'] as String? ?? '';
                      
                      // Check if mentee number is directly in notification
                      final directMenteeNumber = notif['menteeNumber'] as int? ?? 
                                                 notif['mentee_number'] as int? ?? 0;
                      
                      // Debug: Print notification data
                      print('Notification data: menteeId=$menteeId, menteeNumber=$directMenteeNumber, all data: $notif');

                      return Card(
                        margin: const EdgeInsets.only(bottom: 12),
                        color: isRead ? Colors.white : Colors.orange.shade50,
                        child: menteeId.isNotEmpty && directMenteeNumber == 0
                            ? FutureBuilder<DocumentSnapshot>(
                                future: FirebaseFirestore.instance
                                    .collection('mentees')
                                    .doc(menteeId)
                                    .get(),
                                builder: (context, menteeSnapshot) {
                                  int menteeNumber = directMenteeNumber;
                                  if (menteeSnapshot.hasData && menteeSnapshot.data?.exists == true) {
                                    final menteeData = menteeSnapshot.data!.data() as Map<String, dynamic>?;
                                    menteeNumber = menteeData?['menteeNumber'] as int? ?? directMenteeNumber;
                                  }
                                  
                                  String displayTitle = title;
                                  String displayBody = body;
                                  if (menteeNumber > 0) {
                                    displayTitle = title.replaceAll('Mentee', 'Mentee #$menteeNumber').replaceAll('mentee', 'Mentee #$menteeNumber');
                                    displayBody = body.replaceAll('Mentee', 'Mentee #$menteeNumber').replaceAll('mentee', 'Mentee #$menteeNumber');
                                  }

                                  return ListTile(
                                    onTap: () async {
                                      // Mark as read
                                      await notifications[index].reference.update({'isRead': true});
                                      
                                      // Navigate to mentee details if we have the data
                                      if (menteeSnapshot.hasData && menteeSnapshot.data?.exists == true) {
                                        final menteeData = menteeSnapshot.data!.data() as Map<String, dynamic>?;
                                        if (menteeData != null) {
                                          Navigator.pop(context); // Close bottom sheet
                                          Navigator.push(
                                            context,
                                            MaterialPageRoute(
                                              builder: (context) => MenteeDetailsPage(
                                                menteeDocId: menteeId,
                                                name: menteeData['name'] ?? 'Unknown',
                                                age: menteeData['age'] ?? 0,
                                                phone: menteeData['phone'] ?? '',
                                                menteeNumber: menteeData['menteeNumber'] ?? 0,
                                                createdAt: (menteeData['createdAt'] as Timestamp?)?.toDate(),
                                              ),
                                            ),
                                          );
                                        }
                                      }
                                    },
                                    leading: CircleAvatar(
                                      backgroundColor: isRead ? Colors.grey.shade300 : Colors.orange,
                                      child: Icon(
                                        Icons.notifications,
                                        color: isRead ? Colors.grey.shade600 : Colors.white,
                                      ),
                                    ),
                                    title: Text(
                                      displayTitle,
                                      style: TextStyle(
                                        fontWeight: isRead ? FontWeight.normal : FontWeight.bold,
                                      ),
                                    ),
                                    subtitle: Column(
                                      crossAxisAlignment: CrossAxisAlignment.start,
                                      children: [
                                        if (displayBody.isNotEmpty) ...[
                                          Text(displayBody),
                                          const SizedBox(height: 4),
                                        ],
                                        Text(
                                          _formatTimestamp(timestamp),
                                          style: TextStyle(
                                            fontSize: 12,
                                            color: Colors.grey[600],
                                          ),
                                        ),
                                      ],
                                    ),
                                  );
                                },
                              )
                            : Builder(
                                builder: (context) {
                                  int menteeNumber = directMenteeNumber;
                                  String displayTitle = title;
                                  String displayBody = body;
                                  
                                  if (menteeNumber > 0) {
                                    displayTitle = title.replaceAll('Mentee', 'Mentee #$menteeNumber').replaceAll('mentee', 'Mentee #$menteeNumber');
                                    displayBody = body.replaceAll('Mentee', 'Mentee #$menteeNumber').replaceAll('mentee', 'Mentee #$menteeNumber');
                                  }

                                  return ListTile(
                                    onTap: () async {
                                      // Mark as read
                                      await notifications[index].reference.update({'isRead': true});
                                      
                                      // Fetch and navigate to mentee details
                                      if (menteeId.isNotEmpty) {
                                        try {
                                          final menteeDoc = await FirebaseFirestore.instance
                                              .collection('mentees')
                                              .doc(menteeId)
                                              .get();
                                          
                                          if (menteeDoc.exists) {
                                            final menteeData = menteeDoc.data() as Map<String, dynamic>?;
                                            if (menteeData != null) {
                                              Navigator.pop(context); // Close bottom sheet
                                              Navigator.push(
                                                context,
                                                MaterialPageRoute(
                                                  builder: (context) => MenteeDetailsPage(
                                                    menteeDocId: menteeId,
                                                    name: menteeData['name'] ?? 'Unknown',
                                                    age: menteeData['age'] ?? 0,
                                                    phone: menteeData['phone'] ?? '',
                                                    menteeNumber: menteeData['menteeNumber'] ?? 0,
                                                    createdAt: (menteeData['createdAt'] as Timestamp?)?.toDate(),
                                                  ),
                                                ),
                                              );
                                            }
                                          }
                                        } catch (e) {
                                          print('Error navigating to mentee: $e');
                                        }
                                      }
                                    },
                                    leading: CircleAvatar(
                                      backgroundColor: isRead ? Colors.grey.shade300 : Colors.orange,
                                      child: Icon(
                                        Icons.notifications,
                                        color: isRead ? Colors.grey.shade600 : Colors.white,
                                      ),
                                    ),
                                    title: Text(
                                      displayTitle,
                                      style: TextStyle(
                                        fontWeight: isRead ? FontWeight.normal : FontWeight.bold,
                                      ),
                                    ),
                                    subtitle: Column(
                                      crossAxisAlignment: CrossAxisAlignment.start,
                                      children: [
                                        if (displayBody.isNotEmpty) ...[
                                          Text(displayBody),
                                          const SizedBox(height: 4),
                                        ],
                                        Text(
                                          _formatTimestamp(timestamp),
                                          style: TextStyle(
                                            fontSize: 12,
                                            color: Colors.grey[600],
                                          ),
                                        ),
                                      ],
                                    ),
                                  );
                                },
                              ),
                      );
                    },
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }

  String _formatTimestamp(DateTime timestamp) {
    final now = DateTime.now();
    final difference = now.difference(timestamp);
    
    // Debug: print timestamp info
    print('Timestamp: $timestamp, Now: $now, Difference: ${difference.inMinutes} minutes');

    if (difference.inSeconds < 0) {
      // Timestamp is in the future, might be wrong timezone
      return 'Just now';
    } else if (difference.inMinutes < 1) {
      return 'Just now';
    } else if (difference.inHours < 1) {
      return '${difference.inMinutes}m ago';
    } else if (difference.inDays < 1) {
      return '${difference.inHours}h ago';
    } else if (difference.inDays < 7) {
      return '${difference.inDays}d ago';
    } else {
      // Use DateFormat for better formatting
      return DateFormat('MMM d, y h:mm a').format(timestamp);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.grey[50],
      appBar: AppBar(
        elevation: 0,
        backgroundColor: Colors.transparent,
        flexibleSpace: Container(
          decoration: const BoxDecoration(
            color: Colors.green,
          ),
        ),
        actions: [
          StreamBuilder<QuerySnapshot>(
            stream: FirebaseFirestore.instance
                .collection('notifications')
                .snapshots(),
            builder: (context, snapshot) {
              final allNotifications = snapshot.data?.docs ?? [];
              final unreadCount = allNotifications.where((doc) {
                final data = doc.data() as Map<String, dynamic>;
                return data['mentorId'] == _currentMentorId && 
                       (data['isRead'] as bool? ?? false) == false;
              }).length;

              return Stack(
                children: [
                  IconButton(
                    icon: const Icon(Icons.notifications_outlined),
                    onPressed: _showNotifications,
                  ),
                  if (unreadCount > 0)
                    Positioned(
                      right: 8,
                      top: 8,
                      child: Container(
                        padding: const EdgeInsets.all(4),
                        decoration: const BoxDecoration(
                          color: Colors.red,
                          shape: BoxShape.circle,
                        ),
                        constraints: const BoxConstraints(
                          minWidth: 16,
                          minHeight: 16,
                        ),
                        child: Text(
                          unreadCount > 9 ? '9+' : '$unreadCount',
                          style: const TextStyle(
                            color: Colors.white,
                            fontSize: 10,
                            fontWeight: FontWeight.bold,
                          ),
                          textAlign: TextAlign.center,
                        ),
                      ),
                    ),
                ],
              );
            },
          ),
        ],
      ),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(24, 24, 24, 16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Welcome back',
                  style: TextStyle(
                    fontSize: 18,
                    color: Colors.grey[600],
                    fontWeight: FontWeight.w500,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  _mentorName,
                  style: const TextStyle(
                    fontSize: 28,
                    fontWeight: FontWeight.bold,
                    color: Colors.black87,
                  ),
                ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(24, 24, 24, 16),
            child: Row(
              children: [
                const Text(
                  'Your Mentees',
                  style: TextStyle(
                    fontSize: 20,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const Spacer(),
                StreamBuilder<QuerySnapshot>(
                  stream: FirebaseFirestore.instance
                      .collection('mentees')
                      .where('mentorId', isEqualTo: _currentMentorId)
                      .snapshots(),
                  builder: (context, snapshot) {
                    final count = snapshot.data?.docs.length ?? 0;
                    return Container(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                      decoration: BoxDecoration(
                        color: Colors.green.shade50,
                        borderRadius: BorderRadius.circular(20),
                      ),
                      child: Text(
                        '$count mentees',
                        style: const TextStyle(
                          fontSize: 14,
                          fontWeight: FontWeight.w600,
                          color: Colors.green,
                        ),
                      ),
                    );
                  },
                ),
              ],
            ),
          ),
          // Search Bar
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 8),
            child: TextField(
              controller: _searchController,
              onChanged: (value) {
                setState(() {
                  _searchQuery = value.toLowerCase();
                });
              },
              decoration: InputDecoration(
                hintText: 'Search by name or mentee number...',
                prefixIcon: const Icon(Icons.search, color: Colors.green),
                suffixIcon: _searchQuery.isNotEmpty
                    ? IconButton(
                        icon: const Icon(Icons.clear),
                        onPressed: () {
                          _searchController.clear();
                          setState(() {
                            _searchQuery = '';
                          });
                        },
                      )
                    : null,
                filled: true,
                fillColor: Colors.grey.shade100,
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                  borderSide: BorderSide.none,
                ),
                enabledBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                  borderSide: BorderSide.none,
                ),
                focusedBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                  borderSide: const BorderSide(color: Colors.green, width: 2),
                ),
                contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              ),
            ),
          ),
          Expanded(
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child:
              StreamBuilder<QuerySnapshot<Map<String, dynamic>>>(
                stream: FirebaseFirestore.instance
                    .collection('mentees')
                    .where('mentorId', isEqualTo: _currentMentorId)
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
                  
                  // Filter mentees based on search query
                  final filteredDocs = docs.where((doc) {
                    if (_searchQuery.isEmpty) return true;
                    
                    final data = doc.data();
                    final name = (data['name'] as String? ?? '').toLowerCase();
                    final menteeNumber = (data['menteeNumber'] as int? ?? 0).toString();
                    final phone = (data['phone'] as String? ?? '').toLowerCase();
                    
                    return name.contains(_searchQuery) || 
                           menteeNumber.contains(_searchQuery) ||
                           phone.contains(_searchQuery);
                  }).toList();
                  
                  // Sort by createdAt in the app instead of in the query
                  filteredDocs.sort((a, b) {
                    final aTs = a.data()['createdAt'] as Timestamp?;
                    final bTs = b.data()['createdAt'] as Timestamp?;
                    final aMillis = aTs?.millisecondsSinceEpoch ?? 0;
                    final bMillis = bTs?.millisecondsSinceEpoch ?? 0;
                    return bMillis.compareTo(aMillis);
                  });
                  
                  if (filteredDocs.isEmpty) {
                    return Center(
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 24.0),
                        child: Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(
                              _searchQuery.isEmpty ? Icons.group_off : Icons.search_off,
                              size: 64,
                              color: Colors.grey[400],
                            ),
                            const SizedBox(height: 16),
                            Text(
                              _searchQuery.isEmpty ? 'No mentees yet' : 'No results found',
                              style: const TextStyle(
                                fontSize: 18,
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                            const SizedBox(height: 8),
                            Text(
                              _searchQuery.isEmpty
                                  ? 'You have not added any mentees. Tap "Add Mentee" to create one.'
                                  : 'No mentees match "$_searchQuery". Try a different search.',
                              textAlign: TextAlign.center,
                              style: const TextStyle(color: Colors.black54),
                            ),
                            const SizedBox(height: 16),
                          ],
                        ),
                      ),
                    );
                  }

                  return ListView.builder(
                    padding: EdgeInsets.zero,
                    itemCount: filteredDocs.length,
                    itemBuilder: (context, index) {
                      final doc = filteredDocs[index];
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
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        elevation: 4,
        backgroundColor: Colors.green,
        onPressed: _showAddMenteeDialog,
        icon: const Icon(Icons.person_add),
        label: const Text('Add Mentee'),
      ),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _currentIndex,
        items: const [
          BottomNavigationBarItem(icon: Icon(Icons.home), label: 'Home'),
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
    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(16),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.05),
            blurRadius: 10,
            offset: const Offset(0, 4),
          ),
        ],
      ),
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          borderRadius: BorderRadius.circular(16),
          onTap: () {
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
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Row(
              children: [
                Container(
                  width: 60,
                  height: 60,
                  decoration: BoxDecoration(
                    color: Colors.green,
                    borderRadius: BorderRadius.circular(16),
                  ),
                  child: const Icon(
                    Icons.person,
                    size: 32,
                    color: Colors.white,
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        name,
                        style: const TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      const SizedBox(height: 4),
                      Text(
                        subtitle,
                        style: TextStyle(
                          fontSize: 14,
                          color: Colors.grey[600],
                        ),
                      ),
                      const SizedBox(height: 4),
                      Container(
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                        decoration: BoxDecoration(
                          color: Colors.green.shade50,
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Text(
                          '#$menteeNumber',
                          style: const TextStyle(
                            fontSize: 12,
                            fontWeight: FontWeight.w600,
                            color: Colors.green,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                Icon(
                  Icons.arrow_forward_ios,
                  size: 20,
                  color: Colors.grey[400],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
