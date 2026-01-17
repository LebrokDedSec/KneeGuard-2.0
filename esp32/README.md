# Folder ESP32

Ten katalog przeznaczony jest na firmware dla ESP32.

Struktura:
- `src/` — pliki źródłowe
- `include/` — nagłówki

Mogę przygotować projekt PlatformIO lub ESP-IDF (konfiguracja, `platformio.ini`, przykładowy szkic).
 
## Uruchomienie (PlatformIO + Arduino)

- Wymagany jest VS Code z rozszerzeniem PlatformIO lub CLI (`pip install platformio`).
- W folderze [esp32](esp32) znajduje się konfiguracja [esp32/platformio.ini](esp32/platformio.ini) dla płytki `esp32dev` (framework Arduino).

### Budowanie i wgrywanie (CLI)

```powershell
cd "c:\Users\DELL\KneeGuard-2.0\KneeGuard-2.0\esp32"
pio run
pio run -t upload
pio device monitor -b 115200
```

### Kod migającej diody
- Plik: [esp32/src/main.cpp](esp32/src/main.cpp)
- Domyślny pin diody to `LED_BUILTIN` (fallback na GPIO 2). Jeśli Twoja płytka ma diodę na innym GPIO (np. 5), zmień wartość w pliku `main.cpp` lub zdefiniuj `LED_BUILTIN` odpowiednio.

### Uwaga dotycząca pinu LED
Nie wszystkie moduły ESP32 mają wbudowaną diodę. Popularne devkity używają GPIO 2; jeśli dioda nie miga, podepnij zewnętrzną LED z rezystorem do wybranego GPIO i ustaw ten pin w kodzie.