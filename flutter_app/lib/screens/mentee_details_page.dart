import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:flutter_sound/flutter_sound.dart';
import 'package:firebase_storage/firebase_storage.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:io';
import 'package:flutter/foundation.dart';
import '../services/notification_service.dart';
import '../services/emergency_service.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:intl/intl.dart';


class MenteeDetailsPage extends StatefulWidget {
  final String menteeDocId;
  final String name;
  final int age;
  final String phone;
  final int menteeNumber;  // Changed from menteeId to menteeNumber
  final DateTime? createdAt;

  const MenteeDetailsPage({
    super.key,
    required this.menteeDocId,
    required this.name,
    required this.age,
    required this.phone,
    required this.menteeNumber,
    this.createdAt,
  });

  @override
  State<MenteeDetailsPage> createState() => _MenteeDetailsPageState();
}

class _MenteeDetailsPageState extends State<MenteeDetailsPage> with SingleTickerProviderStateMixin {
  final FlutterSoundRecorder _audioRecorder = FlutterSoundRecorder();
  final FlutterSoundPlayer _audioPlayer = FlutterSoundPlayer();
  bool _isRecording = false;
  bool _isUploading = false;
  bool _isRecorderInitialized = false;
  bool _isPlayerInitialized = false;
  bool _isPlayingAudio = false;
  String? _currentPlayingAudioPath;
  String? _recordingPath;
  late TabController _tabController;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
    _initRecorder();
    _initPlayer();
  }

  Future<void> _initRecorder() async {
    await _audioRecorder.openRecorder();
    setState(() {
      _isRecorderInitialized = true;
    });
  }

  Future<void> _initPlayer() async {
    await _audioPlayer.openPlayer();
    setState(() {
      _isPlayerInitialized = true;
    });
  }

  @override
  void dispose() {
    _audioRecorder.closeRecorder();
    _audioPlayer.closePlayer();
    _tabController.dispose();
    super.dispose();
  }

  Future<void> _startRecording() async {
    try {
      if (!_isRecorderInitialized) {
        await _initRecorder();
      }

      // Request microphone permission
      final status = await Permission.microphone.request();
      if (!status.isGranted) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Microphone permission denied')),
          );
        }
        return;
      }

      // Get directory to save recording
      final directory = await getApplicationDocumentsDirectory();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      _recordingPath = '${directory.path}/recording_$timestamp.wav';

      // Start recording
      await _audioRecorder.startRecorder(
        toFile: _recordingPath,
        codec: Codec.pcm16WAV,
      );

      setState(() {
        _isRecording = true;
      });
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error starting recording: $e')),
        );
      }
    }
  }

  Future<void> _stopRecording() async {
    try {
      await _audioRecorder.stopRecorder();

      setState(() {
        _isRecording = false;
        _isUploading = true;
      });

      // Upload to Firebase Storage
      await _uploadToFirebase();
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error stopping recording: $e')),
        );
      }
      setState(() {
        _isRecording = false;
        _isUploading = false;
      });
    }
  }
Future<void> _uploadToRealtimeDatabase(String downloadUrl, String fileName) async {
  try {
    final dbRef = FirebaseDatabase.instance
        .ref('esp_commands/${widget.menteeNumber}');

    await dbRef.set({
      'status': 'new',
      'audioUrl': downloadUrl, // or your proxy URL if needed
      'fileName': fileName,
    });

    print('✅ Uploaded to Realtime Database');
  } catch (e) {
    print('❌ RTDB upload error: $e');
  }
}

  Future<void> _uploadToFirebase() async {
    try {
      if (_recordingPath == null || !File(_recordingPath!).existsSync()) {
        throw Exception('Recording file not found');
      }

      final file = File(_recordingPath!);
      final timestamp = DateTime.now();
      final fileName = 'mentee_${widget.menteeNumber}_${timestamp.millisecondsSinceEpoch}.wav';
      
      // Upload to Firebase Storage
      final storageRef = FirebaseStorage.instance
          .ref()
          .child('voice_recordings')
          .child(widget.menteeDocId)
          .child(fileName);

      final uploadTask = await storageRef.putFile(file);
      
      final downloadUrl = await uploadTask.ref.getDownloadURL();

// Convert to proxy URL
//final proxyUrl = 'http://your-backend.com/proxy?url=${Uri.encodeComponent(downloadUrl)}';

      // Save metadata to Firestore
      final docRef = await FirebaseFirestore.instance
          .collection('voice_recordings')
          .add({
        'menteeDocId': widget.menteeDocId,
        'menteeNumber': widget.menteeNumber,
        'menteeName': widget.name,
        'fileName': fileName,
        'downloadUrl': downloadUrl,  // the prox URL
        'timestamp': timestamp,
        'duration': null,
        'processed': false,
      });
      await _uploadToRealtimeDatabase(downloadUrl, fileName);


      // Send notification to ESP32 via backend
      await NotificationService().sendNotificationToMentee(
        menteeId: widget.menteeDocId,
        title: 'New Voice Recording',
        body: 'Your mentor has sent you a new voice message',
        data: {
          'type': 'voice_recording',
          'recordingId': docRef.id,
          'downloadUrl': downloadUrl,  // Backend will convert this
          'fileName': fileName,
          'menteeNumber': widget.menteeNumber,
        },
      );

      // Delete local file after upload
      await file.delete();

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Recording saved successfully!'),
            backgroundColor: Colors.green,
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Error uploading recording: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } finally {
      setState(() {
        _isUploading = false;
        _recordingPath = null;
      });
    }
  }

  String _formatDate(DateTime date) {
    final months = [
      'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
      'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
    ];
    return '${months[date.month - 1]} ${date.day}, ${date.year}';
  }

  Future<Map<String, dynamic>> _getEmergencyStats() async {
    try {
      // Get current date
      final now = DateTime.now();
      
      // Get ALL notifications for this mentee once
      final allSnapshot = await FirebaseFirestore.instance
          .collection('notifications_from_esp')
          .where('menteeNumber', isEqualTo: widget.menteeNumber)
          .get();
      
      print('📊 Total notifications for mentee ${widget.menteeNumber}: ${allSnapshot.docs.length}');
      
      // Get last 7 days data
      final List<int> dailyCounts = [];
      final List<String> dayLabels = [];
      
      for (int i = 6; i >= 0; i--) {
        final date = now.subtract(Duration(days: i));
        final startOfDay = DateTime(date.year, date.month, date.day);
        final endOfDay = DateTime(date.year, date.month, date.day, 23, 59, 59, 999);
        
        // Filter by date and document ID starting with "help_"
        int dayCount = 0;
        for (var doc in allSnapshot.docs) {
          final data = doc.data();
          final docId = doc.id; // Check document ID, not title field
          
          // Parse timestamp from ESP format "YYYYMMDD_HHMMSS"
          final timestampField = data['timestamp'];
          DateTime? timestamp;
          
          if (timestampField is String && timestampField.isNotEmpty) {
            try {
              if (timestampField.length == 15 && timestampField.contains('_')) {
                final parts = timestampField.split('_');
                final datePart = parts[0];
                final timePart = parts[1];
                
                final year = int.parse(datePart.substring(0, 4));
                final month = int.parse(datePart.substring(4, 6));
                final day = int.parse(datePart.substring(6, 8));
                final hour = int.parse(timePart.substring(0, 2));
                final minute = int.parse(timePart.substring(2, 4));
                final second = int.parse(timePart.substring(4, 6));
                
                // Parse as UTC and convert to local time
                timestamp = DateTime.utc(year, month, day, hour, minute, second).toLocal();
              }
            } catch (e) {
              print('Error parsing timestamp: $e');
            }
          } else if (timestampField is Timestamp) {
            timestamp = timestampField.toDate();
          }
          
          // Check if document ID starts with "help_" and is within the day range (inclusive)
          if (docId.toLowerCase().startsWith('help_') && timestamp != null) {
            if (!timestamp.isBefore(startOfDay) && !timestamp.isAfter(endOfDay)) {
              dayCount++;
              print('✓ Emergency on ${date.toLocal()}: $docId');
            }
          }
        }
        
        dailyCounts.add(dayCount);
        
        // Format day label (e.g., "Mon", "Tue")
        final weekdays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
        dayLabels.add(weekdays[date.weekday % 7]);
      }
      
      // Get total emergencies (document ID starts with "help_")
      int totalEmergencies = 0;
      for (var doc in allSnapshot.docs) {
        final data = doc.data();
        final docId = doc.id; // Check document ID, not title field
        if (docId.toLowerCase().startsWith('help_')) {
          totalEmergencies++;
          print('📍 Emergency: $docId at ${data['timestamp']}');
        }
      }
      
      print('📈 Weekly counts: $dailyCounts');
      print('📊 Total emergencies: $totalEmergencies');
      
      final weeklyTotal = dailyCounts.reduce((a, b) => a + b);
      final weeklyAverage = weeklyTotal / 7;
      
      return {
        'dailyCounts': dailyCounts,
        'dayLabels': dayLabels,
        'totalEmergencies': totalEmergencies,
        'weeklyAverage': weeklyAverage,
        'weeklyTotal': weeklyTotal,
      };
    } catch (e) {
      print('❌ Error getting emergency stats: $e');
      return {
        'dailyCounts': [0, 0, 0, 0, 0, 0, 0],
        'dayLabels': ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'],
        'totalEmergencies': 0,
        'weeklyAverage': 0.0,
        'weeklyTotal': 0,
      };
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Colors.white,
        elevation: 0,
        leading: IconButton(
          icon: const Icon(Icons.arrow_back, color: Colors.black),
          onPressed: () => Navigator.pop(context),
        ),
        title: const Text(
          'Mentee Details',
          style: TextStyle(color: Colors.black, fontSize: 20),
        ),
        bottom: TabBar(
          controller: _tabController,
          labelColor: Colors.green,
          unselectedLabelColor: Colors.grey,
          indicatorColor: Colors.green,
          tabs: const [
            Tab(text: 'Overview'),
            Tab(text: 'Emergency History'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          _buildOverviewTab(),
          _buildEmergencyHistoryTab(),
        ],
      ),
      floatingActionButton: _tabController.index == 0
          ? FloatingActionButton.extended(
              onPressed: _isUploading
                  ? null
                  : () {
                      if (_isRecording) {
                        _stopRecording();
                      } else {
                        _startRecording();
                      }
                    },
              backgroundColor: _isRecording
                  ? Colors.red
                  : (_isUploading ? Colors.grey : Colors.green),
              icon: _isUploading
                  ? const SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(
                        color: Colors.white,
                        strokeWidth: 2,
                      ),
                    )
                  : Icon(_isRecording ? Icons.stop : Icons.mic),
              label: Text(
                _isUploading
                    ? 'Uploading...'
                    : (_isRecording ? 'Stop Recording' : 'Record Voice'),
              ),
            )
          : null,
    );
  }

  Widget _buildOverviewTab() {
    return FutureBuilder<Map<String, dynamic>>(
        future: _getEmergencyStats(),
        builder: (context, snapshot) {
          return SingleChildScrollView(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Profile Card
                Card(
                  elevation: 3,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(16),
                  ),
                  child: Padding(
                    padding: const EdgeInsets.all(24.0),
                    child: Column(
                      children: [
                        // Avatar
                        CircleAvatar(
                          radius: 50,
                          backgroundColor: Colors.green,
                          child: Text(
                            widget.name.isNotEmpty ? widget.name[0].toUpperCase() : 'M',
                            style: const TextStyle(
                              fontSize: 40,
                              fontWeight: FontWeight.bold,
                              color: Colors.white,
                            ),
                          ),
                        ),
                        const SizedBox(height: 16),
                        Text(
                          widget.name,
                          style: const TextStyle(
                            fontSize: 24,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 8),
                        if (widget.phone.isNotEmpty)
                          Row(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              const Icon(
                                Icons.phone,
                                size: 16,
                                color: Colors.grey,
                              ),
                              const SizedBox(width: 4),
                              Text(
                                widget.phone,
                                style: TextStyle(
                                  fontSize: 14,
                                  color: Colors.grey.shade600,
                                ),
                              ),
                            ],
                          ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 24),

                // Information Section
                const Text(
                  'Information',
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 12),

                // Info Cards
                _buildInfoCard(
                  icon: Icons.badge,
                  label: 'Mentee Number',
                  value: '${widget.menteeNumber}',
                  color: Colors.green,
                ),
                const SizedBox(height: 12),
                _buildInfoCard(
                  icon: Icons.cake,
                  label: 'Age',
                  value: '${widget.age} years',
                  color: Colors.orange,
                ),
                const SizedBox(height: 12),
                if (widget.createdAt != null)
                  _buildInfoCard(
                    icon: Icons.calendar_today,
                    label: 'Added Date',
                    value: _formatDate(widget.createdAt!),
                    color: Colors.blue,
                  ),
                const SizedBox(height: 24),

                // Emergency Statistics Section
                const Text(
                  'Emergency Statistics',
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 12),

                if (snapshot.connectionState == ConnectionState.waiting)
                  const Center(
                    child: Padding(
                      padding: EdgeInsets.all(24.0),
                      child: CircularProgressIndicator(),
                    ),
                  )
                else if (snapshot.hasError)
                  Card(
                    elevation: 2,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Text(
                        'Error loading statistics',
                        style: TextStyle(color: Colors.red.shade700),
                      ),
                    ),
                  )
                else ...[
                  // Stats Cards Row
                  Row(
                    children: [
                      Expanded(
                        child: _buildStatCard(
                          icon: Icons.warning_amber,
                          label: 'This Week',
                          value: '${snapshot.data!['weeklyTotal']}',
                          color: Colors.red,
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: _buildStatCard(
                          icon: Icons.emergency,
                          label: 'Total',
                          value: '${snapshot.data!['totalEmergencies']}',
                          color: Colors.deepOrange,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),

                  // Weekly Average Card
                  Card(
                    elevation: 2,
                    color: Colors.red.shade50,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Row(
                        children: [
                          Icon(
                            Icons.trending_up,
                            color: Colors.red.shade700,
                            size: 32,
                          ),
                          const SizedBox(width: 16),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  'Daily Average',
                                  style: TextStyle(
                                    fontSize: 14,
                                    color: Colors.red.shade700,
                                    fontWeight: FontWeight.w500,
                                  ),
                                ),
                                const SizedBox(height: 4),
                                Text(
                                  '${snapshot.data!['weeklyAverage'].toStringAsFixed(1)} emergencies/day',
                                  style: TextStyle(
                                    fontSize: 20,
                                    color: Colors.red.shade900,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 16),

                  // Chart Card
                  Card(
                    elevation: 2,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            'Last 7 Days',
                            style: TextStyle(
                              fontSize: 16,
                              fontWeight: FontWeight.bold,
                              color: Colors.grey.shade800,
                            ),
                          ),
                          const SizedBox(height: 16),
                          SizedBox(
                            height: 200,
                            child: BarChart(
                              BarChartData(
                                alignment: BarChartAlignment.spaceAround,
                                maxY: (snapshot.data!['dailyCounts'] as List<int>)
                                        .reduce((a, b) => a > b ? a : b)
                                        .toDouble() +
                                    2,
                                barTouchData: BarTouchData(enabled: false),
                                titlesData: FlTitlesData(
                                  show: true,
                                  bottomTitles: AxisTitles(
                                    sideTitles: SideTitles(
                                      showTitles: true,
                                      getTitlesWidget: (value, meta) {
                                        final labels = snapshot.data!['dayLabels'] as List<String>;
                                        if (value.toInt() >= 0 && value.toInt() < labels.length) {
                                          return Padding(
                                            padding: const EdgeInsets.only(top: 8.0),
                                            child: Text(
                                              labels[value.toInt()],
                                              style: const TextStyle(
                                                fontSize: 12,
                                                fontWeight: FontWeight.bold,
                                              ),
                                            ),
                                          );
                                        }
                                        return const Text('');
                                      },
                                    ),
                                  ),
                                  leftTitles: AxisTitles(
                                    sideTitles: SideTitles(
                                      showTitles: true,
                                      reservedSize: 30,
                                      getTitlesWidget: (value, meta) {
                                        return Text(
                                          value.toInt().toString(),
                                          style: const TextStyle(fontSize: 12),
                                        );
                                      },
                                    ),
                                  ),
                                  topTitles: const AxisTitles(
                                    sideTitles: SideTitles(showTitles: false),
                                  ),
                                  rightTitles: const AxisTitles(
                                    sideTitles: SideTitles(showTitles: false),
                                  ),
                                ),
                                borderData: FlBorderData(show: false),
                                gridData: FlGridData(
                                  show: true,
                                  drawVerticalLine: false,
                                  horizontalInterval: 1,
                                ),
                                barGroups: List.generate(
                                  (snapshot.data!['dailyCounts'] as List<int>).length,
                                  (index) {
                                    final count = (snapshot.data!['dailyCounts'] as List<int>)[index];
                                    return BarChartGroupData(
                                      x: index,
                                      barRods: [
                                        BarChartRodData(
                                          toY: count.toDouble(),
                                          color: Colors.red,
                                          width: 20,
                                          borderRadius: const BorderRadius.only(
                                            topLeft: Radius.circular(6),
                                            topRight: Radius.circular(6),
                                          ),
                                        ),
                                      ],
                                    );
                                  },
                                ),
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              ],
            ),
          );
        },
      );
  }

  Widget _buildEmergencyHistoryTab() {
    print('🔍 Building emergency history for menteeNumber: ${widget.menteeNumber}');
    
    return StreamBuilder<QuerySnapshot>(
      stream: FirebaseFirestore.instance
          .collection('detections')
          .where('menteeNumber', isEqualTo: widget.menteeNumber)
          .snapshots(),
      builder: (context, snapshot) {
        if (snapshot.connectionState == ConnectionState.waiting) {
          return const Center(child: CircularProgressIndicator());
        }

        if (snapshot.hasError) {
          print('❌ Error loading detections: ${snapshot.error}');
          return Center(
            child: Text('Error loading emergency history: ${snapshot.error}'),
          );
        }

        var docs = snapshot.data?.docs ?? [];
        print('📊 Found ${docs.length} detection documents');
        
        // Debug: Print first doc if exists
        if (docs.isNotEmpty) {
          print('First doc data: ${docs.first.data()}');
        }
        
        // Sort by timestamp in descending order (newest first)
        docs.sort((a, b) {
          final aData = a.data() as Map<String, dynamic>;
          final bData = b.data() as Map<String, dynamic>;
          
          // Get timestamp string
          final aTimestamp = aData['timestamp'] as String?;
          final bTimestamp = bData['timestamp'] as String?;
          
          if (aTimestamp == null) return 1;
          if (bTimestamp == null) return -1;
          return bTimestamp.compareTo(aTimestamp);
        });

        if (docs.isEmpty) {
          // Debug: Query all detections to see if any exist
          _debugQueryAllDetections();
          
          return Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(
                  Icons.check_circle_outline,
                  size: 80,
                  color: Colors.grey.shade400,
                ),
                const SizedBox(height: 16),
                Text(
                  'No Detections',
                  style: TextStyle(
                    fontSize: 18,
                    color: Colors.grey.shade600,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  'No detection events recorded yet.',
                  style: TextStyle(
                    fontSize: 14,
                    color: Colors.grey.shade500,
                  ),
                ),
                const SizedBox(height: 24),
                ElevatedButton.icon(
                  onPressed: _cleanupOldRecords,
                  icon: const Icon(Icons.cleaning_services),
                  label: const Text('Cleanup Old Records'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.orange,
                    foregroundColor: Colors.white,
                  ),
                ),
              ],
            ),
          );
        }

        return ListView.builder(
          padding: const EdgeInsets.all(16),
          itemCount: docs.length,
          itemBuilder: (context, index) {
            final doc = docs[index];
            final data = doc.data() as Map<String, dynamic>;
            return _buildDetectionCard(doc.id, data);
          },
        );
      },
    );
  }

  Widget _buildDetectionCard(String docId, Map<String, dynamic> data) {
    // Parse timestamp string to DateTime
    final timestampStr = data['timestamp'] as String? ?? '';
    DateTime timestamp;
    try {
      // Format: "20260121_151806" -> parse to DateTime
      final year = int.parse(timestampStr.substring(0, 4));
      final month = int.parse(timestampStr.substring(4, 6));
      final day = int.parse(timestampStr.substring(6, 8));
      final hour = int.parse(timestampStr.substring(9, 11));
      final minute = int.parse(timestampStr.substring(11, 13));
      final second = int.parse(timestampStr.substring(13, 15));
      // Parse as UTC and convert to local time
      timestamp = DateTime.utc(year, month, day, hour, minute, second).toLocal();
    } catch (e) {
      timestamp = DateTime.now();
    }
    
    final dateFormat = DateFormat('MMM dd, yyyy • hh:mm a');
    final audioPath = data['audio'] as String?;
    final imagePath = data['image'] as String?;
    final message = data['message'] as String? ?? 'Detection event';
    
    return Card(
      margin: const EdgeInsets.only(bottom: 16),
      elevation: 3,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Header with detection ID
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  children: [
                    Icon(Icons.sensors, color: Colors.blue, size: 24),
                    const SizedBox(width: 8),
                    Text(
                      'DETECTION',
                      style: TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                        color: Colors.blue,
                      ),
                    ),
                  ],
                ),
              ],
            ),
            const SizedBox(height: 12),
            
            // Detection ID/Number
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
              decoration: BoxDecoration(
                color: Colors.blue.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.blue.shade200),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.fingerprint, size: 16, color: Colors.blue.shade700),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      'ID: $docId',
                      style: TextStyle(
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                        color: Colors.blue.shade700,
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 12),
            
            // Timestamp
            Row(
              children: [
                Icon(Icons.access_time, size: 16, color: Colors.grey.shade600),
                const SizedBox(width: 4),
                Text(
                  dateFormat.format(timestamp),
                  style: TextStyle(
                    fontSize: 13,
                    color: Colors.grey.shade600,
                  ),
                ),
              ],
            ),
            
            if (audioPath != null && audioPath.isNotEmpty) ...[
              const SizedBox(height: 16),
              const Divider(),
              const SizedBox(height: 8),
              // Audio player
              GestureDetector(
                onTap: () => _playAudioFromPath(audioPath),
                child: Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: _isPlayingAudio && _currentPlayingAudioPath == audioPath
                        ? Colors.green.shade100
                        : Colors.green.shade50,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                      color: _isPlayingAudio && _currentPlayingAudioPath == audioPath
                          ? Colors.green.shade400
                          : Colors.green.shade200,
                    ),
                  ),
                  child: Row(
                    children: [
                      Icon(
                        _isPlayingAudio && _currentPlayingAudioPath == audioPath
                            ? Icons.stop_circle
                            : Icons.play_circle_filled,
                        color: Colors.green.shade700,
                        size: 32,
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              _isPlayingAudio && _currentPlayingAudioPath == audioPath
                                  ? 'Playing Audio...'
                                  : 'Audio Recording',
                              style: const TextStyle(
                                fontSize: 14,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            const SizedBox(height: 4),
                            Text(
                              _isPlayingAudio && _currentPlayingAudioPath == audioPath
                                  ? 'Tap to stop'
                                  : 'Tap to play',
                              style: TextStyle(
                                fontSize: 11,
                                color: Colors.grey.shade600,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ],
            
            if (imagePath != null && imagePath.isNotEmpty) ...[
              const SizedBox(height: 16),
              const Divider(),
              const SizedBox(height: 8),
              const Text(
                'Detection Image',
                style: TextStyle(
                  fontSize: 14,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 8),
              _buildDetectionImageFromPath(imagePath),
            ],
            
            if (message.isNotEmpty) ...[
              const SizedBox(height: 16),
              const Divider(),
              const SizedBox(height: 8),
              const Text(
                'Message:',
                style: TextStyle(
                  fontSize: 14,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 4),
              Text(
                message,
                style: TextStyle(
                  fontSize: 13,
                  color: Colors.grey.shade700,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildSectionHeader(String title) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 16.0),
      child: Text(
        title,
        style: const TextStyle(
          fontSize: 18,
          fontWeight: FontWeight.bold,
        ),
      ),
    );
  }

  Widget _buildInfoCard({
    required IconData icon,
    required String label,
    required String value,
    required Color color,
  }) {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: color.withOpacity(0.1),
                borderRadius: BorderRadius.circular(12),
              ),
              child: Icon(icon, color: color, size: 28),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    label,
                    style: TextStyle(
                      fontSize: 14,
                      color: Colors.grey.shade600,
                    ),
                  ),
                  const SizedBox(height: 4),
                  Text(
                    value,
                    style: const TextStyle(
                      fontSize: 18,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatCard({
    required IconData icon,
    required String label,
    required String value,
    required Color color,
  }) {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Icon(icon, color: color, size: 32),
            const SizedBox(height: 8),
            Text(
              value,
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.bold,
                color: color,
              ),
            ),
            const SizedBox(height: 4),
            Text(
              label,
              style: TextStyle(
                fontSize: 12,
                color: Colors.grey.shade600,
              ),
            ),
          ],
        ),
      ),
    );
  }

  // Helper method to build detection image from direct path
  Widget _buildDetectionImage(String imagePath, String? detectionId) {
    return GestureDetector(
      onTap: () {
        _showFullScreenImage(imagePath);
      },
      child: Container(
        height: 200,
        decoration: BoxDecoration(
          color: Colors.grey.shade200,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: Colors.grey.shade300),
        ),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(8),
          child: Image.network(
            imagePath,
            fit: BoxFit.cover,
            width: double.infinity,
            loadingBuilder: (context, child, loadingProgress) {
              if (loadingProgress == null) return child;
              return Center(
                child: CircularProgressIndicator(
                  value: loadingProgress.expectedTotalBytes != null
                      ? loadingProgress.cumulativeBytesLoaded /
                          loadingProgress.expectedTotalBytes!
                      : null,
                ),
              );
            },
            errorBuilder: (context, error, stackTrace) {
              return Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.broken_image, size: 48, color: Colors.grey.shade400),
                    const SizedBox(height: 8),
                    const Text(
                      'Failed to load image',
                      style: TextStyle(color: Colors.grey),
                    ),
                  ],
                ),
              );
            },
          ),
        ),
      ),
    );
  }

  // Helper method to fetch and display image from detections collection
  Widget _buildDetectionImageFromFirestore(String detectionId) {
    return FutureBuilder<String?>(
      future: _getDetectionImageUrl(detectionId),
      builder: (context, snapshot) {
        if (snapshot.connectionState == ConnectionState.waiting) {
          return Container(
            height: 200,
            decoration: BoxDecoration(
              color: Colors.grey.shade200,
              borderRadius: BorderRadius.circular(8),
            ),
            child: const Center(
              child: CircularProgressIndicator(),
            ),
          );
        }

        if (snapshot.hasError || !snapshot.hasData || snapshot.data == null) {
          return Container(
            height: 200,
            decoration: BoxDecoration(
              color: Colors.grey.shade100,
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: Colors.grey.shade300),
            ),
            child: Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.image_not_supported, size: 48, color: Colors.grey.shade400),
                  const SizedBox(height: 8),
                  Text(
                    'No image available',
                    style: TextStyle(color: Colors.grey.shade600),
                  ),
                ],
              ),
            ),
          );
        }

        return _buildDetectionImage(snapshot.data!, detectionId);
      },
    );
  }

  // Fetch detection image URL from Firestore detections collection
  Future<String?> _getDetectionImageUrl(String detectionId) async {
    try {
      final doc = await FirebaseFirestore.instance
          .collection('detections')
          .doc(detectionId)
          .get();

      if (!doc.exists) {
        print('Detection document not found: $detectionId');
        return null;
      }

      final data = doc.data();
      if (data == null) return null;

      // Check if image field exists
      final imageField = data['image'];
      if (imageField == null) {
        print('No image field in detection: $detectionId');
        return null;
      }

      String imagePath;
      if (imageField is Map && imageField.containsKey('stringValue')) {
        imagePath = imageField['stringValue'] as String;
      } else if (imageField is String) {
        imagePath = imageField;
      } else {
        print('Unknown image field format: $imageField');
        return null;
      }

      // Convert storage path to download URL
      if (imagePath.startsWith('/')) {
        imagePath = imagePath.substring(1); // Remove leading slash
      }

      final ref = FirebaseStorage.instance.ref(imagePath);
      final url = await ref.getDownloadURL();
      return url;
    } catch (e) {
      print('Error fetching detection image: $e');
      return null;
    }
  }

  // Show full screen image dialog
  void _showFullScreenImage(String imageUrl) {
    showDialog(
      context: context,
      builder: (context) => Dialog(
        backgroundColor: Colors.black,
        child: Stack(
          children: [
            Center(
              child: Image.network(
                imageUrl,
                fit: BoxFit.contain,
                errorBuilder: (context, error, stackTrace) {
                  return const Center(
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(Icons.error, color: Colors.white, size: 48),
                        SizedBox(height: 8),
                        Text(
                          'Failed to load image',
                          style: TextStyle(color: Colors.white),
                        ),
                      ],
                    ),
                  );
                },
              ),
            ),
            Positioned(
              top: 10,
              right: 10,
              child: IconButton(
                icon: const Icon(Icons.close, color: Colors.white),
                onPressed: () => Navigator.pop(context),
              ),
            ),
          ],
        ),
      ),
    );
  }

  // Build detection image from storage path
  Widget _buildDetectionImageFromPath(String storagePath) {
    return FutureBuilder<String>(
      future: _getImageDownloadUrl(storagePath),
      builder: (context, snapshot) {
        if (snapshot.connectionState == ConnectionState.waiting) {
          return Container(
            height: 200,
            decoration: BoxDecoration(
              color: Colors.grey.shade200,
              borderRadius: BorderRadius.circular(8),
            ),
            child: const Center(
              child: CircularProgressIndicator(),
            ),
          );
        }

        if (snapshot.hasError || !snapshot.hasData) {
          return Container(
            height: 200,
            decoration: BoxDecoration(
              color: Colors.grey.shade100,
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: Colors.grey.shade300),
            ),
            child: Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.image_not_supported, size: 48, color: Colors.grey.shade400),
                  const SizedBox(height: 8),
                  Text(
                    'Failed to load image',
                    style: TextStyle(color: Colors.grey.shade600),
                  ),
                ],
              ),
            ),
          );
        }

        return _buildDetectionImage(snapshot.data!, null);
      },
    );
  }

  // Get download URL from Firebase Storage path
  Future<String> _getImageDownloadUrl(String storagePath) async {
    try {
      // Remove leading slash if present
      if (storagePath.startsWith('/')) {
        storagePath = storagePath.substring(1);
      }
      
      final ref = FirebaseStorage.instance.ref(storagePath);
      return await ref.getDownloadURL();
    } catch (e) {
      print('Error getting image URL: $e');
      throw e;
    }
  }

  // Debug method to check all detections in Firestore
  void _debugQueryAllDetections() async {
    try {
      print('🔍 DEBUG: Querying ALL detections...');
      final allDocs = await FirebaseFirestore.instance
          .collection('detections')
          .get();
      
      print('📊 Total detections in database: ${allDocs.docs.length}');
      
      // Clean up old records with menteeId field
      int deletedCount = 0;
      for (var doc in allDocs.docs) {
        final data = doc.data();
        
        // Delete if it has menteeId field (old format)
        if (data.containsKey('menteeId') && !data.containsKey('menteeNumber')) {
          print('  🗑️ Deleting old record ${doc.id} with menteeId field');
          await doc.reference.delete();
          deletedCount++;
        } else {
          print('  - Doc ${doc.id}: menteeNumber=${data['menteeNumber']}, timestamp=${data['timestamp']}');
        }
      }
      
      if (deletedCount > 0) {
        print('✅ Deleted $deletedCount old detection records');
      }
      
      print('🔍 Looking for menteeNumber: ${widget.menteeNumber} (type: ${widget.menteeNumber.runtimeType})');
    } catch (e) {
      print('❌ Debug query error: $e');
    }
  }

  // Debug method to check all emergencies in Firestore
  void _debugQueryAllEmergencies() async {
    try {
      print('🔍 DEBUG: Querying ALL emergencies...');
      final allDocs = await FirebaseFirestore.instance
          .collection('emergencies')
          .get();
      
      print('📊 Total emergencies in database: ${allDocs.docs.length}');
      
      // Clean up old records with menteeId field
      int deletedCount = 0;
      for (var doc in allDocs.docs) {
        final data = doc.data();
        
        // Delete if it has menteeId field (old format)
        if (data.containsKey('menteeId')) {
          print('  🗑️ Deleting old record ${doc.id} with menteeId field');
          await doc.reference.delete();
          deletedCount++;
        } else {
          print('  - Doc ${doc.id}: menteeNumber=${data['menteeNumber']}, emotion=${data['emotion']}, timestamp=${data['timestamp']}');
        }
      }
      
      if (deletedCount > 0) {
        print('✅ Deleted $deletedCount old emergency records');
      }
      
      print('🔍 Looking for menteeNumber: ${widget.menteeNumber} (type: ${widget.menteeNumber.runtimeType})');
    } catch (e) {
      print('❌ Debug query error: $e');
    }
  }

  // Play audio from Firebase Storage path
  Future<void> _playAudioFromPath(String audioPath) async {
    try {
      if (!_isPlayerInitialized) {
        await _initPlayer();
      }

      // If already playing this audio, stop it
      if (_isPlayingAudio && _currentPlayingAudioPath == audioPath) {
        await _audioPlayer.stopPlayer();
        setState(() {
          _isPlayingAudio = false;
          _currentPlayingAudioPath = null;
        });
        return;
      }

      // Stop any currently playing audio
      if (_isPlayingAudio) {
        await _audioPlayer.stopPlayer();
      }

      setState(() {
        _isPlayingAudio = true;
        _currentPlayingAudioPath = audioPath;
      });

      // Download audio file to temporary location
      final tempDir = await getApplicationDocumentsDirectory();
      final fileName = audioPath.split('/').last.replaceAll('.raw', '.wav');
      final localPath = '${tempDir.path}/$fileName';
      
      // Get download URL and download file
      String downloadUrl = await _getAudioDownloadUrl(audioPath);
      
      // Download the file
      final response = await HttpClient().getUrl(Uri.parse(downloadUrl));
      final downloadResponse = await response.close();
      final file = File(localPath);
      final bytes = await consolidateHttpClientResponseBytes(downloadResponse);
      await file.writeAsBytes(bytes);

      // Play the audio from local file
      await _audioPlayer.startPlayer(
        fromURI: localPath,
        codec: Codec.pcm16WAV,
        whenFinished: () {
          setState(() {
            _isPlayingAudio = false;
            _currentPlayingAudioPath = null;
          });
          // Clean up temp file
          file.deleteSync();
        },
      );
    } catch (e) {
      print('❌ Error playing audio: $e');
      setState(() {
        _isPlayingAudio = false;
        _currentPlayingAudioPath = null;
      });
      
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Error playing audio: ${e.toString()}'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  // Get audio download URL from Firebase Storage path
  Future<String> _getAudioDownloadUrl(String storagePath) async {
    try {
      // Remove leading slash if present
      if (storagePath.startsWith('/')) {
        storagePath = storagePath.substring(1);
      }
      
      final ref = FirebaseStorage.instance.ref(storagePath);
      return await ref.getDownloadURL();
    } catch (e) {
      print('Error getting audio URL: $e');
      throw e;
    }
  }

  // Cleanup old records with menteeId field
  Future<void> _cleanupOldRecords() async {
    try {
      print('🧹 Starting cleanup of old records...');
      
      // Show loading dialog
      if (!mounted) return;
      showDialog(
        context: context,
        barrierDismissible: false,
        builder: (context) => const Center(
          child: CircularProgressIndicator(),
        ),
      );
      
      int deletedEmergencies = 0;
      int deletedDetections = 0;
      
      // Clean emergencies with menteeId
      final emergencies = await FirebaseFirestore.instance
          .collection('emergencies')
          .get();
      
      for (var doc in emergencies.docs) {
        final data = doc.data();
        if (data.containsKey('menteeId') && !data.containsKey('menteeNumber')) {
          await doc.reference.delete();
          deletedEmergencies++;
        }
      }
      
      // Clean detections with menteeId  
      final detections = await FirebaseFirestore.instance
          .collection('detections')
          .get();
      
      for (var doc in detections.docs) {
        final data = doc.data();
        if (data.containsKey('menteeId') && !data.containsKey('menteeNumber')) {
          await doc.reference.delete();
          deletedDetections++;
        }
      }
      
      print('✅ Cleanup complete: $deletedEmergencies emergencies, $deletedDetections detections deleted');
      
      // Close loading dialog
      if (!mounted) return;
      Navigator.pop(context);
      
      // Show result
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Cleanup complete! Deleted $deletedEmergencies emergencies and $deletedDetections detections'),
          backgroundColor: Colors.green,
          duration: const Duration(seconds: 3),
        ),
      );
    } catch (e) {
      print('❌ Cleanup error: $e');
      if (mounted) {
        Navigator.pop(context);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Error during cleanup: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }
}
