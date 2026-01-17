# Szybki start - KneeGuard 2.0

Ten przewodnik pomoże Ci szybko uruchomić projekt KneeGuard 2.0.

## Przegląd

KneeGuard 2.0 składa się z dwóch części:
1. **ESP32 Firmware** - oprogramowanie dla urządzenia
2. **Flutter App** - aplikacja mobilna na Androida

## Krok 1: Przygotowanie ESP32

### Wymagania
- Płytka ESP32
- Kabel USB
- PlatformIO

### Instalacja PlatformIO

**Metoda 1: VS Code (zalecane)**
```bash
# 1. Zainstaluj Visual Studio Code
# 2. Zainstaluj rozszerzenie "PlatformIO IDE"
# 3. Uruchom ponownie VS Code
```

**Metoda 2: CLI**
```bash
pip install platformio
```

### Wgrywanie firmware

```bash
cd esp32_firmware
platformio run --target upload
platformio device monitor
```

Powinieneś zobaczyć:
```
Starting KneeGuard ESP32 BLE Server!
Characteristic defined! Now you can read it in your phone!
```

## Krok 2: Konfiguracja aplikacji Flutter

### Wymagania
- Flutter SDK >= 3.0.0
- Urządzenie Android z BLE

### Instalacja Flutter

**Windows:**
```bash
# Pobierz z https://docs.flutter.dev/get-started/install/windows
# Rozpakuj i dodaj do PATH
```

**macOS:**
```bash
brew install flutter
```

**Linux:**
```bash
# Pobierz Flutter SDK
# Dodaj do PATH
```

### Sprawdź instalację
```bash
flutter doctor
```

### Uruchomienie aplikacji

```bash
cd flutter_app
flutter pub get
flutter run
```

## Krok 3: Testowanie połączenia

### Na ESP32
1. Sprawdź monitor portu szeregowego
2. ESP32 powinien rozgłaszać "KneeGuard-ESP32"

### W aplikacji
1. Otwórz aplikację na telefonie
2. Kliknij "Scan for Devices"
3. Znajdź "KneeGuard-ESP32" na liście
4. Kliknij na urządzenie aby się połączyć
5. Kliknij "Request Status" aby przetestować komunikację

### Oczekiwane rezultaty

**ESP32 Serial Monitor:**
```
Client connected
*********
New value: {"command":"status"}
Command received: status
*********
```

**Aplikacja Flutter:**
```
Connection Status: Connected
Device Data:
  Status: active
  Battery: 85%
  Temperature: 25.5°C
```

## Rozwiązywanie problemów

### ESP32 nie jest widoczny

**Problem:** ESP32 nie pojawia się w liście urządzeń

**Rozwiązanie:**
1. Sprawdź czy firmware jest wgrany (monitor portu szeregowego)
2. Upewnij się że Bluetooth jest włączony
3. Włącz lokalizację na telefonie (wymagane dla BLE)
4. Zrestartuj ESP32 i aplikację

### Błąd uprawnień w aplikacji

**Problem:** "Permission denied" lub nie można skanować

**Rozwiązanie:**
1. Przejdź do Ustawienia > Aplikacje > KneeGuard
2. Uprawnienia > Włącz wszystkie
3. Sprawdź czy lokalizacja jest włączona systemowo
4. Uruchom aplikację ponownie

### Błędy kompilacji

**ESP32:**
```bash
# Wyczyść i przebuduj
cd esp32_firmware
platformio run --target clean
platformio run
```

**Flutter:**
```bash
# Wyczyść i pobierz zależności
cd flutter_app
flutter clean
flutter pub get
flutter run
```

## Następne kroki

Po pomyślnym uruchomieniu:

1. **Poznaj dokumentację:**
   - [ESP32 Setup](docs/ESP32_SETUP.md) - szczegółowa konfiguracja ESP32
   - [Flutter Setup](docs/FLUTTER_SETUP.md) - szczegółowa konfiguracja Flutter
   - [Communication Protocol](docs/COMMUNICATION.md) - protokół komunikacji

2. **Eksperymentuj z kodem:**
   - Dodaj nowe czujniki do ESP32
   - Rozbuduj interfejs aplikacji
   - Implementuj nowe komendy

3. **Wnieś swój wkład:**
   - Zobacz [CONTRIBUTING.md](CONTRIBUTING.md)
   - Zgłaszaj błędy i propozycje

## Wsparcie

Potrzebujesz pomocy?
- Utwórz Issue na GitHubie
- Sprawdź istniejące Issues
- Przeczytaj dokumentację w folderze `docs/`

## Licencja

Projekt na licencji MIT - zobacz [LICENSE](LICENSE)
