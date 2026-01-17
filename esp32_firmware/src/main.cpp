#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

// BLE Server Configuration
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

NimBLEServer* pServer = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    };

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected");
    }
};

class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        
        if (value.length() > 0) {
            Serial.println("*********");
            Serial.print("New value: ");
            for (int i = 0; i < value.length(); i++)
                Serial.print(value[i]);
            Serial.println();
            Serial.println("*********");
            
            // Parse JSON command from Flutter app
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, value.c_str());
            
            if (!error) {
                const char* command = doc["command"];
                Serial.print("Command received: ");
                Serial.println(command);
                
                // Handle different commands
                if (strcmp(command, "status") == 0) {
                    sendStatus();
                }
            }
        }
    }
    
    void sendStatus() {
        StaticJsonDocument<200> doc;
        doc["status"] = "active";
        doc["battery"] = 85;
        doc["temperature"] = 25.5;
        
        String output;
        serializeJson(doc, output);
        pCharacteristic->setValue(output.c_str());
        pCharacteristic->notify();
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting KneeGuard ESP32 BLE Server!");

    // Create the BLE Device
    NimBLEDevice::init("KneeGuard-ESP32");

    // Create the BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        NIMBLE_PROPERTY::READ   |
                        NIMBLE_PROPERTY::WRITE  |
                        NIMBLE_PROPERTY::NOTIFY
                      );

    pCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    NimBLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
    // Handle connection state changes
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }
    
    // Connecting
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
    
    delay(100);
}
