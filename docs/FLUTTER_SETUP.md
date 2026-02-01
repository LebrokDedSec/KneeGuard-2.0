# Konfiguracja aplikacji Flutter

## Wymagania

- Flutter SDK >= 3.0.0
- Android Studio lub VS Code z rozszerzeniem Flutter
- Urządzenie Android (fizyczne lub emulator) z Android 5.0+
- Bluetooth Low Energy (BLE) na urządzeniu

## Instalacja Flutter SDK

### Windows

```bash
# Pobierz Flutter SDK z https://docs.flutter.dev/get-started/install/windows
# Rozpakuj do C:\flutter
# Dodaj do PATH: C:\flutter\bin
```

### macOS

```bash
# Użyj Homebrew
brew install flutter

# Lub pobierz manualnie
# https://docs.flutter.dev/get-started/install/macos
```

### Linux

```bash
# Pobierz i rozpakuj Flutter
cd ~
wget https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_3.x.x-stable.tar.xz
tar xf flutter_linux_3.x.x-stable.tar.xz

# Dodaj do PATH
export PATH="$PATH:`pwd`/flutter/bin"
```

## Weryfikacja instalacji

```bash
flutter doctor
```

Upewnij się, że wszystkie wymagane komponenty są zainstalowane.

## Konfiguracja projektu

### Instalacja zależności

```bash
cd flutter_app
flutter pub get
```

### Uruchomienie aplikacji

```bash
# Lista dostępnych urządzeń
flutter devices

# Uruchom na podłączonym urządzeniu
flutter run

# Uruchom w trybie debug
flutter run --debug

# Uruchom w trybie release
flutter run --release
```

## Struktura projektu Flutter

```
flutter_app/
├── lib/
│   └── main.dart           # Główny plik aplikacji
├── android/                # Konfiguracja Android
│   └── app/
│       └── src/main/
│           └── AndroidManifest.xml
├── pubspec.yaml            # Zależności projektu
└── README.md
```

## Uprawnienia Android

Aplikacja wymaga następujących uprawnień (automatycznie dodane w `AndroidManifest.xml`):

- `BLUETOOTH`
- `BLUETOOTH_ADMIN`
- `BLUETOOTH_SCAN`
- `BLUETOOTH_CONNECT`
- `ACCESS_FINE_LOCATION`
- `ACCESS_COARSE_LOCATION`

### Udzielanie uprawnień

1. Zainstaluj aplikację na urządzeniu
2. Przejdź do Ustawienia > Aplikacje > KneeGuard
3. Uprawnienia > Zezwól na wszystkie uprawnienia
4. Włącz lokalizację na urządzeniu (wymagana dla BLE)

## Użyte biblioteki

### flutter_blue_plus
Biblioteka do komunikacji BLE:
```yaml
flutter_blue_plus: ^1.31.0
```

### permission_handler
Zarządzanie uprawnieniami:
```yaml
permission_handler: ^11.0.0
```

### provider
Zarządzanie stanem aplikacji:
```yaml
provider: ^6.0.5
```

## Debugowanie

### Logi Flutter

```bash
# Podgląd logów w czasie rzeczywistym
flutter logs

# Czyszczenie logów
flutter logs --clear
```

### Debugowanie BLE

W kodzie dodano logi do konsoli:
```dart
debugPrint('Error starting scan: $e');
debugPrint('Client connected');
```

### Android Logcat

```bash
# Filtruj logi tylko dla aplikacji
adb logcat -s flutter

# Wszystkie logi
adb logcat
```

## Rozwiązywanie problemów

### Aplikacja nie kompiluje się

```bash
# Wyczyść projekt
flutter clean

# Pobierz zależności ponownie
flutter pub get

# Zaktualizuj Flutter
flutter upgrade
```

### Błąd z uprawnieniami

1. Sprawdź `AndroidManifest.xml`
2. Odinstaluj i zainstaluj aplikację ponownie
3. Upewnij się, że lokalizacja jest włączona

### BLE nie działa

1. Sprawdź czy Bluetooth jest włączony
2. Sprawdź czy lokalizacja jest włączona (Android wymaga tego dla BLE)
3. Uruchom aplikację ponownie
4. Sprawdź czy urządzenie obsługuje BLE:
```dart
await FlutterBluePlus.isSupported
```

### Emulator vs urządzenie fizyczne

- Emulator może nie obsługiwać BLE prawidłowo
- Zalecane jest testowanie na fizycznym urządzeniu Android

## Modyfikacje

### Zmiana UUID BLE

W pliku `lib/main.dart`:
```dart
static const String SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static const String CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
```

### Dodawanie nowych ekranów

1. Utwórz nowy plik w `lib/screens/`
2. Zaimplementuj `StatefulWidget` lub `StatelessWidget`
3. Dodaj nawigację w `main.dart`

### Zmiana stylu aplikacji

W pliku `main.dart`:
```dart
theme: ThemeData(
  primarySwatch: Colors.blue, // Zmień kolor
  useMaterial3: true,
),
```

## Budowanie APK

### Debug APK

```bash
flutter build apk --debug
```

### Release APK

```bash
flutter build apk --release
```

APK znajduje się w: `build/app/outputs/flutter-apk/`

### App Bundle (Google Play)

```bash
flutter build appbundle --release
```

## Testowanie

### Testy jednostkowe

```bash
flutter test
```

### Testy integracyjne

```bash
flutter test integration_test
```

## Dokumentacja

- [Flutter Documentation](https://docs.flutter.dev/)
- [Flutter Blue Plus](https://pub.dev/packages/flutter_blue_plus)
- [Permission Handler](https://pub.dev/packages/permission_handler)
