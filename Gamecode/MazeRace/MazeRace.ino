// ============================================================================
// openLUXbox - MazeRace
// Board-Treiber: NodeMCU 1.0 (ESP-12E Module)
// ============================================================================

#include <FastLED.h>
#include "olb_init_v1.h"

// ============================================================================
// HARDWARE- & DISPLAY-KONFIGURATION (gemäß TSD)
// ============================================================================
#define LED_PIN       D4      // Datenausgang zur WS2812B Matrix
#define COLOR_ORDER   GRB
#define CHIPSET       WS2812B
#define MATRIX_WIDTH  16
#define MATRIX_HEIGHT 16
#define NUM_LEDS      (MATRIX_WIDTH * MATRIX_HEIGHT)

// Taster-Pins (mit internem Pullup)
#define PIN_D1        D1
#define PIN_D2        D2
#define PIN_D5        D5
#define PIN_D6        D6

#define BTN_UP        PIN_D6  // Oben
#define BTN_DOWN      PIN_D1  // Unten
#define BTN_LEFT      PIN_D5  // Links
#define BTN_RIGHT     PIN_D2  // Rechts

CRGB leds[NUM_LEDS];

// ============================================================================
// ANPASSBARE SPIELPARAMETER
// ============================================================================
const unsigned long INITIAL_TIME_MS            = 25000UL;  // Start-Zeit (30 Sek.)
const unsigned long TIME_REDUCTION_MS          = 1000UL;   // Zeitabzug pro Level
const unsigned long MIN_TIME_LIMIT_MS          = 7000UL;   // Mindestzeit

const unsigned long INITIAL_ROTATE_INTERVAL_MS = 6000UL;   // Start Rotations-Intervall
const unsigned long ROTATE_REDUCTION_MS        = 400UL;    // Verkürzung pro Level
const unsigned long MIN_ROTATE_INTERVAL_MS     = 4000UL;   // Kürzestes Intervall
const unsigned long ROTATE_FADE_OUT_MS         = 1000UL;   // Fade-Out
const unsigned long ROTATE_FADE_IN_MS          = 1000UL;   // Fade-In

// Crash-Phasen-Timing
const unsigned long CRASH_FLASH_MS             = 70UL;     // Kurzer greller Blitz der Wände
const unsigned long CRASH_BLACK_MS             = 250UL;    // Komplett schwarz
const unsigned long CRASH_PAUSE_MS             = 130UL;    // Kurze Pause im Dunkeln vor Wiederkehr
const unsigned long CRASH_TOTAL_MS             = (CRASH_FLASH_MS + CRASH_BLACK_MS + CRASH_PAUSE_MS);

// Schwierigkeits-Progression
const uint8_t       LEVELS_PER_DIFFICULTY_STEP = 1;        // Alle 2 Level steigt die Labyrinthkomplexität
const uint8_t       DIFFICULTY_RAMP_STEPS      = 2;        // Zusätzliche Pfadlänge pro Stufe

const float         PULSE_FREQ_HZ              = 3.0f;     // Pulsieren Spieler
const uint8_t       PULSE_BRIGHT_MIN           = 130;
const uint8_t       PULSE_BRIGHT_MAX           = 255;

// Farbpalette
const CRGB COLOR_BG          = CRGB(2, 4, 10);
const CRGB COLOR_WALL        = CRGB(12, 38, 110);
const CRGB COLOR_WALL_CRASH  = CRGB(240, 240, 240);
const CRGB COLOR_TIMER_FULL  = CRGB(60, 60, 60);
const CRGB COLOR_TIMER_EMPTY = CRGB(0, 0, 0);
const CRGB COLOR_EXIT        = CRGB(0, 230, 255);
const CRGB COLOR_PLAYER      = CRGB(255, 200, 0);

// ============================================================================
// STATUSMASCHINE & DATENTYPEN
// ============================================================================
enum GameState {
  STATE_INTRO,
  STATE_PLAYING,
  STATE_CRASH_FREEZE,
  STATE_ROTATING,
  STATE_LEVEL_CLEAR,
  STATE_GAME_OVER,
  STATE_SCORE_DISPLAY
};

GameState currentState = STATE_INTRO;

enum CellType : uint8_t {
  CELL_CORRIDOR = 0,
  CELL_WALL     = 1,
  CELL_TIMER    = 2,
  CELL_EXIT     = 3
};

CellType grid[MATRIX_WIDTH][MATRIX_HEIGHT];

int8_t playerX = 0;
int8_t playerY = 0;
int8_t startX  = 0;
int8_t startY  = 0;
int8_t exitX   = 0;
int8_t exitY   = 0;
bool   playerHasLeftStart = false;

unsigned long levelStartTime = 0;
unsigned long levelTotalDuration = 0;
unsigned long levelElapsedMs = 0;
unsigned long stateEnterTime = 0;

unsigned long nextRotateCountdown = 0;
uint8_t rotationPhase = 0;
uint8_t rotationAngle = 0;

uint16_t currentLevel = 1;

// Mini-Font (3x5)
const uint8_t PROGMEM FONT_3X5[][3] = {
  {0x1F, 0x11, 0x1F}, // 0
  {0x00, 0x1F, 0x00}, // 1
  {0x1D, 0x15, 0x17}, // 2
  {0x15, 0x15, 0x1F}, // 3
  {0x07, 0x04, 0x1F}, // 4
  {0x17, 0x15, 0x1D}, // 5
  {0x1F, 0x15, 0x1D}, // 6
  {0x01, 0x01, 0x1F}, // 7
  {0x1F, 0x15, 0x1F}, // 8
  {0x17, 0x15, 0x1F}, // 9
  {0x04, 0x04, 0x04}, // - (10)
  {0x1F, 0x06, 0x1F}, // M (11)
  {0x1E, 0x05, 0x1E}, // A (12)
  {0x19, 0x15, 0x13}, // Z (13)
  {0x1F, 0x15, 0x11}, // E (14)
  {0x1F, 0x09, 0x16}, // R (15)
  {0x0E, 0x11, 0x11}  // C (16)
};

// ============================================================================
// PROTOTYPEN
// ============================================================================
uint16_t XY(uint8_t x, uint8_t y);
void updateButtons();
bool anyButtonPressed();
void clearButtonPresses();
void seedRandomGenerator();
int getShortestPath(int8_t sX, int8_t sY, int8_t eX, int8_t eY, CellType m[MATRIX_WIDTH][MATRIX_HEIGHT]);
void generateMazeForLevel(uint16_t lvl);
void generateDecorativeMaze();
void rotateGrid(uint8_t steps);
void drawChar3x5(char c, int startX, int startY, CRGB color);
void drawChar3x5Gradient(char c, int startX, int startY, uint8_t alpha);
void renderGame(uint8_t brightnessFactor = 255, bool flashWallsWhite = false);
void startLevel(uint16_t lvl);

// ============================================================================
// HARDWARE-INDEXIERUNG (gemäß TSD)
// ============================================================================
uint16_t XY(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return 0;
  
  uint8_t correctedX = (MATRIX_WIDTH - 1) - x;

  if (y & 1) {
    return (y * MATRIX_WIDTH) + ((MATRIX_WIDTH - 1) - correctedX);
  } else {
    return (y * MATRIX_WIDTH) + correctedX;
  }
}

// ============================================================================
// BUTTON-HANDLING
// ============================================================================
struct Button {
  uint8_t pin;
  bool lastReading;
  bool state;
  unsigned long lastDebounceTime;
  bool wasPressed;
};

Button buttons[4] = {
  {BTN_UP,    HIGH, HIGH, 0, false},
  {BTN_DOWN,  HIGH, HIGH, 0, false},
  {BTN_LEFT,  HIGH, HIGH, 0, false},
  {BTN_RIGHT, HIGH, HIGH, 0, false}
};

const unsigned long DEBOUNCE_DELAY = 30; // 30ms Sperrzeit laut TSD

void updateButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    bool reading = digitalRead(buttons[i].pin);
    if (reading != buttons[i].lastReading) {
      buttons[i].lastDebounceTime = now;
    }
    if ((now - buttons[i].lastDebounceTime) > DEBOUNCE_DELAY) {
      if (reading != buttons[i].state) {
        buttons[i].state = reading;
        if (buttons[i].state == LOW) {
          buttons[i].wasPressed = true;
        }
      }
    }
    buttons[i].lastReading = reading;
  }
}

bool anyButtonPressed() {
  for (int i = 0; i < 4; i++) {
    if (buttons[i].wasPressed) return true;
  }
  return false;
}

void clearButtonPresses() {
  for (int i = 0; i < 4; i++) {
    buttons[i].wasPressed = false;
  }
}

// ============================================================================
// ZUFALLSGENERATOR
// ============================================================================
void seedRandomGenerator() {
  uint32_t seed = ESP.random();
  seed ^= micros();
  seed ^= ((uint32_t)analogRead(A0) << 16);
  randomSeed(seed);
}

// ============================================================================
// LABYRINTH-GENERATOR & BFS
// ============================================================================
int getShortestPath(int8_t sX, int8_t sY, int8_t eX, int8_t eY, CellType m[MATRIX_WIDTH][MATRIX_HEIGHT]) {
  int8_t dist[MATRIX_WIDTH][MATRIX_HEIGHT];
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      dist[x][y] = -1;
    }
  }

  uint8_t qX[256];
  uint8_t qY[256];
  uint8_t head = 0, tail = 0;

  dist[sX][sY] = 0;
  qX[head] = sX;
  qY[head] = sY;
  head++;

  const int8_t dx[] = { 0, 0, -1, 1 };
  const int8_t dy[] = { -1, 1, 0, 0 };

  while (head != tail) {
    uint8_t cx = qX[tail];
    uint8_t cy = qY[tail];
    tail++;

    if (cx == eX && cy == eY) {
      return dist[cx][cy];
    }

    for (int d = 0; d < 4; d++) {
      int nx = cx + dx[d];
      int ny = cy + dy[d];

      if (nx >= 0 && nx < MATRIX_WIDTH && ny >= 0 && ny < MATRIX_HEIGHT) {
        if (m[nx][ny] != CELL_WALL && m[nx][ny] != CELL_TIMER && dist[nx][ny] == -1) {
          dist[nx][ny] = dist[cx][cy] + 1;
          qX[head] = nx;
          qY[head] = ny;
          head++;
        }
      }
    }
  }
  return -1;
}

void generateMazeForLevel(uint16_t lvl) {
  int targetPathLen = 8 + (lvl * 2);
  if (targetPathLen > 55) targetPathLen = 55;

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      grid[x][y] = CELL_WALL;
    }
  }
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    grid[x][0] = CELL_TIMER;
  }

  startX = (random(0, 2) == 0) ? 7 : 8;
  startY = 0;

  uint8_t stX[128];
  uint8_t stY[128];
  int top = 0;

  int rootX = (startX % 2 != 0) ? startX : startX - 1;
  grid[rootX][1] = CELL_CORRIDOR;
  stX[top] = rootX;
  stY[top] = 1;

  grid[startX][1] = CELL_CORRIDOR;

  while (top >= 0) {
    if ((top & 0x0F) == 0) {
      yield();
      ESP.wdtFeed();
    }

    uint8_t cx = stX[top];
    uint8_t cy = stY[top];

    int8_t dirs[4][2] = {{0, -2}, {0, 2}, {-2, 0}, {2, 0}};
    for (int i = 0; i < 4; i++) {
      int r = random(i, 4);
      int8_t tx = dirs[i][0]; int8_t ty = dirs[i][1];
      dirs[i][0] = dirs[r][0]; dirs[i][1] = dirs[r][1];
      dirs[r][0] = tx; dirs[r][1] = ty;
    }

    bool found = false;
    for (int d = 0; d < 4; d++) {
      int nx = cx + dirs[d][0];
      int ny = cy + dirs[d][1];

      if (nx >= 1 && nx < MATRIX_WIDTH && ny >= 1 && ny < MATRIX_HEIGHT) {
        if (grid[nx][ny] == CELL_WALL) {
          grid[cx + (dirs[d][0] / 2)][cy + (dirs[d][1] / 2)] = CELL_CORRIDOR;
          grid[nx][ny] = CELL_CORRIDOR;
          top++;
          stX[top] = nx;
          stY[top] = ny;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      top--;
    }
  }

  int8_t dist[MATRIX_WIDTH][MATRIX_HEIGHT];
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      dist[x][y] = -1;
    }
  }

  uint8_t qX[256];
  uint8_t qY[256];
  uint8_t head = 0, tail = 0;

  dist[startX][1] = 1;
  qX[head] = startX;
  qY[head] = 1;
  head++;

  const int8_t dx[] = { 0, 0, -1, 1 };
  const int8_t dy[] = { -1, 1, 0, 0 };

  while (head != tail) {
    uint8_t cx = qX[tail];
    uint8_t cy = qY[tail];
    tail++;

    for (int d = 0; d < 4; d++) {
      int nx = cx + dx[d];
      int ny = cy + dy[d];

      if (nx >= 0 && nx < MATRIX_WIDTH && ny >= 1 && ny < MATRIX_HEIGHT) {
        if (grid[nx][ny] == CELL_CORRIDOR && dist[nx][ny] == -1) {
          dist[nx][ny] = dist[cx][cy] + 1;
          qX[head] = nx;
          qY[head] = ny;
          head++;
        }
      }
    }
  }

  int bestDiff = 999;
  int bestExX = -1;
  int bestExY = -1;

  for (int y = 2; y < MATRIX_HEIGHT; y++) {
    if (grid[1][y] == CELL_CORRIDOR && dist[1][y] > 0) {
      int dDiff = abs(dist[1][y] - targetPathLen);
      if (dDiff < bestDiff) {
        bestDiff = dDiff;
        bestExX = 0;
        bestExY = y;
      }
    }
    if (grid[14][y] == CELL_CORRIDOR && dist[14][y] > 0) {
      int dDiff = abs(dist[14][y] - targetPathLen);
      if (dDiff < bestDiff) {
        bestDiff = dDiff;
        bestExX = 15;
        bestExY = y;
      }
    }
  }

  for (int x = 1; x < MATRIX_WIDTH - 1; x++) {
    if (grid[x][14] == CELL_CORRIDOR && dist[x][14] > 0) {
      int dDiff = abs(dist[x][14] - targetPathLen);
      if (dDiff < bestDiff) {
        bestDiff = dDiff;
        bestExX = x;
        bestExY = 15;
      }
    }
  }

  if (bestExX == -1) {
    for (int y = 5; y < MATRIX_HEIGHT; y++) {
      for (int x = 0; x < MATRIX_WIDTH; x++) {
        if (grid[x][y] == CELL_CORRIDOR && dist[x][y] > 0) {
          int dDiff = abs(dist[x][y] - targetPathLen);
          if (dDiff < bestDiff) {
            bestDiff = dDiff;
            bestExX = x;
            bestExY = y;
          }
        }
      }
    }
  }

  exitX = bestExX;
  exitY = bestExY;
  grid[exitX][exitY] = CELL_EXIT;

  int loops = max(1, 4 - (lvl / 3));
  for (int i = 0; i < loops; i++) {
    int rx = random(2, 14);
    int ry = random(2, 14);
    if (grid[rx][ry] == CELL_WALL) {
      if ((grid[rx - 1][ry] == CELL_CORRIDOR && grid[rx + 1][ry] == CELL_CORRIDOR) ||
          (grid[rx][ry - 1] == CELL_CORRIDOR && grid[rx][ry + 1] == CELL_CORRIDOR)) {
        grid[rx][ry] = CELL_CORRIDOR;
      }
    }
  }

  playerX = startX;
  playerY = startY;
  playerHasLeftStart = false;
}

// Generiert ein volles 16x16 Labyrinth rein für Menü-/Score-Hintergründe
void generateDecorativeMaze() {
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      grid[x][y] = CELL_WALL;
    }
  }

  uint8_t stX[128];
  uint8_t stY[128];
  int top = 0;

  grid[1][1] = CELL_CORRIDOR;
  stX[top] = 1;
  stY[top] = 1;

  while (top >= 0) {
    if ((top & 0x0F) == 0) {
      yield();
      ESP.wdtFeed();
    }

    uint8_t cx = stX[top];
    uint8_t cy = stY[top];

    int8_t dirs[4][2] = {{0, -2}, {0, 2}, {-2, 0}, {2, 0}};
    for (int i = 0; i < 4; i++) {
      int r = random(i, 4);
      int8_t tx = dirs[i][0]; int8_t ty = dirs[i][1];
      dirs[i][0] = dirs[r][0]; dirs[i][1] = dirs[r][1];
      dirs[r][0] = tx; dirs[r][1] = ty;
    }

    bool found = false;
    for (int d = 0; d < 4; d++) {
      int nx = cx + dirs[d][0];
      int ny = cy + dirs[d][1];

      if (nx >= 1 && nx < MATRIX_WIDTH && ny >= 1 && ny < MATRIX_HEIGHT) {
        if (grid[nx][ny] == CELL_WALL) {
          grid[cx + (dirs[d][0] / 2)][cy + (dirs[d][1] / 2)] = CELL_CORRIDOR;
          grid[nx][ny] = CELL_CORRIDOR;
          top++;
          stX[top] = nx;
          stY[top] = ny;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      top--;
    }
  }
}

// ============================================================================
// DREHUNG DES SPIELFELDS
// ============================================================================
void rotateGrid(uint8_t steps) {
  for (int s = 0; s < steps; s++) {
    CellType temp[MATRIX_WIDTH][MATRIX_HEIGHT];
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      for (int x = 0; x < MATRIX_WIDTH; x++) {
        temp[15 - y][x] = grid[x][y];
      }
    }
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      for (int x = 0; x < MATRIX_WIDTH; x++) {
        grid[x][y] = temp[x][y];
      }
    }

    int8_t oldPX = playerX;
    int8_t oldPY = playerY;
    playerX = 15 - oldPY;
    playerY = oldPX;

    int8_t oldSX = startX;
    int8_t oldSY = startY;
    startX = 15 - oldSY;
    startY = oldSX;

    int8_t oldEX = exitX;
    int8_t oldEY = exitY;
    exitX = 15 - oldEY;
    exitY = oldEX;
  }
}

// ============================================================================
// MINISCHRIFT RENDERER (3x5)
// ============================================================================
void drawChar3x5(char c, int startX, int startY, CRGB color) {
  uint8_t glyphIndex = 0;
  if (c >= '0' && c <= '9') glyphIndex = c - '0';
  else if (c == '-') glyphIndex = 10;
  else if (c == 'M') glyphIndex = 11;
  else if (c == 'A') glyphIndex = 12;
  else if (c == 'Z') glyphIndex = 13;
  else if (c == 'E') glyphIndex = 14;
  else if (c == 'R') glyphIndex = 15;
  else if (c == 'C') glyphIndex = 16;
  else return;

  for (int col = 0; col < 3; col++) {
    uint8_t colBits = pgm_read_byte(&(FONT_3X5[glyphIndex][col]));
    for (int row = 0; row < 5; row++) {
      if (colBits & (1 << row)) {
        int x = startX + col;
        int y = startY + row;
        if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
          leds[XY(x, y)] = color;
        }
      }
    }
  }
}

// Rendert Zeichen mit diagonalem Verlauf (Gelb -> Wand-Blau) und Helligkeits-Fading
void drawChar3x5Gradient(char c, int startX, int startY, uint8_t alpha) {
  if (alpha == 0) return;

  uint8_t glyphIndex = 0;
  if (c >= '0' && c <= '9') glyphIndex = c - '0';
  else if (c == '-') glyphIndex = 10;
  else if (c == 'M') glyphIndex = 11;
  else if (c == 'A') glyphIndex = 12;
  else if (c == 'Z') glyphIndex = 13;
  else if (c == 'E') glyphIndex = 14;
  else if (c == 'R') glyphIndex = 15;
  else if (c == 'C') glyphIndex = 16;
  else return;

  for (int col = 0; col < 3; col++) {
    uint8_t colBits = pgm_read_byte(&(FONT_3X5[glyphIndex][col]));
    for (int row = 0; row < 5; row++) {
      if (colBits & (1 << row)) {
        int x = startX + col;
        int y = startY + row;
        if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
          // Diagonale Position (0 bis 30)
          uint8_t diagFrac = ((x + y) * 255) / 30;
          CRGB gradColor = blend(COLOR_PLAYER, COLOR_WALL, diagFrac);
          gradColor.nscale8_video(alpha);

          // Subtraktives/Additives Mischen über das Hintergrund-Labyrinth
          leds[XY(x, y)] = blend(leds[XY(x, y)], gradColor, alpha);
        }
      }
    }
  }
}

// ============================================================================
// RENDERING
// ============================================================================
void renderGame(uint8_t brightnessFactor, bool flashWallsWhite) {
  CRGB wColor = flashWallsWhite ? COLOR_WALL_CRASH : COLOR_WALL;

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      CRGB color;
      switch (grid[x][y]) {
        case CELL_WALL:
          color = wColor;
          break;
        case CELL_EXIT:
          color = COLOR_EXIT;
          break;
        default:
          color = COLOR_BG;
          break;
      }
      leds[XY(x, y)] = color;
    }
  }

  // Timer-Linie rendern
  float timeFraction = 1.0f - ((float)levelElapsedMs / (float)levelTotalDuration);
  if (timeFraction < 0.0f) timeFraction = 0.0f;
  if (timeFraction > 1.0f) timeFraction = 1.0f;
  float activeSegments = timeFraction * 8.0f;

  int timerPixelIndex = 0;
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      if (grid[x][y] == CELL_TIMER) {
        if (x == playerX && y == playerY && !playerHasLeftStart) {
          timerPixelIndex++;
          continue;
        }

        int distFromCenter = timerPixelIndex;
        if (distFromCenter >= 8) distFromCenter = 15 - distFromCenter;
        distFromCenter = 7 - distFromCenter;

        CRGB tColor;
        if ((float)distFromCenter + 1.0f <= activeSegments) {
          tColor = COLOR_TIMER_FULL;
        } else if ((float)distFromCenter < activeSegments) {
          float subFade = activeSegments - (float)distFromCenter;
          tColor = blend(wColor, COLOR_TIMER_FULL, (uint8_t)(subFade * 255));
        } else {
          tColor = wColor;
        }

        leds[XY(x, y)] = tColor;
        timerPixelIndex++;
      }
    }
  }

  // Ausgang pulsieren lassen
  uint8_t exitPulse = beatsin8(120, 180, 255);
  leds[XY(exitX, exitY)] = COLOR_EXIT;
  leds[XY(exitX, exitY)].nscale8_video(exitPulse);

  // Spielfigur
  uint8_t pulse = beatsin8((uint16_t)(PULSE_FREQ_HZ * 60.0f), PULSE_BRIGHT_MIN, PULSE_BRIGHT_MAX);
  CRGB pColor = COLOR_PLAYER;
  pColor.nscale8_video(pulse);
  leds[XY(playerX, playerY)] = pColor;

  if (brightnessFactor < 255) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].nscale8_video(brightnessFactor);
    }
  }
}

// ============================================================================
// LEVEL START
// ============================================================================
void startLevel(uint16_t lvl) {
  currentLevel = lvl;

  levelTotalDuration = INITIAL_TIME_MS;
  if ((unsigned long)(lvl - 1) * TIME_REDUCTION_MS < INITIAL_TIME_MS - MIN_TIME_LIMIT_MS) {
    levelTotalDuration -= (lvl - 1) * TIME_REDUCTION_MS;
  } else {
    levelTotalDuration = MIN_TIME_LIMIT_MS;
  }

  nextRotateCountdown = INITIAL_ROTATE_INTERVAL_MS;
  if ((unsigned long)(lvl - 1) * ROTATE_REDUCTION_MS < INITIAL_ROTATE_INTERVAL_MS - MIN_ROTATE_INTERVAL_MS) {
    nextRotateCountdown -= (lvl - 1) * ROTATE_REDUCTION_MS;
  } else {
    nextRotateCountdown = MIN_ROTATE_INTERVAL_MS;
  }

  generateMazeForLevel(lvl);

  uint8_t initialRotation = random(0, 4);
  if (initialRotation > 0) {
    rotateGrid(initialRotation);
  }

  levelElapsedMs = 0;
  levelStartTime = millis();
  currentState = STATE_PLAYING;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  pinMode(BTN_UP,    INPUT_PULLUP);
  pinMode(BTN_DOWN,  INPUT_PULLUP);
  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 900);
  FastLED.setBrightness(51); // 20% Helligkeit gemäß TSD

  // oLb Startanimation
  olb_v1_playIntro(leds);
  // 1 Sekunde Pause nach olb_init

  FastLED.clear();
  FastLED.show();
  delay(1000);

  seedRandomGenerator();
  generateDecorativeMaze();

  currentState = STATE_INTRO;
  stateEnterTime = millis();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  yield();
  ESP.wdtFeed();
  updateButtons();
  unsigned long now = millis();

  switch (currentState) {
    case STATE_INTRO: {
      // 1. Hintergrund zeichnen: Labyrinth-Wände in COLOR_BG, Wege schwarz
      for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
          leds[XY(x, y)] = (grid[x][y] == CELL_WALL) ? COLOR_BG : CRGB::Black;
        }
      }

      // 2. 1 Sekunde stehen lassen, danach abblenden bis auf Schwarz und wieder aufblenden
      unsigned long cycle = (now - stateEnterTime) % 2000UL; // 3-Sekunden-Zyklus
      uint8_t alpha = 255;
      if (cycle >= 1000UL) {
        // 1000ms bis 3000ms: Cosinus-Welle von 255 nach 0 und zurück nach 255
        alpha = (cos8((uint8_t)(((cycle - 1000UL) * 256UL) / 1000UL)) + 1) / 2; // sanfte Kurve 255 -> 0 -> 255
      }

      // 3. Titel mit Farbverlauf Gelb -> Blau rendern
      drawChar3x5Gradient('M', 1, 2, alpha);
      drawChar3x5Gradient('A', 5, 2, alpha);
      drawChar3x5Gradient('Z', 9, 2, alpha);
      drawChar3x5Gradient('E', 13, 2, alpha);

      drawChar3x5Gradient('R', 1, 9, alpha);
      drawChar3x5Gradient('A', 5, 9, alpha);
      drawChar3x5Gradient('C', 9, 9, alpha);
      drawChar3x5Gradient('E', 13, 9, alpha);

      FastLED.show();

      if (anyButtonPressed()) {
        seedRandomGenerator();
        clearButtonPresses();
        startLevel(1);
      }
      break;
    }

    case STATE_PLAYING: {
      levelElapsedMs = now - levelStartTime;
      if (levelElapsedMs >= levelTotalDuration) {
        stateEnterTime = now;
        currentState = STATE_GAME_OVER;
        break;
      }

      if (levelElapsedMs >= nextRotateCountdown) {
        rotationAngle = random(1, 4);
        rotationPhase = 0;
        stateEnterTime = now;
        clearButtonPresses();
        currentState = STATE_ROTATING;
        break;
      }

      int8_t moveX = 0;
      int8_t moveY = 0;

      if (buttons[0].wasPressed) { moveY = -1; }      // BTN_UP
      else if (buttons[1].wasPressed) { moveY = 1; }  // BTN_DOWN
      else if (buttons[2].wasPressed) { moveX = -1; } // BTN_LEFT
      else if (buttons[3].wasPressed) { moveX = 1; }  // BTN_RIGHT
      clearButtonPresses();

      if (moveX != 0 || moveY != 0) {
        int8_t targetX = playerX + moveX;
        int8_t targetY = playerY + moveY;

        if (targetX < 0 || targetX >= MATRIX_WIDTH || targetY < 0 || targetY >= MATRIX_HEIGHT) {
          stateEnterTime = now;
          currentState = STATE_CRASH_FREEZE;
          break;
        }

        CellType targetCell = grid[targetX][targetY];

        if (targetCell == CELL_WALL || targetCell == CELL_TIMER) {
          stateEnterTime = now;
          currentState = STATE_CRASH_FREEZE;
          break;
        }

        playerX = targetX;
        playerY = targetY;
        playerHasLeftStart = true;

        if (playerX == exitX && playerY == exitY) {
          stateEnterTime = now;
          currentState = STATE_LEVEL_CLEAR;
          break;
        }
      }

      renderGame(255, false);
      FastLED.show();
      break;
    }

    case STATE_CRASH_FREEZE: {
      clearButtonPresses();
      unsigned long freezeElapsed = now - stateEnterTime;

      if (freezeElapsed < CRASH_FLASH_MS) {
        renderGame(255, true);
        FastLED.show();
      } else if (freezeElapsed < CRASH_TOTAL_MS) {
        FastLED.clear();
        FastLED.show();
      } else {
        levelStartTime += CRASH_TOTAL_MS;

        unsigned long currentElapsed = now - levelStartTime;
        if (nextRotateCountdown <= currentElapsed + 3000UL) {
          nextRotateCountdown = currentElapsed + 3000UL;
        }

        playerX = startX;
        playerY = startY;
        playerHasLeftStart = false;
        clearButtonPresses();

        renderGame(255, false);
        FastLED.show();

        currentState = STATE_PLAYING;
      }
      break;
    }

    case STATE_ROTATING: {
      clearButtonPresses();
      unsigned long rotElapsed = now - stateEnterTime;

      if (rotationPhase == 0) {
        if (rotElapsed < ROTATE_FADE_OUT_MS) {
          uint8_t factor = 255 - (uint8_t)((rotElapsed * 255) / ROTATE_FADE_OUT_MS);
          renderGame(factor, false);
          FastLED.show();
        } else {
          rotateGrid(rotationAngle);
          rotationPhase = 1;
          stateEnterTime = now;
        }
      } else {
        if (rotElapsed < ROTATE_FADE_IN_MS) {
          uint8_t factor = (uint8_t)((rotElapsed * 255) / ROTATE_FADE_IN_MS);
          renderGame(factor, false);
          FastLED.show();
        } else {
          levelStartTime += (ROTATE_FADE_OUT_MS + ROTATE_FADE_IN_MS);
          nextRotateCountdown = (now - levelStartTime) + INITIAL_ROTATE_INTERVAL_MS;
          clearButtonPresses();
          currentState = STATE_PLAYING;
        }
      }
      break;
    }

    case STATE_LEVEL_CLEAR: {
      clearButtonPresses();
      unsigned long animElapsed = now - stateEnterTime;
      const unsigned long ANIM_DURATION = 300UL;

      if (animElapsed < ANIM_DURATION) {
        FastLED.clear();

        // 1. Zuerst das Spielfeld als Basis rendern (Wände, Timer etc.)
        renderGame(255, false);

        // Fortschritt: 0.0f (Start) bis 1.0f (vollständig aufgesaugt)
        float progress = (float)animElapsed / (float)ANIM_DURATION;
        
        // Kollaps-Horizont zieht sich von außen (max Distanz ~32) zum Ausgang zusammen
        int collapseRadius = (int)((1.0f - progress) * 28.0f);

        // 2. Pixel außerhalb des aktuellen Horizonts verschlucken (schwarz)
        // Pixel kurz vor dem Verschlucken werden zum Ausgang hin heller/cyan verzerrt
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
          for (int x = 0; x < MATRIX_WIDTH; x++) {
            int dist = abs(x - exitX) + abs(y - exitY); // Manhattan-Distanz zum Ausgang

            if (dist > collapseRadius) {
              leds[XY(x, y)] = CRGB::Black; // Bereits aufgesaugt
            } else if (dist == collapseRadius && collapseRadius > 0) {
              // Der "Ereignishorizont": Rand leuchtet beim Ansaugen hell in Exit-Farbe
              leds[XY(x, y)] = blend(leds[XY(x, y)], COLOR_EXIT, 180);
            }
          }
        }

        // 3. Strudel-/Ansaug-Effekt: Gelegentliche Energiefunken, die in den Ausgang rasen
        for (int i = 0; i < 4; i++) {
          if (collapseRadius > 1) {
            float sparkDist = (float)collapseRadius * ((float)random8(30, 95) / 100.0f);
            int sx = exitX + (int)((random8(2) == 0 ? 1 : -1) * (sparkDist * 0.7f));
            int sy = exitY + (int)((random8(2) == 0 ? 1 : -1) * (sparkDist * 0.7f));
            if (sx >= 0 && sx < MATRIX_WIDTH && sy >= 0 && sy < MATRIX_HEIGHT) {
              leds[XY(sx, sy)] = CRGB(200, 255, 255);
            }
          }
        }

        // 4. Der Ausgangspunkt selbst glüht intensiv als Gravitationszentrum
        uint8_t coreBrightness = (progress < 0.85f) ? 255 : (uint8_t)((1.0f - progress) * 255 / 0.15f);
        CRGB coreColor = CRGB(255, 255, 255);
        coreColor.nscale8_video(coreBrightness);
        leds[XY(exitX, exitY)] = coreColor;

        FastLED.show();
      } else {
        generateDecorativeMaze();
        stateEnterTime = now;
        currentState = STATE_SCORE_DISPLAY;
      }
      break;
    }

    case STATE_GAME_OVER: {
      clearButtonPresses();
      unsigned long elapsed = now - stateEnterTime;
      const unsigned long BURN_DURATION = 1000UL;

      if (elapsed < BURN_DURATION) {
        FastLED.clear();

        // 1. Spielfeld als Basis ins leds[] rendern (ohne Spieler)
        renderGame(255, false);

        // Fortschritt: 0 (Start) bis 255 (Ende)
        uint8_t progress = (uint8_t)((elapsed * 255UL) / BURN_DURATION);

        // 2. Glutfarbe für die Wände berechnen
        CRGB wallFlameColor;
        if (progress < 40) {
          uint8_t heat = map(progress, 0, 39, 0, 255);
          wallFlameColor = blend(COLOR_WALL, CRGB(255, 230, 150), heat);
        } else if (progress < 120) {
          uint8_t heat = map(progress, 40, 119, 0, 255);
          wallFlameColor = blend(CRGB(255, 230, 150), CRGB(255, 60, 0), heat);
        } else if (progress < 200) {
          uint8_t heat = map(progress, 120, 199, 0, 255);
          wallFlameColor = blend(CRGB(255, 60, 0), CRGB(100, 0, 0), heat);
        } else {
          uint8_t heat = map(progress, 200, 254, 0, 255);
          wallFlameColor = blend(CRGB(100, 0, 0), CRGB::Black, heat);
        }

        // 3. Wände glühen lassen, Wege nach Schwarz faden
        for (int i = 0; i < NUM_LEDS; i++) {
          if (leds[i] == COLOR_WALL) {
            leds[i] = wallFlameColor; // Nur Wände glühen
          } else if (leds[i]) {
            leds[i].nscale8_video(255 - progress); // Wege/Rest faden dunkel
          }
        }

        FastLED.show();
      } else {
        generateDecorativeMaze();
        stateEnterTime = now;
        currentState = STATE_SCORE_DISPLAY;
      }
      break;
    }

    case STATE_SCORE_DISPLAY: {
      // 1. Hintergrund zeichnen: Labyrinth-Wände in COLOR_BG
      for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
          leds[XY(x, y)] = (grid[x][y] == CELL_WALL) ? COLOR_BG : CRGB::Black;
        }
      }

      // 2. Faden bis auf Schwarz (0) und zurück
      // 2. 1 Sekunde stehen lassen, danach abblenden bis auf Schwarz und wieder aufblenden
      unsigned long cycle = (now - stateEnterTime) % 2000UL; // 3-Sekunden-Zyklus
      uint8_t alpha = 255;
      if (cycle >= 1000UL) {
        alpha = (cos8((uint8_t)(((cycle - 1000UL) * 256UL) / 1000UL)) + 1) / 2;
      }

      char str[6];
      snprintf(str, sizeof(str), "%d", currentLevel);

      int len = strlen(str);
      int totalWidth = (len * 3) + (len - 1);
      int startOffset = (MATRIX_WIDTH - totalWidth) / 2;

      // 3. Level-Zahl mit Farbverlauf Gelb -> Blau rendern
      for (int i = 0; i < len; i++) {
        drawChar3x5Gradient(str[i], startOffset + (i * 4), 6, alpha);
      }

      FastLED.show();

      if ((now - stateEnterTime > 500) && anyButtonPressed()) {
        clearButtonPresses();
        if (levelElapsedMs < levelTotalDuration) {
          startLevel(currentLevel + 1);
        } else {
          generateDecorativeMaze();
          currentState = STATE_INTRO;
        }
      }
      break;
    }
  }

  delay(5);
}