import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:flutter_sound/flutter_sound.dart';
import 'package:firebase_storage/firebase_storage.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:io';
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
  bool _isRecording = false;
  bool _isUploading = false;
  bool _isRecorderInitialized = false;
  String? _recordingPath;
  late TabController _tabController;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
    _initRecorder();
  }

  Future<void> _initRecorder() async {
    await _audioRecorder.openRecorder();
    setState(() {
      _isRecorderInitialized = true;
    });
  }

  @override
  void dispose() {
    _audioRecorder.closeRecorder();
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
      
      // Get last 7 days data
      final List<int> dailyCounts = [];
      final List<String> dayLabels = [];
      
      for (int i = 6; i >= 0; i--) {
        final date = now.subtract(Duration(days: i));
        final startOfDay = DateTime(date.year, date.month, date.day);
        final endOfDay = DateTime(date.year, date.month, date.day, 23, 59, 59);
        
        // Query by menteeNumber to include all emergencies (old and new)
        final snapshot = await FirebaseFirestore.instance
            .collection('emergencies')
            .where('menteeNumber', isEqualTo: widget.menteeNumber)
            .where('timestamp', isGreaterThanOrEqualTo: startOfDay)
            .where('timestamp', isLessThanOrEqualTo: endOfDay)
            .get();
        
        dailyCounts.add(snapshot.docs.length);
        
        // Format day label (e.g., "Mon", "Tue")
        final weekdays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
        dayLabels.add(weekdays[date.weekday % 7]);
      }
      
      // Get total emergencies by menteeNumber
      final totalSnapshot = await FirebaseFirestore.instance
          .collection('emergencies')
          .where('menteeNumber', isEqualTo: widget.menteeNumber)
          .get();
      
      final totalEmergencies = totalSnapshot.docs.length;
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
    return StreamBuilder<List<EmergencyEvent>>(
      stream: EmergencyService().getEmergencyEventsForMentee(widget.menteeDocId),
      builder: (context, snapshot) {
        if (snapshot.connectionState == ConnectionState.waiting) {
          return const Center(child: CircularProgressIndicator());
        }

        if (snapshot.hasError) {
          return Center(
            child: Text('Error loading emergency history: ${snapshot.error}'),
          );
        }

        final events = snapshot.data ?? [];

        if (events.isEmpty) {
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
                  'No Emergency Events',
                  style: TextStyle(
                    fontSize: 18,
                    color: Colors.grey.shade600,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  'All clear! No emergency events recorded.',
                  style: TextStyle(
                    fontSize: 14,
                    color: Colors.grey.shade500,
                  ),
                ),
              ],
            ),
          );
        }

        return ListView.builder(
          padding: const EdgeInsets.all(16),
          itemCount: events.length,
          itemBuilder: (context, index) {
            final event = events[index];
            return _buildEmergencyEventCard(event);
          },
        );
      },
    );
  }

  Widget _buildEmergencyEventCard(EmergencyEvent event) {
    final dateFormat = DateFormat('MMM dd, yyyy • hh:mm a');
    
    Color statusColor;
    IconData statusIcon;
    
    switch (event.status) {
      case 'new':
        statusColor = Colors.red;
        statusIcon = Icons.warning;
        break;
      case 'viewed':
        statusColor = Colors.orange;
        statusIcon = Icons.visibility;
        break;
      case 'resolved':
        statusColor = Colors.green;
        statusIcon = Icons.check_circle;
        break;
      default:
        statusColor = Colors.grey;
        statusIcon = Icons.info;
    }

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
            // Header with status
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  children: [
                    Icon(statusIcon, color: statusColor, size: 24),
                    const SizedBox(width: 8),
                    Text(
                      event.emotionDetected.toUpperCase(),
                      style: TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                        color: statusColor,
                      ),
                    ),
                  ],
                ),
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                  decoration: BoxDecoration(
                    color: statusColor.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(color: statusColor),
                  ),
                  child: Text(
                    event.status.toUpperCase(),
                    style: TextStyle(
                      fontSize: 11,
                      fontWeight: FontWeight.bold,
                      color: statusColor,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            
            // Timestamp
            Row(
              children: [
                Icon(Icons.access_time, size: 16, color: Colors.grey.shade600),
                const SizedBox(width: 4),
                Text(
                  dateFormat.format(event.timestamp),
                  style: TextStyle(
                    fontSize: 13,
                    color: Colors.grey.shade600,
                  ),
                ),
              ],
            ),
            
            if (event.audioUrl != null) ...[
              const SizedBox(height: 16),
              const Divider(),
              const SizedBox(height: 8),
              // Audio player placeholder
              Row(
                children: [
                  const Icon(Icons.audiotrack, color: Colors.green),
                  const SizedBox(width: 8),
                  const Expanded(
                    child: Text(
                      'Audio Recording Available',
                      style: TextStyle(fontSize: 14),
                    ),
                  ),
                  const Icon(Icons.play_circle_outline, size: 32, color: Colors.green),
                ],
              ),
            ],
            
            if (event.imageUrls.isNotEmpty) ...[
              const SizedBox(height: 16),
              const Divider(),
              const SizedBox(height: 8),
              // Images
              Text(
                '${event.imageUrls.length} Image${event.imageUrls.length > 1 ? 's' : ''} Captured',
                style: const TextStyle(
                  fontSize: 14,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 8),
              SizedBox(
                height: 80,
                child: ListView.builder(
                  scrollDirection: Axis.horizontal,
                  itemCount: event.imageUrls.length,
                  itemBuilder: (context, index) {
                    return Container(
                      margin: const EdgeInsets.only(right: 8),
                      width: 80,
                      decoration: BoxDecoration(
                        color: Colors.grey.shade200,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: Colors.grey.shade300),
                      ),
                      child: ClipRRect(
                        borderRadius: BorderRadius.circular(8),
                        child: Icon(Icons.image, size: 40, color: Colors.grey.shade400),
                        // TODO: Load actual image from Firebase Storage
                        // child: Image.network(event.imageUrls[index], fit: BoxFit.cover),
                      ),
                    );
                  },
                ),
              ),
            ],
            
            if (event.notes != null && event.notes!.isNotEmpty) ...[
              const SizedBox(height: 16),
              const Divider(),
              const SizedBox(height: 8),
              const Text(
                'Notes:',
                style: TextStyle(
                  fontSize: 14,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 4),
              Text(
                event.notes!,
                style: TextStyle(
                  fontSize: 13,
                  color: Colors.grey.shade700,
                ),
              ),
            ],
            
            // Action buttons
            if (event.status != 'resolved') ...[
              const SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  if (event.status == 'new')
                    TextButton.icon(
                      onPressed: () async {
                        await EmergencyService().updateEmergencyStatus(event.id, 'viewed');
                      },
                      icon: const Icon(Icons.visibility, size: 18),
                      label: const Text('Mark Viewed'),
                      style: TextButton.styleFrom(
                        foregroundColor: Colors.orange,
                      ),
                    ),
                  const SizedBox(width: 8),
                  ElevatedButton.icon(
                    onPressed: () async {
                      await EmergencyService().updateEmergencyStatus(event.id, 'resolved');
                    },
                    icon: const Icon(Icons.check, size: 18),
                    label: const Text('Resolve'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.green,
                      foregroundColor: Colors.white,
                    ),
                  ),
                ],
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
}
