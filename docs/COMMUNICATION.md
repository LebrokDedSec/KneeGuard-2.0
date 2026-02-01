# Protokół komunikacji BLE

## Przegląd

KneeGuard 2.0 używa Bluetooth Low Energy (BLE) do komunikacji między ESP32 a aplikacją mobilną Flutter. Komunikacja odbywa się poprzez charakterystykę BLE obsługującą odczyt, zapis i powiadomienia.

## Specyfikacja BLE

### Service UUID
```
4fafc201-1fb5-459e-8fcc-c5c9c331914b
```

### Characteristic UUID
```
beb5483e-36e1-4688-b7f5-ea07361b26a8
```

### Właściwości charakterystyki
- **READ**: Odczyt danych z ESP32
- **WRITE**: Zapis danych do ESP32
- **NOTIFY**: Powiadomienia o zmianach danych

## Format danych

Wszystkie dane wymieniane między urządzeniami są w formacie JSON zakodowanym w UTF-8.

### Struktura komunikatów

#### Polecenia (App → ESP32)

Format podstawowy:
```json
{
  "command": "nazwa_polecenia",
  "params": {
    "param1": "wartość1",
    "param2": "wartość2"
  }
}
```

#### Odpowiedzi (ESP32 → App)

Format podstawowy:
```json
{
  "status": "success|error",
  "data": {
    "key1": "value1",
    "key2": "value2"
  },
  "timestamp": 1234567890
}
```

## Dostępne polecenia

### 1. Status urządzenia

**Polecenie:**
```json
{
  "command": "status"
}
```

**Odpowiedź:**
```json
{
  "status": "active",
  "battery": 85,
  "temperature": 25.5,
  "timestamp": 1234567890
}
```

### 2. Konfiguracja (planowane)

**Polecenie:**
```json
{
  "command": "config",
  "params": {
    "sampling_rate": 100,
    "threshold": 50
  }
}
```

**Odpowiedź:**
```json
{
  "status": "success",
  "message": "Configuration updated"
}
```

### 3. Odczyt czujników (planowane)

**Polecenie:**
```json
{
  "command": "read_sensors"
}
```

**Odpowiedź:**
```json
{
  "accelerometer": {
    "x": 0.5,
    "y": -0.3,
    "z": 9.8
  },
  "gyroscope": {
    "x": 0.1,
    "y": 0.2,
    "z": -0.1
  },
  "timestamp": 1234567890
}
```

### 4. Reset urządzenia (planowane)

**Polecenie:**
```json
{
  "command": "reset"
}
```

**Odpowiedź:**
```json
{
  "status": "success",
  "message": "Device will reset in 3 seconds"
}
```

## Sekwencja połączenia

### 1. Skanowanie

```
App: Rozpoczyna skanowanie BLE
ESP32: Rozgłasza swoją obecność z nazwą "KneeGuard-ESP32"
```

### 2. Połączenie

```
App: Łączy się z urządzeniem
ESP32: Akceptuje połączenie, wywołuje onConnect()
```

### 3. Odkrywanie usług

```
App: Odkrywa dostępne usługi i charakterystyki
ESP32: Udostępnia Service i Characteristic
```

### 4. Subskrypcja powiadomień

```
App: Włącza powiadomienia dla charakterystyki
ESP32: Gotowy do wysyłania powiadomień
```

### 5. Wymiana danych

```
App → ESP32: Wysyła polecenie (write)
ESP32 → App: Wysyła odpowiedź (notify)
```

## Implementacja po stronie ESP32

### Odbieranie danych

```cpp
void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, value.c_str());
    
    if (!error) {
        const char* command = doc["command"];
        // Przetwarzanie polecenia
    }
}
```

### Wysyłanie danych

```cpp
void sendData() {
    StaticJsonDocument<200> doc;
    doc["status"] = "active";
    doc["battery"] = 85;
    
    String output;
    serializeJson(doc, output);
    
    pCharacteristic->setValue(output.c_str());
    pCharacteristic->notify();
}
```

## Implementacja po stronie Flutter

### Wysyłanie polecenia

```dart
Future<void> sendCommand(String command) async {
  if (targetCharacteristic != null) {
    Map<String, dynamic> commandJson = {"command": command};
    String jsonString = json.encode(commandJson);
    await targetCharacteristic!.write(utf8.encode(jsonString));
  }
}
```

### Odbieranie danych

```dart
characteristic.lastValueStream.listen((value) {
  if (value.isNotEmpty) {
    String jsonString = utf8.decode(value);
    Map<String, dynamic> data = json.decode(jsonString);
    // Przetwarzanie danych
  }
});
```

## Obsługa błędów

### Timeout połączenia

```dart
try {
  await device.connect(timeout: Duration(seconds: 10));
} catch (e) {
  print('Connection timeout: $e');
}
```

### Błąd parsowania JSON

```cpp
DeserializationError error = deserializeJson(doc, value.c_str());
if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
}
```

### Utrata połączenia

```cpp
void onDisconnect(NimBLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected");
    pServer->startAdvertising(); // Wznów rozgłaszanie
}
```

## Bezpieczeństwo

### Obecne
- Podstawowa komunikacja BLE bez szyfrowania

### Planowane
- Parowanie urządzeń
- Szyfrowanie połączenia BLE
- Autoryzacja poleceń
- Weryfikacja integralności danych

## Optymalizacja

### Minimalizacja rozmiaru pakietów
- Używaj krótkich kluczy JSON
- Unikaj nadmiarowych danych
- Kompresja danych jeśli potrzebna

### Zarządzanie energią
- Wysyłaj dane tylko gdy to konieczne
- Użyj connection interval dostosowanego do potrzeb
- Wyłącz powiadomienia gdy nie są używane

### Niezawodność
- Implementuj retry dla krytycznych poleceń
- Dodaj timeout dla operacji
- Waliduj dane przed wysłaniem i po odbiorze

## Diagram sekwencji

```
App                 ESP32
 |                    |
 |-- Scan Start ----->|
 |<-- Advertising ----|
 |                    |
 |-- Connect -------->|
 |<-- Connected ------|
 |                    |
 |-- Discover ------->|
 |<-- Services -------|
 |                    |
 |-- Subscribe ------>|
 |<-- Subscribed -----|
 |                    |
 |-- Write Command -->|
 |                    |-- Process
 |                    |
 |<-- Notify Data ----|
 |                    |
```

## Rozszerzanie protokołu

Aby dodać nowe polecenie:

1. Zdefiniuj strukturę JSON polecenia i odpowiedzi
2. Dodaj obsługę w `onWrite()` callback w ESP32
3. Dodaj metodę wysyłania w aplikacji Flutter
4. Zaktualizuj dokumentację
5. Przetestuj komunikację end-to-end
