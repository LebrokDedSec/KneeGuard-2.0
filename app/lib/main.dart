import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:fl_chart/fl_chart.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'KneeGuard',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        primaryColor: const Color(0xFFF2C400),
        scaffoldBackgroundColor: const Color(0xFF4B4B4B),
        appBarTheme: const AppBarTheme(
          backgroundColor: Color(0xFFF2C400), // top bar color
          titleTextStyle: TextStyle(color: Color(0xFF0B0B0B), fontSize: 20, fontWeight: FontWeight.w600), // title color
          iconTheme: IconThemeData(color: Color(0xFF0B0B0B)),
        ),
        elevatedButtonTheme: ElevatedButtonThemeData(
          style: ElevatedButton.styleFrom(
            backgroundColor: const Color(0xFFF2C400),
            foregroundColor: const Color(0xFF0B0B0B),
          ),
        ),
        cardColor: const Color(0xFF5A5A5A),
        textTheme: const TextTheme(
          bodyLarge: TextStyle(color: Colors.white),
          bodyMedium: TextStyle(color: Color(0xFFECECEC)),
          titleLarge: TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
        ),
      ),
      home: const HomeScreen(),
    );
  }
}

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  List<BluetoothDevice> _devices = [];
  BluetoothDevice? _selected;
  BluetoothConnection? _connection;
  bool _isConnecting = false;
  bool _isConnected = false;

  // buffer for incoming bytes
  String _buffer = '';

  // latest parsed values
  final Map<String, String> _latest = {
    'time': '-',
    'roll1': '-',
    'pitch1': '-',
    'yaw1': '-',
    'roll2': '-',
    'pitch2': '-',
    'yaw2': '-',
  };

  // History for chart (IMU Shank: roll1, pitch1, yaw1)
  final List<double> _roll1History = [];
  final List<double> _pitch1History = [];
  final List<double> _yaw1History = [];
  // History for chart (IMU Thigh: roll2, pitch2, yaw2)
  final List<double> _roll2History = [];
  final List<double> _pitch2History = [];
  final List<double> _yaw2History = [];
  final int _maxHistoryLength = 100; // keep last 100 samples

  // Chart visibility toggles
  bool _showRoll1 = true;
  bool _showPitch1 = true;
  bool _showYaw1 = true;
  bool _showRoll2 = true;
  bool _showPitch2 = true;
  bool _showYaw2 = true;

  // Knee angle calculation
  double _kneeAngle = 0.0;
  final List<double> _kneeAngleHistory = [];

  @override
  void initState() {
    super.initState();
    // ensure runtime permissions and Bluetooth are enabled first
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      await _ensurePermissions();
      await _ensureBluetoothEnabled();
      _getPairedDevices();
    });
  }

  Future<void> _ensurePermissions() async {
    // Only request on Android; on other platforms permissions may not be required.
    try {
      // Construct the list of permissions we want to request
      final permissions = <Permission>[
        Permission.bluetooth,
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
        Permission.locationWhenInUse,
      ];

      // Request all
      final statuses = await permissions.request();

      // If any are permanently denied, ask user to open app settings
      bool anyPermanentlyDenied = statuses.values.any((s) => s.isPermanentlyDenied);
      bool anyDenied = statuses.values.any((s) => s.isDenied || s.isRestricted || s.isLimited);

      if (anyPermanentlyDenied) {
        await showDialog<void>(
          context: context,
          builder: (c) => AlertDialog(
            title: const Text('Permissions required'),
            content: const Text('Bluetooth and location permissions are required. Please enable them in app settings.'),
            actions: [
              TextButton(onPressed: () => Navigator.of(c).pop(), child: const Text('Cancel')),
              TextButton(onPressed: () { openAppSettings(); Navigator.of(c).pop(); }, child: const Text('Open settings')),
            ],
          ),
        );
      } else if (anyDenied) {
        // Try requesting once more if the user temporarily denied
        await permissions.request();
      }
    } catch (e) {
      // Ignore; permission_handler may throw on platforms where permissions are not defined.
    }
  }

  Future<void> _ensureBluetoothEnabled() async {
    try {
      final state = await FlutterBluetoothSerial.instance.state;
      if (state == BluetoothState.STATE_OFF) {
        // ask user to enable bluetooth
        await FlutterBluetoothSerial.instance.requestEnable();
      }
    } catch (e) {
      // ignore errors — user may enable Bluetooth manually
    }
  }

  Future<void> _getPairedDevices() async {
    try {
      List<BluetoothDevice> bonded = await FlutterBluetoothSerial.instance.getBondedDevices();
      setState(() => _devices = bonded);
    } catch (e) {
      setState(() => _devices = []);
    }
  }

  Future<void> _connectTo(BluetoothDevice d) async {
    if (_isConnected || _isConnecting) return;
    setState(() {
      _isConnecting = true;
      _selected = d;
    });

    try {
      BluetoothConnection connection = await BluetoothConnection.toAddress(d.address);
      setState(() {
        _connection = connection;
        _isConnected = true;
        _isConnecting = false;
      });

      connection.input?.listen(_onDataReceived).onDone(() {
        // connection closed
        setState(() {
          _isConnected = false;
          _connection = null;
        });
      });
    } catch (e) {
      setState(() {
        _isConnecting = false;
        _isConnected = false;
        _connection = null;
      });
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to connect: $e')));
    }
  }

  void _onDataReceived(Uint8List data) {
    // append incoming bytes to buffer
    String incoming = utf8.decode(data, allowMalformed: true);
    _buffer += incoming;

    int idx;
    while ((idx = _buffer.indexOf('\n')) != -1) {
      String line = _buffer.substring(0, idx).trim();
      _buffer = _buffer.substring(idx + 1);
      if (line.isNotEmpty) _processLine(line);
    }
  }

  void _processLine(String line) {
    // expected CSV: time,roll1,pitch1,yaw1,roll2,pitch2,yaw2
    List<String> parts = line.split(',');
    if (parts.length >= 7) {
      setState(() {
        _latest['time'] = parts[0];
        _latest['roll1'] = parts[1];
        _latest['pitch1'] = parts[2];
        _latest['yaw1'] = parts[3];
        _latest['roll2'] = parts[4];
        _latest['pitch2'] = parts[5];
        _latest['yaw2'] = parts[6];

        // Add to history for chart (IMU Shank)
        double? roll1 = double.tryParse(parts[1]);
        double? pitch1 = double.tryParse(parts[2]);
        double? yaw1 = double.tryParse(parts[3]);

        if (roll1 != null) {
          _roll1History.add(roll1);
          if (_roll1History.length > _maxHistoryLength) _roll1History.removeAt(0);
        }
        if (pitch1 != null) {
          _pitch1History.add(pitch1);
          if (_pitch1History.length > _maxHistoryLength) _pitch1History.removeAt(0);
        }
        if (yaw1 != null) {
          _yaw1History.add(yaw1);
          if (_yaw1History.length > _maxHistoryLength) _yaw1History.removeAt(0);
        }

        // Add to history for chart (IMU Thigh)
        double? roll2 = double.tryParse(parts[4]);
        double? pitch2 = double.tryParse(parts[5]);
        double? yaw2 = double.tryParse(parts[6]);

        if (roll2 != null) {
          _roll2History.add(roll2);
          if (_roll2History.length > _maxHistoryLength) _roll2History.removeAt(0);
        }
        if (pitch2 != null) {
          _pitch2History.add(pitch2);
          if (_pitch2History.length > _maxHistoryLength) _pitch2History.removeAt(0);
        }
        if (yaw2 != null) {
          _yaw2History.add(yaw2);
          if (_yaw2History.length > _maxHistoryLength) _yaw2History.removeAt(0);
        }

        // Calculate knee angle (difference in roll between Thigh and Shank)
        // Knee flexion angle = roll of thigh - roll of shank
        if (roll1 != null && roll2 != null) {
          _kneeAngle = (roll2 - roll1).abs();
          _kneeAngleHistory.add(_kneeAngle);
          if (_kneeAngleHistory.length > _maxHistoryLength) _kneeAngleHistory.removeAt(0);
        }
      });
    }
  }

  Future<void> _disconnect() async {
    if (_connection != null) {
      await _connection!.close();
      _connection = null;
    }
    setState(() {
      _isConnected = false;
      _selected = null;
    });
  }

  Future<void> _sendCalibration() async {
    if (_connection != null && _connection!.isConnected) {
      try {
        _connection!.output.add(utf8.encode('calib\n'));
        await _connection!.output.allSent;
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Calibration command sent')),
        );
      } catch (e) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to send calib: $e')),
        );
      }
    }
  }

  @override
  void dispose() {
    _disconnect();
    super.dispose();
  }

  Widget _buildDeviceTile(BluetoothDevice d) {
    bool isSelected = _selected?.address == d.address;
    return ListTile(
      title: Text(d.name ?? d.address, style: const TextStyle(color: Colors.white)),
      subtitle: Text(d.address, style: const TextStyle(color: Color(0xFFECECEC))),
      trailing: isSelected && _isConnected
          ? const Text('connected', style: TextStyle(color: Colors.lightGreenAccent))
          : ElevatedButton(
              child: const Text('Connect'),
              onPressed: () => _connectTo(d),
            ),
    );
  }

  Widget _buildImuTab() {
    return Padding(
      padding: const EdgeInsets.all(12.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Time container (left) and Calibration button (right) side by side
          Row(
            children: [
              // Time container (left)
              Expanded(
                flex: 2,
                child: Card(
                  color: const Color(0xFF5A5A5A),
                  child: Padding(
                    padding: const EdgeInsets.all(12.0),
                    child: Center(
                      child: Text(
                        'Time: ${_latest['time']}',
                        style: const TextStyle(fontSize: 16, color: Colors.white, fontWeight: FontWeight.w500),
                      ),
                    ),
                  ),
                ),
              ),
              const SizedBox(width: 12),
              // Calibration button (right)
              Expanded(
                flex: 1,
                child: ElevatedButton(
                  onPressed: _isConnected ? _sendCalibration : null,
                  style: ElevatedButton.styleFrom(
                    backgroundColor: const Color(0xFFF2C400),
                    foregroundColor: const Color(0xFF0B0B0B),
                    padding: const EdgeInsets.symmetric(vertical: 12),
                  ),
                  child: const Text('Calibration', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          
          // Two containers side by side for IMU1 and IMU2
          Row(
            children: [
              // IMU Shank container
              Expanded(
                  child: Card(
                    color: const Color(0xFF5A5A5A),
                    child: Padding(
                      padding: const EdgeInsets.all(12.0),
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.start,
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('IMU Shank', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Color(0xFFF2C400))),
                          const SizedBox(height: 12),
                          Text('Roll: ${_latest['roll1']}°', style: const TextStyle(fontSize: 16, color: Colors.white)),
                          const SizedBox(height: 8),
                          Text('Pitch: ${_latest['pitch1']}°', style: const TextStyle(fontSize: 16, color: Colors.white)),
                          const SizedBox(height: 8),
                          Text('Yaw: ${_latest['yaw1']}°', style: const TextStyle(fontSize: 16, color: Colors.white)),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                // IMU Thigh container
                Expanded(
                  child: Card(
                    color: const Color(0xFF5A5A5A),
                    child: Padding(
                      padding: const EdgeInsets.all(12.0),
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.start,
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('IMU Thigh', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Color(0xFFF2C400))),
                          const SizedBox(height: 12),
                          Text('Roll: ${_latest['roll2']}°', style: const TextStyle(fontSize: 16, color: Colors.white)),
                          const SizedBox(height: 8),
                          Text('Pitch: ${_latest['pitch2']}°', style: const TextStyle(fontSize: 16, color: Colors.white)),
                          const SizedBox(height: 8),
                          Text('Yaw: ${_latest['yaw2']}°', style: const TextStyle(fontSize: 16, color: Colors.white)),
                        ],
                      ),
                    ),
                  ),
                ),
              ],
            ),
          const SizedBox(height: 16),
          
          // Chart for both IMUs (Roll, Pitch, Yaw over time)
          const Text('IMU History - Shank & Thigh', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Colors.white)),
          const SizedBox(height: 8),
          SizedBox(
            height: 200,
            child: Card(
              color: const Color(0xFF5A5A5A),
              child: Padding(
                padding: const EdgeInsets.all(12.0),
                child: _roll1History.isEmpty
                    ? const Center(child: Text('Waiting for data...', style: TextStyle(color: Color(0xFFECECEC))))
                    : LineChart(
                        LineChartData(
                          gridData: FlGridData(
                            show: true,
                            drawVerticalLine: true,
                            getDrawingHorizontalLine: (value) => FlLine(
                              color: const Color(0xFF3A3A3A),
                              strokeWidth: 1,
                            ),
                            getDrawingVerticalLine: (value) => FlLine(
                              color: const Color(0xFF3A3A3A),
                              strokeWidth: 1,
                            ),
                          ),
                          titlesData: FlTitlesData(
                            show: true,
                            rightTitles: const AxisTitles(
                              sideTitles: SideTitles(showTitles: false),
                            ),
                            topTitles: const AxisTitles(
                              sideTitles: SideTitles(showTitles: false),
                            ),
                            bottomTitles: AxisTitles(
                              sideTitles: SideTitles(
                                showTitles: true,
                                reservedSize: 22,
                                getTitlesWidget: (value, meta) => Text(
                                  value.toInt().toString(),
                                  style: const TextStyle(color: Color(0xFFECECEC), fontSize: 10),
                                ),
                              ),
                            ),
                            leftTitles: AxisTitles(
                              sideTitles: SideTitles(
                                showTitles: true,
                                reservedSize: 40,
                                getTitlesWidget: (value, meta) => Text(
                                  value.toInt().toString(),
                                  style: const TextStyle(color: Color(0xFFECECEC), fontSize: 10),
                                ),
                              ),
                            ),
                          ),
                          borderData: FlBorderData(
                            show: true,
                            border: Border.all(color: const Color(0xFF3A3A3A)),
                          ),
                          minX: 0,
                          maxX: (_roll1History.length - 1).toDouble(),
                          minY: -180,
                          maxY: 180,
                          lineBarsData: [
                            // IMU Shank - Roll line (red)
                            if (_showRoll1)
                              LineChartBarData(
                                spots: List.generate(
                                  _roll1History.length,
                                  (index) => FlSpot(index.toDouble(), _roll1History[index]),
                                ),
                                isCurved: true,
                                color: Colors.red,
                                barWidth: 2,
                                dotData: const FlDotData(show: false),
                                belowBarData: BarAreaData(show: false),
                              ),
                            // IMU Shank - Pitch line (green)
                            if (_showPitch1)
                              LineChartBarData(
                                spots: List.generate(
                                  _pitch1History.length,
                                  (index) => FlSpot(index.toDouble(), _pitch1History[index]),
                                ),
                                isCurved: true,
                                color: Colors.green,
                                barWidth: 2,
                                dotData: const FlDotData(show: false),
                                belowBarData: BarAreaData(show: false),
                              ),
                            // IMU Shank - Yaw line (blue)
                            if (_showYaw1)
                              LineChartBarData(
                                spots: List.generate(
                                  _yaw1History.length,
                                  (index) => FlSpot(index.toDouble(), _yaw1History[index]),
                                ),
                                isCurved: true,
                                color: Colors.blue,
                                barWidth: 2,
                                dotData: const FlDotData(show: false),
                                belowBarData: BarAreaData(show: false),
                              ),
                            // IMU Thigh - Roll line (orange)
                            if (_showRoll2)
                              LineChartBarData(
                                spots: List.generate(
                                  _roll2History.length,
                                  (index) => FlSpot(index.toDouble(), _roll2History[index]),
                                ),
                                isCurved: true,
                                color: Colors.orange,
                                barWidth: 2,
                                dotData: const FlDotData(show: false),
                                belowBarData: BarAreaData(show: false),
                              ),
                            // IMU Thigh - Pitch line (lightGreen)
                            if (_showPitch2)
                              LineChartBarData(
                                spots: List.generate(
                                  _pitch2History.length,
                                  (index) => FlSpot(index.toDouble(), _pitch2History[index]),
                                ),
                                isCurved: true,
                                color: Colors.lightGreen,
                                barWidth: 2,
                                dotData: const FlDotData(show: false),
                                belowBarData: BarAreaData(show: false),
                              ),
                            // IMU Thigh - Yaw line (cyan)
                            if (_showYaw2)
                              LineChartBarData(
                                spots: List.generate(
                                  _yaw2History.length,
                                  (index) => FlSpot(index.toDouble(), _yaw2History[index]),
                                ),
                                isCurved: true,
                                color: Colors.cyan,
                                barWidth: 2,
                                dotData: const FlDotData(show: false),
                                belowBarData: BarAreaData(show: false),
                              ),
                          ],
                          lineTouchData: const LineTouchData(enabled: false),
                        ),
                      ),
              ),
            ),
          ),
          const SizedBox(height: 8),
          // Legend - Shank
          const Text('Shank:', style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: Color(0xFFECECEC))),
          const SizedBox(height: 4),
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              _buildToggleLegendItem(Colors.red, 'Roll', _showRoll1, (val) => setState(() => _showRoll1 = val)),
              const SizedBox(width: 12),
              _buildToggleLegendItem(Colors.green, 'Pitch', _showPitch1, (val) => setState(() => _showPitch1 = val)),
              const SizedBox(width: 12),
              _buildToggleLegendItem(Colors.blue, 'Yaw', _showYaw1, (val) => setState(() => _showYaw1 = val)),
            ],
          ),
          const SizedBox(height: 8),
          // Legend - Thigh
          const Text('Thigh:', style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: Color(0xFFECECEC))),
          const SizedBox(height: 4),
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              _buildToggleLegendItem(Colors.orange, 'Roll', _showRoll2, (val) => setState(() => _showRoll2 = val)),
              const SizedBox(width: 12),
              _buildToggleLegendItem(Colors.lightGreen, 'Pitch', _showPitch2, (val) => setState(() => _showPitch2 = val)),
              const SizedBox(width: 12),
              _buildToggleLegendItem(Colors.cyan, 'Yaw', _showYaw2, (val) => setState(() => _showYaw2 = val)),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildToggleLegendItem(Color color, String label, bool isVisible, Function(bool) onChanged) {
    return InkWell(
      onTap: () => onChanged(!isVisible),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          SizedBox(
            width: 20,
            height: 20,
            child: Checkbox(
              value: isVisible,
              onChanged: (val) => onChanged(val ?? false),
              activeColor: color,
              checkColor: const Color(0xFF0B0B0B),
              side: BorderSide(color: color, width: 2),
            ),
          ),
          const SizedBox(width: 4),
          Container(
            width: 16,
            height: 3,
            color: color,
          ),
          const SizedBox(width: 4),
          Text(label, style: const TextStyle(color: Colors.white, fontSize: 12)),
        ],
      ),
    );
  }

  Widget _buildAngleTab() {
    return Padding(
      padding: const EdgeInsets.all(12.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Current knee angle display
          Card(
            color: const Color(0xFF5A5A5A),
            child: Padding(
              padding: const EdgeInsets.all(24.0),
              child: Column(
                children: [
                  const Text(
                    'Knee Flexion Angle',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Colors.white),
                  ),
                  const SizedBox(height: 16),
                  Text(
                    '${_kneeAngle.toStringAsFixed(1)}°',
                    style: const TextStyle(fontSize: 48, fontWeight: FontWeight.bold, color: Color(0xFFF2C400)),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Calculated from Thigh-Shank roll difference',
                    style: const TextStyle(fontSize: 12, color: Color(0xFFECECEC)),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),
          
          // Angle history chart
          const Text('Angle History', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Colors.white)),
          const SizedBox(height: 8),
          Expanded(
            child: Card(
              color: const Color(0xFF5A5A5A),
              child: Padding(
                padding: const EdgeInsets.all(12.0),
                child: _kneeAngleHistory.isEmpty
                    ? const Center(child: Text('Waiting for data...', style: TextStyle(color: Color(0xFFECECEC))))
                    : LineChart(
                        LineChartData(
                          gridData: FlGridData(
                            show: true,
                            drawVerticalLine: true,
                            getDrawingHorizontalLine: (value) => FlLine(
                              color: const Color(0xFF3A3A3A),
                              strokeWidth: 1,
                            ),
                            getDrawingVerticalLine: (value) => FlLine(
                              color: const Color(0xFF3A3A3A),
                              strokeWidth: 1,
                            ),
                          ),
                          titlesData: FlTitlesData(
                            show: true,
                            rightTitles: const AxisTitles(
                              sideTitles: SideTitles(showTitles: false),
                            ),
                            topTitles: const AxisTitles(
                              sideTitles: SideTitles(showTitles: false),
                            ),
                            bottomTitles: AxisTitles(
                              sideTitles: SideTitles(
                                showTitles: true,
                                reservedSize: 22,
                                getTitlesWidget: (value, meta) => Text(
                                  value.toInt().toString(),
                                  style: const TextStyle(color: Color(0xFFECECEC), fontSize: 10),
                                ),
                              ),
                            ),
                            leftTitles: AxisTitles(
                              sideTitles: SideTitles(
                                showTitles: true,
                                reservedSize: 40,
                                getTitlesWidget: (value, meta) => Text(
                                  '${value.toInt()}°',
                                  style: const TextStyle(color: Color(0xFFECECEC), fontSize: 10),
                                ),
                              ),
                            ),
                          ),
                          borderData: FlBorderData(
                            show: true,
                            border: Border.all(color: const Color(0xFF3A3A3A)),
                          ),
                          minX: 0,
                          maxX: (_kneeAngleHistory.length - 1).toDouble(),
                          minY: 0,
                          maxY: 180,
                          lineBarsData: [
                            LineChartBarData(
                              spots: List.generate(
                                _kneeAngleHistory.length,
                                (index) => FlSpot(index.toDouble(), _kneeAngleHistory[index]),
                              ),
                              isCurved: true,
                              color: const Color(0xFFF2C400),
                              barWidth: 3,
                              dotData: const FlDotData(show: false),
                              belowBarData: BarAreaData(
                                show: true,
                                color: const Color(0xFFF2C400).withOpacity(0.2),
                              ),
                            ),
                          ],
                          lineTouchData: const LineTouchData(enabled: false),
                        ),
                      ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSetupTab() {
    return Padding(
      padding: const EdgeInsets.all(12.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Text('Paired devices', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Colors.white)),
          const SizedBox(height: 8),
          Expanded(
            flex: 2,
            child: _devices.isEmpty
                ? const Center(
                    child: Text(
                      'No paired devices. Pair the ESP (KneeGuard) in system settings first.',
                      style: TextStyle(color: Color(0xFFECECEC)),
                      textAlign: TextAlign.center,
                    ),
                  )
                : ListView.separated(
                    itemBuilder: (_, i) => _buildDeviceTile(_devices[i]),
                    separatorBuilder: (_, __) => const Divider(),
                    itemCount: _devices.length,
                  ),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                  child: Text(
                'Status: ${_isConnected ? 'Connected to ${_selected?.name ?? _selected?.address}' : _isConnecting ? 'Connecting...' : 'Disconnected'}',
                style: const TextStyle(color: Colors.white),
              )),
              if (_isConnected) ...[
                const SizedBox(width: 8),
                ElevatedButton(onPressed: _sendCalibration, child: const Text('Calibrate')),
                const SizedBox(width: 8),
                ElevatedButton(onPressed: _disconnect, child: const Text('Disconnect'))
              ]
            ],
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 3,
      child: Scaffold(
        appBar: AppBar(
          title: const Text('KneeGuard Viewer'),
          bottom: const TabBar(
            tabs: [
              Tab(text: 'IMU'),
              Tab(text: 'Angle'),
              Tab(text: 'Setup'),
            ],
            labelColor: Color(0xFF0B0B0B),
            unselectedLabelColor: Color(0xFF0B0B0B),
            indicatorColor: Color(0xFF0B0B0B),
          ),
          actions: [
            IconButton(
              icon: const Icon(Icons.refresh),
              onPressed: _getPairedDevices,
              tooltip: 'Refresh paired devices',
            ),
          ],
        ),
        body: TabBarView(
          children: [
            _buildImuTab(),
            _buildAngleTab(),
            _buildSetupTab(),
          ],
        ),
      ),
    );
  }
}
