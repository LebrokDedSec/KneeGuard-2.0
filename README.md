# KneeGuard 2.0

KneeGuard 2.0 to inteligentne urządzenie oparte na ESP32, które współpracuje z aplikacją mobilną na Androida stworzoną w Flutter. System umożliwia bezprzewodową komunikację i monitorowanie urządzenia poprzez Bluetooth Low Energy (BLE).

## Komponenty projektu

### 1. ESP32 Firmware (`esp32_firmware/`)
Oprogramowanie mikrokontrolera ESP32 odpowiedzialne za:
- Komunikację Bluetooth Low Energy (BLE)
- Obsługę czujników i urządzenia
- Wysyłanie danych do aplikacji mobilnej
- Odbieranie poleceń z aplikacji

### 2. Flutter App (`flutter_app/`)
Aplikacja mobilna na Androida umożliwiająca:
- Skanowanie urządzeń BLE w pobliżu
- Łączenie z urządzeniem KneeGuard ESP32
- Wysyłanie poleceń do urządzenia
- Odbieranie i wyświetlanie danych z urządzenia

## Wymagania

### ESP32 Firmware
- PlatformIO IDE lub PlatformIO Core
- ESP32 DevKit (lub kompatybilna płytka)
- Biblioteki:
  - Arduino Framework
  - NimBLE-Arduino (BLE)
  - ArduinoJson

### Flutter App
- Flutter SDK (>= 3.0.0)
- Android Studio lub VS Code z rozszerzeniem Flutter
- Urządzenie Android z Bluetooth LE (Android 5.0+)

## Instalacja i uruchomienie

### ESP32 Firmware

1. Zainstaluj [PlatformIO](https://platformio.org/install)

2. Przejdź do katalogu firmware:
```bash
cd esp32_firmware
```

3. Skompiluj i wgraj kod na ESP32:
```bash
platformio run --target upload
```

4. Otwórz monitor portu szeregowego:
```bash
platformio device monitor
```

### Flutter App

1. Zainstaluj [Flutter SDK](https://docs.flutter.dev/get-started/install)

2. Przejdź do katalogu aplikacji:
```bash
cd flutter_app
```

3. Pobierz zależności:
```bash
flutter pub get
```

4. Uruchom aplikację na urządzeniu Android:
```bash
flutter run
```

## Architektura komunikacji

### Protokół BLE

**Service UUID**: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
**Characteristic UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`

### Format komunikacji JSON

**Polecenie z aplikacji do ESP32:**
```json
{
  "command": "status"
}
```

**Odpowiedź z ESP32 do aplikacji:**
```json
{
  "status": "active",
  "battery": 85,
  "temperature": 25.5
}
```

## Funkcje

### Obecne
- ✅ Komunikacja BLE między ESP32 a aplikacją Flutter
- ✅ Skanowanie i łączenie z urządzeniami
- ✅ Wysyłanie i odbieranie danych JSON
- ✅ Wyświetlanie stanu urządzenia w aplikacji

### Planowane
- 🔄 Monitorowanie czujników (akcelerometr, żyroskop)
- 🔄 Analiza ruchu kolana
- 🔄 Powiadomienia i alerty
- 🔄 Historia danych i wykresy
- 🔄 Konfiguracja urządzenia przez aplikację

## Struktura projektu

```
KneeGuard-2.0/
├── esp32_firmware/          # Oprogramowanie ESP32
│   ├── src/
│   │   └── main.cpp         # Główny kod firmware
│   ├── include/             # Pliki nagłówkowe
│   ├── lib/                 # Dodatkowe biblioteki
│   └── platformio.ini       # Konfiguracja PlatformIO
│
├── flutter_app/             # Aplikacja Flutter
│   ├── lib/
│   │   └── main.dart        # Główny kod aplikacji
│   ├── android/             # Konfiguracja Android
│   ├── ios/                 # Konfiguracja iOS
│   └── pubspec.yaml         # Zależności Flutter
│
└── docs/                    # Dokumentacja
    ├── ESP32_SETUP.md       # Instrukcja konfiguracji ESP32
    ├── FLUTTER_SETUP.md     # Instrukcja konfiguracji Flutter
    └── COMMUNICATION.md     # Opis protokołu komunikacji
```

## Rozwiązywanie problemów

### ESP32 nie może się połączyć
- Upewnij się, że Bluetooth jest włączony na telefonie
- Sprawdź, czy ESP32 jest zasilany i firmware jest wgrany
- Sprawdź logi na porcie szeregowym ESP32

### Aplikacja nie wykrywa urządzenia
- Sprawdź uprawnienia Bluetooth w ustawieniach Androida
- Upewnij się, że lokalizacja jest włączona (wymagana dla BLE)
- Zrestartuj aplikację i skanowanie

## Licencja

Ten projekt jest open-source i dostępny na licencji MIT.

## Autorzy

Projekt KneeGuard 2.0

## Wsparcie

W razie pytań lub problemów, utwórz issue w repozytorium GitHub.