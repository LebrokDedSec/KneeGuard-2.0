/// Constants for KneeGuard BLE communication
class BleConstants {
  // BLE UUIDs - must match ESP32 firmware configuration
  static const String serviceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  static const String characteristicUuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  
  // Device name
  static const String deviceName = "KneeGuard-ESP32";
  
  // Timeouts
  static const Duration connectionTimeout = Duration(seconds: 10);
  static const Duration scanTimeout = Duration(seconds: 4);
}
