# Flutter App

Aplikacja mobilna na Androida dla KneeGuard 2.0.

## Struktura

- `lib/` - Kod źródłowy aplikacji
  - `main.dart` - Główny plik aplikacji
- `android/` - Konfiguracja platformy Android
- `ios/` - Konfiguracja platformy iOS
- `pubspec.yaml` - Zależności projektu

## Szybki start

```bash
# Instalacja zależności
flutter pub get

# Uruchomienie aplikacji
flutter run

# Budowanie APK
flutter build apk --release
```

Zobacz [dokumentację Flutter](../docs/FLUTTER_SETUP.md) dla szczegółowych instrukcji.

## Funkcje

- Skanowanie urządzeń BLE
- Łączenie z ESP32
- Wysyłanie poleceń
- Odbieranie danych w czasie rzeczywistym
- Wyświetlanie statusu urządzenia
