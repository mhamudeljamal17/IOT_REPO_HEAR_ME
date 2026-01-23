import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:flutter_sound/flutter_sound.dart';
import 'package:firebase_storage/firebase_storage.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:io';
import '../services/notification_service.dart';
import 'package:firebase_database/firebase_database.dart';


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

class _MenteeDetailsPageState extends State<MenteeDetailsPage> {
  final FlutterSoundRecorder _audioRecorder = FlutterSoundRecorder();
  bool _isRecording = false;
  bool _isUploading = false;
  bool _isRecorderInitialized = false;
  String? _recordingPath;

  @override
  void initState() {
    super.initState();
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
        
        final snapshot = await FirebaseFirestore.instance
            .collection('emergencies')
            .where('menteeId', isEqualTo: widget.menteeDocId)
            .where('timestamp', isGreaterThanOrEqualTo: startOfDay)
            .where('timestamp', isLessThanOrEqualTo: endOfDay)
            .get();
        
        dailyCounts.add(snapshot.docs.length);
        
        // Format day label (e.g., "Mon", "Tue")
        final weekdays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
        dayLabels.add(weekdays[date.weekday % 7]);
      }
      
      // Get total emergencies
      final totalSnapshot = await FirebaseFirestore.instance
          .collection('emergencies')
          .where('menteeId', isEqualTo: widget.menteeDocId)
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
      ),
      body: FutureBuilder<Map<String, dynamic>>(
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
      ),
      floatingActionButton: FloatingActionButton.extended(
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
