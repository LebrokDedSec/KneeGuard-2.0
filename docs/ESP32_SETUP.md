# Konfiguracja ESP32

## Wymagania sprzętowe

- Płytka ESP32 DevKit (lub kompatybilna)
- Kabel USB do programowania
- (Opcjonalnie) Czujniki i komponenty do podłączenia

## Instalacja środowiska

### PlatformIO IDE

1. Zainstaluj [Visual Studio Code](https://code.visualstudio.com/)
2. Zainstaluj rozszerzenie PlatformIO IDE z marketplace VS Code
3. Otwórz folder `esp32_firmware` w VS Code

### PlatformIO Core (CLI)

```bash
# Instalacja przez Python pip
pip install platformio

# Sprawdź instalację
pio --version
```

## Kompilacja i wgrywanie firmware

### Przez VS Code

1. Otwórz folder `esp32_firmware` w VS Code
2. Kliknij ikonę PlatformIO na pasku bocznym
3. Wybierz "Build" aby skompilować
4. Podłącz ESP32 przez USB
5. Wybierz "Upload" aby wgrać firmware

### Przez terminal

```bash
cd esp32_firmware

# Kompilacja
platformio run

# Wgranie na ESP32
platformio run --target upload

# Monitor portu szeregowego
platformio device monitor
```

## Konfiguracja pinów

Domyślne piny (można zmienić w `main.cpp`):

```cpp
// Przykładowa konfiguracja pinów dla czujników
#define LED_PIN 2           // Wbudowana dioda LED
#define SENSOR_PIN 34       // Pin analogowy dla czujnika
```

## Biblioteki

Projekt używa następujących bibliotek (automatycznie pobierane przez PlatformIO):

- **NimBLE-Arduino**: Lekka implementacja Bluetooth Low Energy
- **ArduinoJson**: Parsowanie i tworzenie JSON

## Debugowanie

### Monitor szeregowy

```bash
platformio device monitor --baud 115200
```

### Logi BLE

Firmware wyświetla logi komunikacji BLE:
```
Starting KneeGuard ESP32 BLE Server!
Characteristic defined! Now you can read it in your phone!
Client connected
New value: {"command":"status"}
Command received: status
```

## Rozwiązywanie problemów

### ESP32 nie jest wykrywany

1. Sprawdź czy sterowniki USB są zainstalowane (CP210x lub CH340)
2. Sprawdź czy kabel USB obsługuje transmisję danych (nie tylko zasilanie)
3. Spróbuj innego portu USB

### Błąd kompilacji

```bash
# Wyczyść projekt i pobierz biblioteki ponownie
platformio run --target clean
rm -rf .pio
platformio run
```

### Problem z BLE

- Upewnij się, że używasz ESP32 (nie ESP8266)
- Sprawdź czy antena WiFi/BLE jest prawidłowo podłączona
- Zrestartuj ESP32 po wgraniu firmware

## Modyfikacje

### Zmiana nazwy urządzenia BLE

W pliku `src/main.cpp`:
```cpp
NimBLEDevice::init("KneeGuard-ESP32"); // Zmień nazwę tutaj
```

### Zmiana UUID

```cpp
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
```

### Dodawanie czujników

1. Dodaj definicje pinów w sekcji definicji
2. Inicjalizuj czujniki w funkcji `setup()`
3. Odczytuj dane w funkcji `loop()`
4. Wysyłaj dane przez BLE używając JSON

## Przykład rozszerzenia

```cpp
// W setup()
pinMode(SENSOR_PIN, INPUT);

// W loop() lub w odpowiedzi na komendę
int sensorValue = analogRead(SENSOR_PIN);
StaticJsonDocument<200> doc;
doc["sensor"] = sensorValue;
String output;
serializeJson(doc, output);
pCharacteristic->setValue(output.c_str());
pCharacteristic->notify();
```
