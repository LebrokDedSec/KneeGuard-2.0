# ESP32 Firmware

Ten katalog zawiera oprogramowanie dla mikrokontrolera ESP32.

## Struktura

- `src/` - Kod źródłowy
  - `main.cpp` - Główny plik programu
- `include/` - Pliki nagłówkowe
- `lib/` - Lokalne biblioteki (jeśli potrzebne)
- `platformio.ini` - Konfiguracja projektu PlatformIO

## Szybki start

```bash
# Kompilacja
platformio run

# Wgranie na ESP32
platformio run --target upload

# Monitor portu szeregowego
platformio device monitor
```

Zobacz [dokumentację ESP32](../docs/ESP32_SETUP.md) dla szczegółowych instrukcji.
