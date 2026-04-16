### Technichal Specification Document

Dieses Dokument beschreibt die Funktionen und Spezifikationen einer Spielekonsole, die einen ESP8266  als Prozessor und eine 16x16 LED-Matrix als Ausgabe hat. 4 Buttons dienen der Bedienung und Steuerung.

## 1\. Hardware

### 1.1 Microcontroller:

D1 Mini NodeMcu mit ESP8266-12F
(https://www.az-delivery.de/products/d1-mini)
Treiber: "NodeMCU 1.0 (ESP-12E Module)" soll als Hinweis in jedem Code ganz oben stehen.

### 1.2 Display

RGB LED Panel WS2812B 16x16 256 LEDs
([https://www.az-delivery.de/products/rgb-led-panel-ws2812b-16x16-256-leds-flexibel-led-modul-5050smd-ic-einzeladressierbare-vollfarbfunktionen-mit-dc5v-kompatibel-mit-raspberry-pi?\_pos=6&\_sid=7159b8595&_ss=r](https://www.az-delivery.de/products/rgb-led-panel-ws2812b-16x16-256-leds-flexibel-led-modul-5050smd-ic-einzeladressierbare-vollfarbfunktionen-mit-dc5v-kompatibel-mit-raspberry-pi?_pos=6&_sid=7159b8595&_ss=r))

Helligkeit: 20%
Angesteuert über GPIO D4 mit FastLED.
Wlan/Watchdogs deaktiviert.
Framerate abhängig vom jeweiligen Spiel. Ziel: 30 FPS.

LED-Matrix Verkabelung:
- Typ: WS2812B, 16x16, 256 LEDs, Serpentinen-Layout
- Startpunkt: oben rechts
- Zeile 0 (oben): läuft von rechts nach links (Index 0 = oben rechts)
- Zeile 1: läuft von links nach rechts
- Gerade Zeilen (0, 2, 4, ...): rechts → links
- Ungerade Zeilen (1, 3, 5, ...): links → rechts

Koordinaten-Mapping (olb_xy):
  if (y % 2 == 0) → Index = y * 16 + (15 - x)   // rechts→links
  if (y % 2 == 1) → Index = y * 16 + x           // links→rechts

Bilddaten in den FRAME-Arrays sind logisch gespeichert:
  Index 0 = oben links, zeilenweise von links nach rechts.
  Die olb_xy()-Funktion übernimmt die Umsetzung auf die physikalische Verkabelung.

### 1.3 Buttons

4 Pushbuttons an GPIO

|     |     |     |     |
| --- | --- | --- | --- |
| D-PIN | GPIO | Richtung | Buttonfarbe |
| D5  | 14  | links | gelb |
| D2  | 4   | rechts | grün |
| D1  | 5   | unten | weiß |
| D6  | 12  | hoch | blau |

#### 1.3.1 Button Electrical Specification

Die Pushbuttons sind jeweils wie folgt verschaltet:

- Ein Kontakt des Buttons liegt am GPIO-Pin
- Der andere Kontakt liegt an GND
- Es sind keine externen Widerstände verbaut

###### 1.3.1.1 Elektrisches Verhalten

- Interne Pullup-Widerstände des ESP8266 werden verwendet
- Pin-Konfiguration im Code:
    - pinMode(PIN, INPUT_PULLUP)

##### 1.3.2 Logik

- Ruhestand (nicht gedrückt): HIGH
- Gedrückt: LOW
- Interpretation im Code:
    - digitalRead(pin) == LOW → Button gedrückt
    - digitalRead(pin) == HIGH → Button nicht gedrückt

##### 1.3.3 Taster-Auswertung

- Ein Tastendruck wird nur beim Flankenwechsel erkannt:
    - HIGH → LOW
- Gedrückt-Halten darf keine Mehrfachbewegung auslösen
- Entprellung per Software mit 30ms Sperrzeit nach Flankenwechsel

### 1.4 Weiteres

Tonausgabe ist nicht implementiert.
Eine Debug-Ausgabe erfolgt über den Seriellen Monitor.

## 2\. Coding Guidelines

- Der Code muss vollständig Arduino-IDE kompatibel sein
- Alle Funktionen müssen entweder:
    - vor ihrem ersten Aufruf definiert sein
    - oder per Prototyp deklariert werden
- Funktions-Prototypen müssen nach den Includes stehen
- Keine Abhängigkeit von automatischer Arduino-Prototypgenerierung
- Alle Spielparameter müssen als Konstanten im oberen Codebereich definiert sein
- Gameplay-Logik darf nicht blockierend sein (millis-basiert)
- Der Code muss strukturiert sein:
    - Includes
    - Defines
    - Structs
    - Prototypen
    - Globale Variablen
    - setup()
    - loop()
    - Funktionsimplementierungen
- Zufallsfunktionen sollen nicht bei jedem Spielstart die gleichen Werte liefern. Die Werte sollen wirklich zufällig sein: Seed wird aus `analogRead` eines unbeschalteten Pins ermittelt.
- Startbildschirm vor Start eines Spiels.
- Bei Game over ein entsprechender Bildschirm. Neustart nach Tastendruck.
- Weitere Design-Regeln werden im jeweiligen Game Specification Document erläutert.