# Wkład do projektu KneeGuard 2.0

Dziękujemy za zainteresowanie projektem KneeGuard 2.0! Przyjmujemy wszelkie formy wkładu.

## Jak mogę pomóc?

### Zgłaszanie błędów

Jeśli znajdziesz błąd:

1. Sprawdź czy błąd nie został już zgłoszony w Issues
2. Utwórz nowy Issue z opisem:
   - Kroki do odtworzenia błędu
   - Oczekiwane zachowanie
   - Rzeczywiste zachowanie
   - Wersja oprogramowania
   - Typ urządzenia (ESP32, telefon)

### Propozycje funkcji

Masz pomysł na nową funkcję?

1. Sprawdź czy nie została już zaproponowana
2. Utwórz Issue opisujące:
   - Cel funkcji
   - Przypadki użycia
   - Propozycja implementacji (opcjonalnie)

### Kod

#### Proces Pull Request

1. Fork repozytorium
2. Utwórz branch dla swojej funkcji (`git checkout -b feature/AmazingFeature`)
3. Commit zmian (`git commit -m 'Dodaj AmazingFeature'`)
4. Push do brancha (`git push origin feature/AmazingFeature`)
5. Otwórz Pull Request

#### Wytyczne kodu

**ESP32 (C++):**
- Używaj formatowania zgodnego z Arduino Style Guide
- Komentuj złożoną logikę
- Testuj na rzeczywistym ESP32 przed PR

**Flutter (Dart):**
- Używaj `dart format` przed commitem
- Przestrzegaj [Dart Style Guide](https://dart.dev/guides/language/effective-dart/style)
- Dodaj komentarze do publicznych API

#### Commit messages

- Używaj języka polskiego lub angielskiego
- Pierwsza linia: krótki opis (max 50 znaków)
- Pusta linia
- Szczegółowy opis (jeśli potrzebny)

Przykład:
```
Dodaj obsługę czujnika akcelerometru

- Implementacja odczytu danych z MPU6050
- Dodanie parsowania JSON dla danych czujnika
- Aktualizacja UI w aplikacji Flutter
```

## Kodeks postępowania

- Bądź przyjazny i pełen szacunku
- Akceptuj konstruktywną krytykę
- Skup się na tym, co najlepsze dla projektu
- Pokaż empatię wobec innych członków społeczności

## Pytania?

Jeśli masz pytania, utwórz Issue z etykietą "question" lub skontaktuj się z maintainerami.

## Licencja

Wnosząc swój wkład, zgadzasz się na licencjonowanie go na licencji MIT, tak jak reszta projektu.
