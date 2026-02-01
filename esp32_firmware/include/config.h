// Configuration file for KneeGuard ESP32
// This file can be used to customize device settings

#ifndef CONFIG_H
#define CONFIG_H

// BLE Configuration
#define DEVICE_NAME "KneeGuard-ESP32"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Serial Configuration
#define SERIAL_BAUD_RATE 115200

// Sensor Pins (example - customize based on your hardware)
#define LED_PIN 2           // Built-in LED
#define SENSOR_PIN 34       // Analog sensor pin

// Timing Configuration
#define LOOP_DELAY_MS 100   // Main loop delay

// Battery monitoring (example values)
#define BATTERY_PIN 35      // Battery voltage monitoring pin
#define BATTERY_FULL 4.2    // Full battery voltage
#define BATTERY_EMPTY 3.0   // Empty battery voltage

// Enable/Disable features
#define ENABLE_DEBUG_LOGGING true

#endif // CONFIG_H
