// =============================================================================
// Board: NodeMCU 1.0 (ESP-12E Module)
// Projekt: openLUXbox - Shining
// =============================================================================

#include <FastLED.h>
#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif
#include "olb_init_v1.h"

// =============================================================================
// 1. DEFINES & KONFIGURATION
// =============================================================================

// Display-Hardware
#define LED_PIN          2
#define MATRIX_WIDTH     16
#define MATRIX_HEIGHT    16
#define NUM_LEDS         (MATRIX_WIDTH * MATRIX_HEIGHT)
#define LED_TYPE         WS2812B
#define COLOR_ORDER      GRB
#define MASTER_BRIGHTNESS 51   // ca. 20% von 255

// Button-Pins (Pull-Up, Active LOW)
#if defined(ESP8266)
    #define BTN_PIN_LEFT     D5   // GPIO 14 (Gelb)
    #define BTN_PIN_RIGHT    D2   // GPIO 4  (Grün)
    #define BTN_PIN_DOWN     D1   // GPIO 5  (Weiß)
    #define BTN_PIN_UP       D6   // GPIO 12 (Blau)
#elif defined(ESP32)
    #define BTN_PIN_LEFT     23   // Gelb
    #define BTN_PIN_RIGHT    19   // Grün
    #define BTN_PIN_DOWN     26   // Weiß
    #define BTN_PIN_UP       18   // Blau
#endif

#define DEBOUNCE_MS      30
#define NUM_BUTTONS      4

// Richtungs- & Farb-Indizes
enum DirIndex {
  DIR_LEFT  = 0, // Gelb  - Schießt von links nach rechts (+X)
  DIR_RIGHT = 1, // Grün  - Schießt von rechts nach links (-X)
  DIR_UP    = 2, // Blau  - Schießt von oben nach unten (+Y)
  DIR_DOWN  = 3  // Weiß  - Schießt von unten nach oben (-Y)
};

// =============================================================================
// 2. SPIELPARAMETER (Anpassbare Variablen/Konstanten)
// =============================================================================

const uint16_t FPS                      = 30;
const uint16_t FRAME_DELAY_MS           = 1000 / FPS;

// Geschwindigkeiten (in Millisekunden pro Bewegungsschritt)
const uint32_t CANNON_MOVE_INTERVAL_MS  = 125;  // 8 Pixel/Sekunde (1000/8)
const uint32_t SHOT_MOVE_INTERVAL_MS    = 30;   // Geschoss-Geschwindigkeit
const uint32_t FREEZE_DURATION_MS       = 1000; // 1 Sekunde Einfrierzeit bei Treffer

// Cluster-Wachstum
const uint32_t CLUSTER_INIT_INTERVAL_MS = 1500; // Start: 1 Punkt pro 2 Sekunden
const uint32_t CLUSTER_SPEEDUP_TIME_MS  = 10000;// Alle 10 Sekunden...
const uint32_t CLUSTER_INTERVAL_DEC_MS  = 50;   // ...um 0,05 Sek (50ms) schneller
const uint32_t CLUSTER_MIN_INTERVAL_MS  = 150;  // Minimale Wachstumszeit

// Wabern (Helligkeits-Oszillation)
const float    WABER_MIN_PERCENT        = 0.02f; // 2%
const float    WABER_MAX_PERCENT        = 0.12f; // 12%
const float    WABER_SPEED              = 0.005f;

// Farben für die 4 Buttons / Richtungen
const CRGB COLOR_LEFT   = CRGB(255, 230, 0);   // Gelb
const CRGB COLOR_RIGHT  = CRGB(0, 255, 0);     // Grün
const CRGB COLOR_UP     = CRGB(0, 100, 255);   // Blau
const CRGB COLOR_DOWN   = CRGB(255, 255, 255); // Weiß

const CRGB BUTTON_COLORS[4] = { COLOR_LEFT, COLOR_RIGHT, COLOR_UP, COLOR_DOWN };

// 3x5 Pixel Font für den Schriftzug "SHINING" (S, H, I, N, G)
const uint8_t CHAR_S[5] = { 0b111, 0b100, 0b111, 0b001, 0b111 };
const uint8_t CHAR_H[5] = { 0b101, 0b101, 0b111, 0b101, 0b101 };
const uint8_t CHAR_I[5] = { 0b111, 0b010, 0b010, 0b010, 0b111 };
const uint8_t CHAR_N[5] = { 0b111, 0b101, 0b101, 0b101, 0b101 };
const uint8_t CHAR_G[5] = { 0b111, 0b100, 0b101, 0b101, 0b111 };

// =============================================================================
// 3. DATENTYPEN & ENUMS
// =============================================================================

enum GameState {
  STATE_TITLE,
  STATE_PLAYING,
  STATE_GAMEOVER
};

struct Button {
  uint8_t  pin;
  bool     lastState;
  bool     pressedEvent;
  uint32_t lastDebounceTime;
};

struct Cannon {
  int8_t   pos;        // Position auf der Kante (1 bis 14)
  int8_t   dir;        // +1 oder -1 (Bewegungsrichtung)
  uint32_t lastMoveTime;
  uint32_t frozenUntil;
};

struct Shot {
  bool     active;
  float    x;
  float    y;
  int8_t   dx;
  int8_t   dy;
  uint8_t  colorIdx;
  uint32_t lastMoveTime;
};

struct ClusterPoint {
  int8_t   x;
  int8_t   y;
  uint8_t  colorIdx;
};

// =============================================================================
// 4. FUNKTIONSPROTOTYPEN
// =============================================================================

void initHardware();
void resetGame();
void respawnCenterCluster();
void triggerGameOverExplosion();
void updateButtons();
bool anyButtonPressed();

void updateGame();
void updateCannons();
void updateShots();
void updateCluster();
bool addClusterPoint(int8_t reqColor = -1);

void renderGame();
void renderWabern();
void renderCluster();
void renderShots();
void renderCannons();
void renderTitleScreen();
void renderGameOverScreen();
CRGB getDiagonalGradient(uint8_t x, uint8_t y);
void drawDigit3x5(uint8_t digit, int8_t startX, int8_t startY);
void drawScore(uint16_t score);

// =============================================================================
// 5. GLOBALE VARIABLEN
// =============================================================================

CRGB leds[NUM_LEDS];

Button buttons[NUM_BUTTONS] = {
  { BTN_PIN_LEFT,  HIGH, false, 0 },
  { BTN_PIN_RIGHT, HIGH, false, 0 },
  { BTN_PIN_UP,    HIGH, false, 0 },
  { BTN_PIN_DOWN,  HIGH, false, 0 }
};

Cannon cannons[4];
Shot   shots[4];

// Cluster-Speicher (Maximaler Innenbereich: 14 x 14 = 196)
#define MAX_CLUSTER_POINTS 200
ClusterPoint cluster[MAX_CLUSTER_POINTS];
uint16_t clusterCount = 0;

// Gitter für schnelle Cluster-Kollisionsabfrage: -1 = leer, 0..3 = Farbe
int8_t grid[MATRIX_WIDTH][MATRIX_HEIGHT];

GameState currentState = STATE_TITLE;
uint16_t  score = 0;

uint32_t gameStartTime        = 0;
uint32_t lastClusterAddTime   = 0;
uint32_t currentClusterRate   = CLUSTER_INIT_INTERVAL_MS;
uint32_t lastSpeedupTime      = 0;
uint32_t gameOverTimer        = 0;

// 3x5 Pixel Font für Zahlen 0-9
const uint8_t DIGITS_3x5[10][5] = {
  { 0b111, 0b101, 0b101, 0b101, 0b111 }, // 0
  { 0b010, 0b110, 0b010, 0b010, 0b111 }, // 1
  { 0b111, 0b001, 0b111, 0b100, 0b111 }, // 2
  { 0b111, 0b001, 0b111, 0b001, 0b111 }, // 3
  { 0b101, 0b101, 0b111, 0b001, 0b001 }, // 4
  { 0b111, 0b100, 0b111, 0b001, 0b111 }, // 5
  { 0b111, 0b100, 0b111, 0b101, 0b111 }, // 6
  { 0b111, 0b001, 0b010, 0b010, 0b010 }, // 7
  { 0b111, 0b101, 0b111, 0b101, 0b111 }, // 8
  { 0b111, 0b101, 0b111, 0b001, 0b111 }  // 9
};

// =============================================================================
// 6. SETUP & LOOP
// =============================================================================

void drawChar3x5(const uint8_t fontChar[5], int8_t startX, int8_t startY, CRGB col) {
  for (uint8_t r = 0; r < 5; r++) {
    uint8_t rowBits = fontChar[r];
    for (uint8_t c = 0; c < 3; c++) {
      if (rowBits & (1 << (2 - c))) {
        int8_t px = startX + c;
        int8_t py = startY + r;
        if (px >= 0 && px < MATRIX_WIDTH && py >= 0 && py < MATRIX_HEIGHT) {
          leds[olb_xy(px, py)] = col;
        }
      }
    }
  }
}

void setup() {
  initHardware();

  // Startanimation
  olb_v1_playIntro(leds);

  currentState = STATE_TITLE;
}

void loop() {
  uint32_t currentMillis = millis();
  static uint32_t lastFrameTime = 0;

  updateButtons();

  switch (currentState) {
    case STATE_TITLE:
      renderTitleScreen();
      if (anyButtonPressed()) {
        resetGame();
        currentState = STATE_PLAYING;
      }
      break;

    case STATE_PLAYING:
      updateGame();
      renderGame();
      break;

    case STATE_GAMEOVER:
      renderGameOverScreen();
      // Kurze Sperrzeit (500ms), damit man nicht versehentlich sofort wegdrückt
      if (currentMillis - gameOverTimer > 500 && anyButtonPressed()) {
        resetGame();
        currentState = STATE_PLAYING;
      }
      break;
  }

  // Framerate Begrenzung auf 30 FPS
  while (millis() - lastFrameTime < FRAME_DELAY_MS) {
    yield();
  }
  lastFrameTime = millis();
  FastLED.show();
}

// =============================================================================
// 7. HARDWARE & KOORDINATENMAPPING
// =============================================================================

void initHardware() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);

  // Echter Zufall über unbeschalteten Analog-Pin
  randomSeed(analogRead(A0) ^ millis());

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].lastState = digitalRead(buttons[i].pin);
    buttons[i].pressedEvent = false;
    buttons[i].lastDebounceTime = 0;
  }

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(MASTER_BRIGHTNESS);
  FastLED.clear(true);
}

void updateButtons() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].pressedEvent = false;
    bool reading = digitalRead(buttons[i].pin);
    if (reading != buttons[i].lastState) {
      if ((now - buttons[i].lastDebounceTime) > DEBOUNCE_MS) {
        if (reading == LOW) { // Flankenwechsel HIGH -> LOW
          buttons[i].pressedEvent = true;
        }
        buttons[i].lastState = reading;
        buttons[i].lastDebounceTime = now;
      }
    }
  }
}

bool anyButtonPressed() {
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (buttons[i].pressedEvent) return true;
  }
  return false;
}

// =============================================================================
// 8. SPIEL-LOGIK & RESET
// =============================================================================

void respawnCenterCluster() {
  // 1. Bildschirm aufblitzen lassen (Weißer Flash)
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(255, 255, 255);
  }
  FastLED.show();
  delay(100);

  // 2. 50 Extrapunkte
  score += 50;

  // 3. Tempo sofort um 3 Stufen steigern
  uint32_t speedupAmount = 3 * CLUSTER_INTERVAL_DEC_MS;
  if (currentClusterRate > (CLUSTER_MIN_INTERVAL_MS + speedupAmount)) {
    currentClusterRate -= speedupAmount;
  } else {
    currentClusterRate = CLUSTER_MIN_INTERVAL_MS;
  }

  // 4. Grid & Cluster säubern
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
      grid[x][y] = -1;
    }
  }
  clusterCount = 0;

  // 5. 4 Startpunkte in der Mitte neu erzeugen
  uint8_t startColors[4] = { 0, 1, 2, 3 };
  for (int i = 3; i > 0; i--) {
    int j = random(i + 1);
    uint8_t temp = startColors[i];
    startColors[i] = startColors[j];
    startColors[j] = temp;
  }

  int8_t cx = 7 + random(2);
  int8_t cy = 7 + random(2);
  grid[cx][cy] = startColors[0];
  cluster[0] = { cx, cy, startColors[0] };
  clusterCount = 1;

  for (uint8_t i = 1; i < 4; i++) {
    addClusterPoint(startColors[i]);
  }

  lastClusterAddTime = millis();
}

// Spektakuläre Farbexplosion bei Game Over
void triggerGameOverExplosion() {
  float maxRadius = 14.0f;

  // 1. Schockwelle expandiert
  for (float r = 0.5f; r <= maxRadius; r += 0.85f) {
    FastLED.clear();
    for (int8_t x = 0; x < MATRIX_WIDTH; x++) {
      for (int8_t y = 0; y < MATRIX_HEIGHT; y++) {
        float dx = x - 7.5f;
        float dy = y - 7.5f;
        float dist = sqrt(dx * dx + dy * dy);
        float diff = fabs(dist - r);

        if (diff < 1.6f) {
          float intensity = 1.0f - (diff / 1.6f);
          // Farbverlauf anhand von Winkel und Radius
          float angle = atan2(dy, dx) + 3.14159f; // 0 .. 2*PI
          uint8_t colorIdx = (uint8_t)(angle / (3.14159f * 0.5f)) % 4;
          CRGB col = BUTTON_COLORS[colorIdx];

          leds[olb_xy(x, y)] = CRGB(col.r * intensity, col.g * intensity, col.b * intensity);
        }
      }
    }
    // Glitzernde Funken
    for (uint8_t s = 0; s < 5; s++) {
      uint8_t sx = random(MATRIX_WIDTH);
      uint8_t sy = random(MATRIX_HEIGHT);
      leds[olb_xy(sx, sy)] = BUTTON_COLORS[random(4)];
    }
    FastLED.show();
    delay(28);
  }

  // 2. Sanftes Ausglimmen / Verglühen
  for (int fade = 0; fade < 12; fade++) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      leds[i].nscale8(180);
    }
    FastLED.show();
    delay(25);
  }
  FastLED.clear(true);
}

void resetGame() {
  score = 0;
  clusterCount = 0;
  gameStartTime = millis();
  lastClusterAddTime = millis();
  lastSpeedupTime = millis();
  currentClusterRate = CLUSTER_INIT_INTERVAL_MS;

  // Grid leeren
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
      grid[x][y] = -1;
    }
  }

  // Schüsse leeren
  for (uint8_t i = 0; i < 4; i++) {
    shots[i].active = false;
  }

  // Kanonen initialisieren (Position 1 bis 14)
  for (uint8_t i = 0; i < 4; i++) {
    cannons[i].pos = 2 + (i * 3); // Leicht versetzte Startpositionen
    cannons[i].dir = (i % 2 == 0) ? 1 : -1;
    cannons[i].lastMoveTime = millis();
    cannons[i].frozenUntil = 0;
  }

  // 4 Startpunkte in der Mitte erzeugen:
  uint8_t startColors[4] = { 0, 1, 2, 3 };
  for (int i = 3; i > 0; i--) {
    int j = random(i + 1);
    uint8_t temp = startColors[i];
    startColors[i] = startColors[j];
    startColors[j] = temp;
  }

  // Erster Punkt
  int8_t cx = 7 + random(2);
  int8_t cy = 7 + random(2);
  grid[cx][cy] = startColors[0];
  cluster[0] = { cx, cy, startColors[0] };
  clusterCount = 1;

  // Die restlichen 3 Punkte angrenzend hinzufügen
  for (uint8_t i = 1; i < 4; i++) {
    addClusterPoint(startColors[i]);
  }
}

void updateGame() {
  uint32_t now = millis();

  // 1. Schwierigkeitsgrad (Cluster-Geschwindigkeit) alle 10 Sek. steigern
  if (now - lastSpeedupTime >= CLUSTER_SPEEDUP_TIME_MS) {
    if (currentClusterRate > (CLUSTER_MIN_INTERVAL_MS + CLUSTER_INTERVAL_DEC_MS)) {
      currentClusterRate -= CLUSTER_INTERVAL_DEC_MS;
    } else {
      currentClusterRate = CLUSTER_MIN_INTERVAL_MS;
    }
    lastSpeedupTime = now;
  }

  // 2. Neuer Cluster-Punkt
  if (now - lastClusterAddTime >= currentClusterRate) {
    bool ok = addClusterPoint(-1); // Beliebige Farbe
    lastClusterAddTime = now;
    if (!ok) {
      // Rand erreicht -> Farbexplosion & Game Over
      triggerGameOverExplosion();
      currentState = STATE_GAMEOVER;
      gameOverTimer = millis();
      return;
    }
  }

  // 3. Kanonen bewegen & Buttons zum Schießen abfragen
  updateCannons();

  // 4. Schüsse bewegen & Kollision prüfen
  updateShots();
}

void updateCannons() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < 4; i++) {
    // Schuss auslösen, falls Button gedrückt und kein Schuss aktiv
    if (buttons[i].pressedEvent && !shots[i].active) {
      shots[i].active = true;
      shots[i].colorIdx = i;
      shots[i].lastMoveTime = now;

      switch (i) {
        case DIR_LEFT:  // Von linker Kante nach rechts (+X)
          shots[i].x = 1;
          shots[i].y = cannons[i].pos;
          shots[i].dx = 1;
          shots[i].dy = 0;
          break;
        case DIR_RIGHT: // Von rechter Kante nach links (-X)
          shots[i].x = 14;
          shots[i].y = cannons[i].pos;
          shots[i].dx = -1;
          shots[i].dy = 0;
          break;
        case DIR_UP:    // Von oberer Kante nach unten (+Y)
          shots[i].x = cannons[i].pos;
          shots[i].y = 1;
          shots[i].dx = 0;
          shots[i].dy = 1;
          break;
        case DIR_DOWN:  // Von unterer Kante nach oben (-Y)
          shots[i].x = cannons[i].pos;
          shots[i].y = 14;
          shots[i].dx = 0;
          shots[i].dy = -1;
          break;
      }
    }

    // Bewegung der Kanone (wenn nicht eingefroren)
    if (now >= cannons[i].frozenUntil) {
      if (now - cannons[i].lastMoveTime >= CANNON_MOVE_INTERVAL_MS) {
        cannons[i].pos += cannons[i].dir;
        // Reflektieren an den Ecken (Bereich 1 bis 14)
        if (cannons[i].pos >= 14) {
          cannons[i].pos = 14;
          cannons[i].dir = -1;
        } else if (cannons[i].pos <= 1) {
          cannons[i].pos = 1;
          cannons[i].dir = 1;
        }
        cannons[i].lastMoveTime = now;
      }
    }
  }
}

void updateShots() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (!shots[i].active) continue;

    // Geschosse bewegen sich im festen Schritt-Intervall
    if (now - shots[i].lastMoveTime >= SHOT_MOVE_INTERVAL_MS) {
      shots[i].lastMoveTime = now;

      int8_t sx = (int8_t)shots[i].x;
      int8_t sy = (int8_t)shots[i].y;

      // 1. Kollisionsprüfung VOR der Bewegung (damit Pixel bei x=1/14 oder y=1/14 getroffen werden)
      if (sx >= 0 && sx < MATRIX_WIDTH && sy >= 0 && sy < MATRIX_HEIGHT) {
        if (grid[sx][sy] != -1) {
          if (grid[sx][sy] == shots[i].colorIdx) {
            grid[sx][sy] = -1; // Treffer! Punkt entfernen
            for (uint16_t c = 0; c < clusterCount; c++) {
              if (cluster[c].x == sx && cluster[c].y == sy) {
                cluster[c] = cluster[clusterCount - 1];
                clusterCount--;
                break;
              }
            }
            shots[i].active = false;
            score++;

            // Wurde der letzte Pixel weggeschossen?
            if (clusterCount == 0) {
              respawnCenterCluster();
            }
            continue; // Schuss verbraucht
          }
        }
      }

      // 2. Jetzt erst um 1 Pixel in Flugrichtung bewegen
      shots[i].x += shots[i].dx;
      shots[i].y += shots[i].dy;
      sx = (int8_t)shots[i].x;
      sy = (int8_t)shots[i].y;

      // 3. Treffer auf gegnerische Kanone an der Zielkante (friert Gegner ein)
      if (i == DIR_LEFT && sx >= 15) {
        if (cannons[DIR_RIGHT].pos == sy) {
          cannons[DIR_RIGHT].frozenUntil = now + FREEZE_DURATION_MS;
        }
        shots[i].active = false;
        continue;
      } else if (i == DIR_RIGHT && sx <= 0) {
        if (cannons[DIR_LEFT].pos == sy) {
          cannons[DIR_LEFT].frozenUntil = now + FREEZE_DURATION_MS;
        }
        shots[i].active = false;
        continue;
      } else if (i == DIR_UP && sy >= 15) {
        if (cannons[DIR_DOWN].pos == sx) {
          cannons[DIR_DOWN].frozenUntil = now + FREEZE_DURATION_MS;
        }
        shots[i].active = false;
        continue;
      } else if (i == DIR_DOWN && sy <= 0) {
        if (cannons[DIR_UP].pos == sx) {
          cannons[DIR_UP].frozenUntil = now + FREEZE_DURATION_MS;
        }
        shots[i].active = false;
        continue;
      }

      // 4. Spielfeld verlassen? -> Geschoss verpufft
      if (sx < 0 || sx >= MATRIX_WIDTH || sy < 0 || sy >= MATRIX_HEIGHT) {
        shots[i].active = false;
      }
    }
  }
}

// Fügt einen Punkt an einer freien, angrenzenden Position hinzu
bool addClusterPoint(int8_t reqColor) {
  if (clusterCount == 0) return false;

  // Freie Nachbarpositionen suchen
  struct Cand { int8_t x, y; };
  Cand candidates[MAX_CLUSTER_POINTS * 4];
  uint16_t candCount = 0;

  const int8_t dxs[4] = { 0, 0, -1, 1 };
  const int8_t dys[4] = { -1, 1, 0, 0 };

  for (uint16_t i = 0; i < clusterCount; i++) {
    for (uint8_t d = 0; d < 4; d++) {
      int8_t nx = cluster[i].x + dxs[d];
      int8_t ny = cluster[i].y + dys[d];

      // Liegt im Spielfeld?
      if (nx >= 0 && nx < MATRIX_WIDTH && ny >= 0 && ny < MATRIX_HEIGHT) {
        if (grid[nx][ny] == -1) {
          // Prüfen, ob bereits in Kandidatenliste
          bool alreadyIn = false;
          for (uint16_t k = 0; k < candCount; k++) {
            if (candidates[k].x == nx && candidates[k].y == ny) {
              alreadyIn = true;
              break;
            }
          }
          if (!alreadyIn) {
            candidates[candCount++] = { nx, ny };
          }
        }
      }
    }
  }

  if (candCount == 0) return false; // Kein Platz mehr

  // Zufälligen Kandidaten auswählen
  Cand chosen = candidates[random(candCount)];

  // Game-Over-Bedingung: Wenn ein Punkt auf einer Außenlinie erscheint
  if (chosen.x == 0 || chosen.x == 15 || chosen.y == 0 || chosen.y == 15) {
    return false;
  }

  uint8_t cColor = (reqColor >= 0) ? reqColor : random(4);

  grid[chosen.x][chosen.y] = cColor;
  cluster[clusterCount++] = { chosen.x, chosen.y, cColor };

  return true;
}

// =============================================================================
// 9. RENDERING & GRAFIK
// =============================================================================

void renderGame() {
  FastLED.clear();

  renderWabern();  // 1. Wabern (Halos um Punkte)
  renderCluster(); // 2. Feste Cluster-Punkte
  renderShots();   // 3. Geschosse
  renderCannons(); // 4. Kanonen am Rand
}

void renderWabern() {
  uint32_t now = millis();
  // Sinus-Oszillation zwischen WABER_MIN und WABER_MAX
  float wave = (sin(now * WABER_SPEED) + 1.0f) * 0.5f; // 0.0 .. 1.0
  float factor = WABER_MIN_PERCENT + wave * (WABER_MAX_PERCENT - WABER_MIN_PERCENT);

  const int8_t dxs[4] = { 0, 0, -1, 1 };
  const int8_t dys[4] = { -1, 1, 0, 0 };

  for (int8_t x = 1; x < 15; x++) {
    for (int8_t y = 1; y < 15; y++) {
      if (grid[x][y] != -1) continue; // Ist bereits Clusterpunkt

      uint16_t r = 0, g = 0, b = 0;
      uint8_t neighborCount = 0;

      for (uint8_t d = 0; d < 4; d++) {
        int8_t nx = x + dxs[d];
        int8_t ny = y + dys[d];
        if (nx >= 0 && nx < 16 && ny >= 0 && ny < 16) {
          int8_t c = grid[nx][ny];
          if (c != -1) {
            r += BUTTON_COLORS[c].r;
            g += BUTTON_COLORS[c].g;
            b += BUTTON_COLORS[c].b;
            neighborCount++;
          }
        }
      }

      if (neighborCount > 0) {
        // Mischfarbe berechnen
        CRGB mixed = CRGB(r / neighborCount, g / neighborCount, b / neighborCount);
        leds[olb_xy(x, y)] = CRGB(mixed.r * factor, mixed.g * factor, mixed.b * factor);
      }
    }
  }
}

void renderCluster() {
  for (uint16_t i = 0; i < clusterCount; i++) {
    int8_t x = cluster[i].x;
    int8_t y = cluster[i].y;
    leds[olb_xy(x, y)] = BUTTON_COLORS[cluster[i].colorIdx];
  }
}

void renderShots() {
  for (uint8_t i = 0; i < 4; i++) {
    if (shots[i].active) {
      int8_t sx = (int8_t)round(shots[i].x);
      int8_t sy = (int8_t)round(shots[i].y);
      if (sx >= 0 && sx < MATRIX_WIDTH && sy >= 0 && sy < MATRIX_HEIGHT) {
        leds[olb_xy(sx, sy)] = BUTTON_COLORS[shots[i].colorIdx];
      }
    }
  }
}

void renderCannons() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < 4; i++) {
    int8_t cx = 0, cy = 0;
    switch (i) {
      case DIR_LEFT:  cx = 0;  cy = cannons[i].pos; break;
      case DIR_RIGHT: cx = 15; cy = cannons[i].pos; break;
      case DIR_UP:    cx = cannons[i].pos; cy = 0;  break;
      case DIR_DOWN:  cx = cannons[i].pos; cy = 15; break;
    }

    CRGB c = BUTTON_COLORS[i];

    // Wenn eingefroren: schnelles Blinken / gedimmtes Eisblau
    if (now < cannons[i].frozenUntil) {
      if ((now / 100) % 2 == 0) {
        c = CRGB(60, 180, 255); // Frost-Farbe
      } else {
        c = CRGB(10, 30, 60);
      }
    }

    leds[olb_xy(cx, cy)] = c;
  }
}

// =============================================================================
// 10. MENÜ- & GAMEOVER-BILDSCHIRME
// =============================================================================

void renderTitleScreen() {
  FastLED.clear();
  uint32_t now = millis();

  // 1. Aurora-Wabern im Hintergrund
  for (int8_t x = 0; x < MATRIX_WIDTH; x++) {
    for (int8_t y = 0; y < MATRIX_HEIGHT; y++) {
      float dx = x - 7.5f;
      float dy = y - 7.5f;
      float dist = sqrt(dx * dx + dy * dy);

      float wave = sin((dist * 0.8f) - (now * 0.005f));
      wave = (wave + 1.0f) * 0.5f;

      float brightness = wave * (1.0f - (dist / 12.0f));
      if (brightness < 0.0f) brightness = 0.0f;

      uint8_t colorProgress = (uint8_t)((dist * 18.0f) + (now * 0.04f)) % 255;
      CRGB baseColor;
      if (colorProgress < 64) {
        baseColor = blend(COLOR_LEFT, COLOR_RIGHT, colorProgress * 4);
      } else if (colorProgress < 128) {
        baseColor = blend(COLOR_RIGHT, COLOR_UP, (colorProgress - 64) * 4);
      } else if (colorProgress < 192) {
        baseColor = blend(COLOR_UP, COLOR_DOWN, (colorProgress - 128) * 4);
      } else {
        baseColor = blend(COLOR_DOWN, COLOR_LEFT, (colorProgress - 192) * 4);
      }

      // Hintergrund etwas sanfter/dunkler, damit die Schrift klar lesbar ist
      leds[olb_xy(x, y)] = CRGB(
        baseColor.r * (brightness * 0.22f),
        baseColor.g * (brightness * 0.22f),
        baseColor.b * (brightness * 0.22f)
      );
    }
  }

  // 2. Kristall-Kern in der Mitte (rotiert farblich)
  float coreBreath = (sin(now * 0.008f) + 1.0f) * 0.5f;
  float coreIntensity = 0.5f + (coreBreath * 0.5f);
  uint8_t shift = (now / 400) % 4;
  leds[olb_xy(7, 7)] = CRGB(BUTTON_COLORS[shift].r * coreIntensity,
                            BUTTON_COLORS[shift].g * coreIntensity,
                            BUTTON_COLORS[shift].b * coreIntensity);
  leds[olb_xy(8, 7)] = CRGB(BUTTON_COLORS[(shift + 1) % 4].r * coreIntensity,
                            BUTTON_COLORS[(shift + 1) % 4].g * coreIntensity,
                            BUTTON_COLORS[(shift + 1) % 4].b * coreIntensity);
  leds[olb_xy(8, 8)] = CRGB(BUTTON_COLORS[(shift + 2) % 4].r * coreIntensity,
                            BUTTON_COLORS[(shift + 2) % 4].g * coreIntensity,
                            BUTTON_COLORS[(shift + 2) % 4].b * coreIntensity);
  leds[olb_xy(7, 8)] = CRGB(BUTTON_COLORS[(shift + 3) % 4].r * coreIntensity,
                            BUTTON_COLORS[(shift + 3) % 4].g * coreIntensity,
                            BUTTON_COLORS[(shift + 3) % 4].b * coreIntensity);

  // 3. Scrollender Text "SHINING" mit 3s Start- & 3s Endpause
  // Phasen: 0..3000ms (Pause 1) | 3000..7000ms (Scroll 4s) | 7000..10000ms (Pause 2)
  const uint32_t SCROLL_PERIOD_MS = 10000;
  const uint32_t PAUSE_START_MS   = 3000;
  const uint32_t SCROLL_TIME_MS   = 4000;

  uint32_t cycleTime = now % SCROLL_PERIOD_MS;

  if (cycleTime >= PAUSE_START_MS && cycleTime < (PAUSE_START_MS + SCROLL_TIME_MS)) {
    uint32_t scrollProgress = cycleTime - PAUSE_START_MS;
    // Läuft in 4 Sekunden von ganz rechts (X=16) bis ganz links aus dem Bild (X=-32)
    int16_t startX = 16 - (int16_t)((scrollProgress * 48) / SCROLL_TIME_MS);
    int8_t textY = 5; // Zentriert auf 16 Pixel Höhe (5 bis 9)

    const uint8_t* word[7] = { CHAR_S, CHAR_H, CHAR_I, CHAR_N, CHAR_I, CHAR_N, CHAR_G };

    for (uint8_t i = 0; i < 7; i++) {
      int8_t letterX = startX + (i * 4); // 3 Pixel Zeichen + 1 Pixel Abstand
      drawChar3x5(word[i], letterX, textY, CRGB(255, 255, 255));
    }
  }

  // 4. "Shining"-Glitzern / Sparkles
  if (random8() < 60) {
    uint8_t sparkX = 1 + random8(14);
    uint8_t sparkY = 1 + random8(14);
    leds[olb_xy(sparkX, sparkY)] += CRGB(200, 200, 255);
  }
}

void renderGameOverScreen() {
  FastLED.clear();
  // Score anzeigen mit stufenlosem Diagonal-Farbverlauf
  drawScore(score);
}

// =============================================================================
// 11. SCORE RENDERING (Stufenloser diagonaler Farbverlauf von oben links nach unten rechts)
// =============================================================================

CRGB getDiagonalGradient(uint8_t x, uint8_t y) {
  // Diagonaler Wert von 0 (oben-links) bis 30 (unten-rechts)
  // Skaliert auf 0 .. 255 für flüssige Farbübergänge
  uint8_t pos = ((x + y) * 255) / 30;

  if (pos < 85) {
    // Phase 1: Gelb -> Grün
    uint8_t frac = (pos * 255) / 85;
    return blend(COLOR_LEFT, COLOR_RIGHT, frac);
  } else if (pos < 170) {
    // Phase 2: Grün -> Blau
    uint8_t frac = ((pos - 85) * 255) / 85;
    return blend(COLOR_RIGHT, COLOR_UP, frac);
  } else {
    // Phase 3: Blau -> Weiß
    uint8_t frac = ((pos - 170) * 255) / 85;
    return blend(COLOR_UP, COLOR_DOWN, frac);
  }
}

void drawDigit3x5(uint8_t digit, int8_t startX, int8_t startY) {
  if (digit > 9) return;
  for (uint8_t r = 0; r < 5; r++) {
    uint8_t rowBits = DIGITS_3x5[digit][r];
    for (uint8_t c = 0; c < 3; c++) {
      if (rowBits & (1 << (2 - c))) {
        int8_t px = startX + c;
        int8_t py = startY + r;
        if (px >= 0 && px < MATRIX_WIDTH && py >= 0 && py < MATRIX_HEIGHT) {
          leds[olb_xy(px, py)] = getDiagonalGradient(px, py);
        }
      }
    }
  }
}

void drawScore(uint16_t s) {
  // 1-stellige, 2-stellige oder 3-stellige Zentrierung
  if (s < 10) {
    // 1 Ziffer zentriert bei X=6, Y=5
    drawDigit3x5(s, 6, 5);
  } else if (s < 100) {
    // 2 Ziffern zentriert bei X=4 und X=8, Y=5
    drawDigit3x5(s / 10, 4, 5);
    drawDigit3x5(s % 10, 9, 5);
  } else {
    // 3 Ziffern bei X=2, X=6, X=10, Y=5 (bis 999)
    if (s > 999) s = 999;
    drawDigit3x5(s / 100, 2, 5);
    drawDigit3x5((s / 10) % 10, 6, 5);
    drawDigit3x5(s % 10, 10, 5);
  }
}
