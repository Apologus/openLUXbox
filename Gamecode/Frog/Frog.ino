// NodeMCU 1.0 (ESP-12E Module)
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include "olb_init_v1.h"

// ── Defines ──────────────────────────────────────────────
#define LED_PIN        D4
#define NUM_LEDS       256
#define MATRIX_W       16
#define MATRIX_H       16
#define BRIGHTNESS     51

#define BTN_LEFT       D2
#define BTN_RIGHT      D5
#define BTN_DOWN       D1
#define BTN_UP         D6

#define DEBOUNCE_MS    30

#define ROW_GOAL       0
#define ROW_WATER_TOP  1
#define ROW_WATER_BOT  5
#define ROW_SAFE_MID   6
#define ROW_ROAD_TOP   7
#define ROW_ROAD_BOT   13
#define ROW_START      15

#define LOG_LEN        4
#define CAR_LEN        2

// Mindest- und Maximalabstand zwischen den 2 Autos (in Pixel)
#define CAR_GAP_MIN    2
#define CAR_GAP_MAX    7

#define BASE_INTERVAL  600
#define MIN_INTERVAL   80

#define TOTAL_LANES    12
#define NUM_GOALS      4

#define SPEED_EVERY_FLIES      1
#define SPEED_PERCENT_PER_STEP 10

// ── Glühwürmchen-Defines ─────────────────────────────────
#define MAX_FIREFLIES        3
#define FIREFLY_BASE_NEEDED  2   // Level 1: 2 nötig, Level 2: 3 nötig, usw.
#define FIREFLY_SPAWN_CHANCE 25 // % Chance pro Sekunde auf neuen Spawn-Versuch
#define FIREFLY_PULSE_SPEED  6  // ms-Teiler für Pulsieren

#define COL_FROG       CRGB(0,255,0)
#define COL_CAR        CRGB(255,50,0)
#define COL_LOG        CRGB(139,69,19)
#define COL_WATER      CRGB(0,0,180)
#define COL_ROAD       CRGB(60,60,60)
#define COL_SAFE       CRGB(0,80,0)
#define COL_GOAL       CRGB(255,215,0)
#define COL_GOAL_REACH CRGB(255,255,0)
#define COL_FLY        CRGB(255,100,255)
#define COL_FIREFLY    CRGB(255,255,120)

// ── Titelbild 16x16 (0xRRGGBB) ───────────────────────────
const uint32_t titleBitmap[16][16] = {
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0xFFFFFF, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0xFF64FF },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x14C828, 0x000A19, 0x14C828, 0x000A19, 0x000A19, 0xFF3C3C, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x14C828, 0xFFFFFF, 0x14C828, 0xFFFFFF, 0x14C828, 0xFF3C3C, 0x000A19, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x14C828, 0x000000, 0x14C828, 0x000000, 0x14C828, 0x000A19, 0x000A19, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x14C828, 0x14C828, 0x0A7814, 0x0A7814, 0x14C828, 0x000A19, 0x000A19, 0x000A19 },
  { 0x000A19, 0x000A19, 0x000A19, 0x000A19, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x000A19, 0x000A19, 0x000A19, 0x000A19 },
  { 0x000A19, 0x006400, 0x000A19, 0x000A19, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x000A19, 0x000A19, 0x006400, 0x000A19 },
  { 0x000A19, 0x006400, 0x000A19, 0x000A19, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x14C828, 0x000A19, 0x000A19, 0x006400, 0x000A19 },
  { 0x003C00, 0x003C00, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x0A7814, 0x003C00, 0x003C00 },
  { 0x003C00, 0x003C00, 0x0A7814, 0x0A7814, 0x003C00, 0x0A7814, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x0A7814, 0x003C00, 0x0A7814, 0x0A7814, 0x003C00, 0x003C00 },
  { 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00 },
  { 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00, 0x003C00 }
};

// ── Structs ──────────────────────────────────────────────
struct Lane {
  int  row;
  int  pos;   // Auto 1 (Logs: einzige Position)
  int  pos2;  // Auto 2 (nur für Autos genutzt)
  int  len;
  int  dir;
  bool isLog;
  bool hasSecondCar; // ob Auto 2 aktiv ist
};

struct Button {
  uint8_t       pin;
  bool          lastState;
  unsigned long lastTime;
  bool          pressed;
};

struct Firefly {
  bool active;
  bool onLog;     // true = sitzt auf einem Log (driftet mit), false = auf der Straße (fest)
  int  laneIndex; // Index in lanes[] (Zeile)
  int  x;         // aktuelle X-Position (nur bei Straßen-Glühwürmchen relevant/fest)
  int  offsetInLog; // Position relativ zum Log-Start (bei onLog == true)
};

// ── Prototypen ───────────────────────────────────────────
void initGame();
void initGoals();
void initLanes();
void nextRound();
void updateSpeed();
void showStartScreen();
void showGameOver();
void handleButtons();
bool btnJustPressed(Button &b);
void updateLanes();
void checkCollision();
void loseLife();
void drawFrame();
void drawHUD();
void setPixel(int x, int y, CRGB color);
int  xyToIndex(int x, int y);
bool frogOnLog(int &outDrift);
bool frogHitCar();
uint32_t getTrueRandom();

// Firefly-Prototypen
void initFireflies();
void updateFireflies();
void trySpawnFirefly();
void drawFireflies();
void checkFireflyCollision();
int  firefliesNeeded();
int  firefliesActiveCount();

// ── Globale Variablen ────────────────────────────────────
CRGB leds[NUM_LEDS];

Button btnL = {BTN_LEFT,  false, 0, false};
Button btnR = {BTN_RIGHT, false, 0, false};
Button btnD = {BTN_DOWN,  false, 0, false};
Button btnU = {BTN_UP,    false, 0, false};

int  frogX, frogY;
int  level;
int  lives;
int  totalFliesCaught;
bool waitForRestart;
bool gameStarted;

unsigned long lastMoveTime;
unsigned long moveInterval;

unsigned long gameOverTime = 0;

// ── Zielpunkte ───────────────────────────────────────────
const int goalX[NUM_GOALS] = {2, 6, 10, 14};
bool      goalReached[NUM_GOALS];
int       goalsLeft;

// ── Glühwürmchen-Variablen ───────────────────────────────
Firefly  fireflies[MAX_FIREFLIES];
int      firefliesCollectedThisLevel;
unsigned long lastFireflySpawnCheck;
bool     goalsUnlocked;

// ── Lane-Konfiguration ───────────────────────────────────
const int  laneRows[TOTAL_LANES]  = { 1,  2,  3,  4,  5,
                                       7,  8,  9, 10, 11, 12, 13};
const int  laneDirs[TOTAL_LANES]  = { 1, -1,  1, -1,  1,
                                      -1,  1, -1,  1, -1,  1, -1};
const bool laneIsLog[TOTAL_LANES] = {true,  true,  true,  true,  true,
                                     false, false, false, false, false, false, false};
const int  laneLen[TOTAL_LANES]   = {LOG_LEN, LOG_LEN, LOG_LEN, LOG_LEN, LOG_LEN,
                                     CAR_LEN, CAR_LEN, CAR_LEN, CAR_LEN, CAR_LEN, CAR_LEN, CAR_LEN};

Lane lanes[TOTAL_LANES];

// ── Echter Zufallsgenerator ───────────────────────────────
uint32_t getTrueRandom() {
  uint32_t seed = 0;
  for (int i = 0; i < 32; i++) {
    seed = (seed << 1) | (analogRead(A0) & 1);
    delayMicroseconds(50);
  }
  return seed;
}

// ── Tempo berechnen ──────────────────────────────────────
void updateSpeed() {
  int steps = totalFliesCaught / SPEED_EVERY_FLIES;
  float interval = BASE_INTERVAL;
  float factor   = 1.0f - (SPEED_PERCENT_PER_STEP / 100.0f);
  for (int s = 0; s < steps; s++) interval *= factor;
  moveInterval = (unsigned long)max((float)MIN_INTERVAL, interval);

  Serial.print("Tempo: Fliegen="); Serial.print(totalFliesCaught);
  Serial.print("  Intervall=");    Serial.println(moveInterval);
}

// ── Lane-Init (mit zufälligem Auto-Abstand, jede 2. Fahrspur nur 1 Auto) ──
void initLanes() {
  int roadLaneCounter = 0;

  for (int i = 0; i < TOTAL_LANES; i++) {
    lanes[i].row   = laneRows[i];
    lanes[i].dir   = laneDirs[i];
    lanes[i].isLog = laneIsLog[i];
    lanes[i].len   = laneLen[i];
    lanes[i].pos   = random(0, MATRIX_W);

    if (!laneIsLog[i]) {
      bool secondCar = (roadLaneCounter % 2 == 0);
      lanes[i].hasSecondCar = secondCar;
      if (secondCar) {
        int gap = random(CAR_GAP_MIN, CAR_GAP_MAX + 1);
        lanes[i].pos2 = ((lanes[i].pos - (CAR_LEN + gap)) % MATRIX_W
                          + MATRIX_W) % MATRIX_W;
      } else {
        lanes[i].pos2 = -1000;
      }
      roadLaneCounter++;
    } else {
      lanes[i].pos2 = 0;
      lanes[i].hasSecondCar = false;
    }
  }
}

// ── setup() ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("openLUXbox Frogger startet...");

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN,  INPUT_PULLUP);
  pinMode(BTN_UP,    INPUT_PULLUP);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);

  // Startanimation
  olb_v1_playIntro(leds);

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  randomSeed(getTrueRandom());

  gameStarted    = false;
  waitForRestart = false;

  showStartScreen();
}

// ── loop() ───────────────────────────────────────────────
void loop() {
  handleButtons();

  if (!gameStarted) {
    if (btnL.pressed || btnR.pressed || btnU.pressed || btnD.pressed) {
      gameStarted = true;
      initGame();
    }
    return;
  }

  if (waitForRestart) {
    if (btnL.pressed || btnR.pressed || btnU.pressed || btnD.pressed) {
      gameStarted    = false;
      waitForRestart = false;
      showStartScreen();
    }
    return;
  }

  // Frosch bewegen
  if (btnU.pressed && frogY > ROW_GOAL)    frogY--;
  if (btnD.pressed && frogY < ROW_START)   frogY++;
  if (btnL.pressed && frogX > 0)           frogX--;
  if (btnR.pressed && frogX < MATRIX_W-1)  frogX++;

  // Takt: Spuren + Log-Drift
  unsigned long now = millis();
  if (now - lastMoveTime >= moveInterval) {
    lastMoveTime = now;

    if (frogY >= ROW_WATER_TOP && frogY <= ROW_WATER_BOT) {
      int drift = 0;
      if (frogOnLog(drift))
        frogX = constrain(frogX + drift, 0, MATRIX_W - 1);
    }

    updateLanes();
    updateFireflies();
  }

  // Zufälliger Spawn-Versuch für Glühwürmchen (einmal pro Sekunde geprüft)
  if (now - lastFireflySpawnCheck >= 1000) {
    lastFireflySpawnCheck = now;
    trySpawnFirefly();
  }

  checkCollision();
  if (waitForRestart) return;

  checkFireflyCollision();
  drawFrame();
  FastLED.show();
  delay(16);
}

// ── initGame() ───────────────────────────────────────────
void initGame() {
  level            = 1;
  lives            = 3;
  totalFliesCaught = 0;
  waitForRestart   = false;
  moveInterval     = BASE_INTERVAL;
  lastMoveTime     = millis();
  frogX            = MATRIX_W / 2;
  frogY            = ROW_START;

  initLanes();
  initGoals();
  initFireflies();
  Serial.print("Level "); Serial.println(level);
}

// ── initGoals() ──────────────────────────────────────────
void initGoals() {
  goalsLeft = NUM_GOALS;
  for (int i = 0; i < NUM_GOALS; i++)
    goalReached[i] = false;
}

// ── nextRound() ──────────────────────────────────────────
void nextRound() {
  level++;
  initLanes();
  initGoals();
  initFireflies();

  frogX = MATRIX_W / 2;
  frogY = ROW_START;

  fill_solid(leds, NUM_LEDS, COL_GOAL_REACH);
  FastLED.show();
  delay(600);

  Serial.print("Runde! Level "); Serial.print(level);
  Serial.print("  Intervall=");  Serial.println(moveInterval);
}

// ── Button-Handling ──────────────────────────────────────
void handleButtons() {
  btnL.pressed = btnJustPressed(btnL);
  btnR.pressed = btnJustPressed(btnR);
  btnU.pressed = btnJustPressed(btnU);
  btnD.pressed = btnJustPressed(btnD);
}

bool btnJustPressed(Button &b) {
  bool          current = (digitalRead(b.pin) == LOW);
  unsigned long now     = millis();
  if (current && !b.lastState && (now - b.lastTime >= DEBOUNCE_MS)) {
    b.lastState = true;
    b.lastTime  = now;
    return true;
  }
  if (!current) b.lastState = false;
  return false;
}

// ── Spuren bewegen ───────────────────────────────────────
void updateLanes() {
  for (int i = 0; i < TOTAL_LANES; i++) {
    lanes[i].pos += lanes[i].dir;
    if (lanes[i].pos >= MATRIX_W) lanes[i].pos = 0;
    if (lanes[i].pos < 0)         lanes[i].pos = MATRIX_W - 1;

    if (!lanes[i].isLog && lanes[i].hasSecondCar) {
      lanes[i].pos2 += lanes[i].dir;
      if (lanes[i].pos2 >= MATRIX_W) lanes[i].pos2 = 0;
      if (lanes[i].pos2 < 0)         lanes[i].pos2 = MATRIX_W - 1;
    }
  }
}

// ── Log-Check ────────────────────────────────────────────
bool frogOnLog(int &outDrift) {
  for (int i = 0; i < TOTAL_LANES; i++) {
    if (!lanes[i].isLog)       continue;
    if (lanes[i].row != frogY) continue;
    for (int l = 0; l < lanes[i].len; l++) {
      if ((lanes[i].pos + l) % MATRIX_W == frogX) {
        outDrift = lanes[i].dir;
        return true;
      }
    }
  }
  outDrift = 0;
  return false;
}

// ── Auto-Treffer-Check (Hilfsfunktion) ───────────────────
bool frogHitCar() {
  for (int i = 0; i < TOTAL_LANES; i++) {
    if (lanes[i].isLog)        continue;
    if (lanes[i].row != frogY) continue;
    for (int l = 0; l < lanes[i].len; l++) {
      if ((lanes[i].pos  + l) % MATRIX_W == frogX) return true;
      if (lanes[i].hasSecondCar && (lanes[i].pos2 + l) % MATRIX_W == frogX) return true;
    }
  }
  return false;
}

// ── Kollision ────────────────────────────────────────────
void checkCollision() {

  // Zielzeile
  if (frogY == ROW_GOAL) {
    if (!goalsUnlocked) {
      frogY = ROW_WATER_TOP;
      return;
    }

    for (int i = 0; i < NUM_GOALS; i++) {
      if (!goalReached[i] && frogX == goalX[i]) {
        goalReached[i] = true;
        goalsLeft--;
        totalFliesCaught++;
        updateSpeed();

        Serial.print("Fliege #"); Serial.print(totalFliesCaught);
        Serial.print(" – noch "); Serial.print(goalsLeft);
        Serial.println(" offen");

        if (goalsLeft <= 0) {
          nextRound();
        } else {
          goalsUnlocked = false;
          firefliesCollectedThisLevel = 0;
          initFireflies();
          frogX = MATRIX_W / 2;
          frogY = ROW_START;
        }
        return;
      }
    }
    frogY = ROW_WATER_TOP;
    return;
  }

  // Wasserzone
  if (frogY >= ROW_WATER_TOP && frogY <= ROW_WATER_BOT) {
    int drift = 0;
    if (!frogOnLog(drift)) loseLife();
    return;
  }

  // Straße
  if (frogY >= ROW_ROAD_TOP && frogY <= ROW_ROAD_BOT) {
    if (frogHitCar()) loseLife();
  }
}

// ── Leben verlieren ──────────────────────────────────────
void loseLife() {
  lives--;
  Serial.print("Leben übrig: "); Serial.println(lives);

  for (int i = 0; i < 3; i++) {
    fill_solid(leds, NUM_LEDS, CRGB(180, 0, 0));
    FastLED.show(); delay(150);
    FastLED.clear();
    FastLED.show(); delay(150);
  }

  if (lives <= 0) {
    showGameOver();
    gameOverTime   = millis();
    waitForRestart = true;
  } else {
    frogX = MATRIX_W / 2;
    frogY = ROW_START;
  }
}

// ═══════════════════════════════════════════════════════
// ── GLÜHWÜRMCHEN-LOGIK ───────────────────────────────────
// ═══════════════════════════════════════════════════════

int firefliesNeeded() {
  return FIREFLY_BASE_NEEDED + (level - 1);
}

int firefliesActiveCount() {
  int c = 0;
  for (int i = 0; i < MAX_FIREFLIES; i++)
    if (fireflies[i].active) c++;
  return c;
}

void initFireflies() {
  for (int i = 0; i < MAX_FIREFLIES; i++)
    fireflies[i].active = false;
  firefliesCollectedThisLevel = 0;
  goalsUnlocked = false;
  lastFireflySpawnCheck = millis();
}

void trySpawnFirefly() {
  if (goalsUnlocked) return;

  int stillNeeded = firefliesNeeded() - firefliesCollectedThisLevel - firefliesActiveCount();
  if (stillNeeded <= 0) return;
  if ((int)random(0, 100) >= FIREFLY_SPAWN_CHANCE) return;

  int slot = -1;
  for (int i = 0; i < MAX_FIREFLIES; i++) {
    if (!fireflies[i].active) { slot = i; break; }
  }
  if (slot == -1) return;

  bool putOnLog = random(0, 2) == 0;

  int tries = 0;
  while (tries < 20) {
    int laneIdx = random(0, TOTAL_LANES);
    if (lanes[laneIdx].isLog == putOnLog) {
      fireflies[slot].active    = true;
      fireflies[slot].onLog     = putOnLog;
      fireflies[slot].laneIndex = laneIdx;

      if (putOnLog) {
        fireflies[slot].offsetInLog = random(0, lanes[laneIdx].len);
      } else {
        fireflies[slot].x = random(0, MATRIX_W);
      }
      return;
    }
    tries++;
  }
}

void updateFireflies() {
  for (int i = 0; i < MAX_FIREFLIES; i++) {
    if (!fireflies[i].active) continue;
  }
}

int fireflyScreenX(Firefly &f) {
  if (f.onLog) {
    int laneIdx = f.laneIndex;
    return ((lanes[laneIdx].pos + f.offsetInLog) % MATRIX_W + MATRIX_W) % MATRIX_W;
  } else {
    return f.x;
  }
}

void drawFireflies() {
  unsigned long t = millis();
  for (int i = 0; i < MAX_FIREFLIES; i++) {
    if (!fireflies[i].active) continue;

    int x = fireflyScreenX(fireflies[i]);
    int y = lanes[fireflies[i].laneIndex].row;

    float phase = (t / (float)(FIREFLY_PULSE_SPEED * 10)) + i * 1.7f;
    float pulse = (sinf(phase) + 1.0f) / 2.0f;
    uint8_t brightness = 60 + (uint8_t)(pulse * 195);

    CRGB c = COL_FIREFLY;
    c.nscale8_video(brightness);
    setPixel(x, y, c);
  }
}

void checkFireflyCollision() {
  if (goalsUnlocked) return;

  for (int i = 0; i < MAX_FIREFLIES; i++) {
    if (!fireflies[i].active) continue;

    int x = fireflyScreenX(fireflies[i]);
    int y = lanes[fireflies[i].laneIndex].row;

    if (frogX == x && frogY == y) {
      fireflies[i].active = false;
      firefliesCollectedThisLevel++;

      Serial.print("Glühwürmchen eingesammelt: ");
      Serial.print(firefliesCollectedThisLevel);
      Serial.print(" / ");
      Serial.println(firefliesNeeded());

      if (firefliesCollectedThisLevel >= firefliesNeeded()) {
        goalsUnlocked = true;
        for (int j = 0; j < MAX_FIREFLIES; j++) fireflies[j].active = false;
        Serial.println("Zielbuchten freigeschaltet!");
      }
    }
  }
}

// ── Rendering ────────────────────────────────────────────
void drawHUD() {
  for (int i = 0; i < lives; i++)
    setPixel(i, ROW_START, CRGB(0, 255, 100));
  for (int i = 0; i < level && i < 8; i++)
    setPixel(MATRIX_W - 1 - i, ROW_START, COL_GOAL);
}

void drawFrame() {
  FastLED.clear();

  // Zielzeile
  for (int x = 0; x < MATRIX_W; x++)
    setPixel(x, ROW_GOAL, CRGB(0, 30, 0));
  for (int i = 0; i < NUM_GOALS; i++) {
    if (!goalReached[i]) {
      if (goalsUnlocked) {
        setPixel(goalX[i]-1, ROW_GOAL, COL_GOAL);
        setPixel(goalX[i],   ROW_GOAL, COL_GOAL);
        setPixel(goalX[i]+1, ROW_GOAL, COL_GOAL);
        setPixel(goalX[i],   ROW_GOAL, COL_FLY);
      } else {
        setPixel(goalX[i], ROW_GOAL, CRGB(60, 60, 20));
      }
    } else {
      setPixel(goalX[i], ROW_GOAL, COL_FROG);
    }
  }

  // Wasserzone
  for (int y = ROW_WATER_TOP; y <= ROW_WATER_BOT; y++)
    for (int x = 0; x < MATRIX_W; x++)
      setPixel(x, y, COL_WATER);

  // Sicherheitszone
  for (int x = 0; x < MATRIX_W; x++)
    setPixel(x, ROW_SAFE_MID, COL_SAFE);

  // Straße
  for (int y = ROW_ROAD_TOP; y <= ROW_ROAD_BOT; y++)
    for (int x = 0; x < MATRIX_W; x++)
      setPixel(x, y, COL_ROAD);

  // Trennlinie + Startzeile
  for (int x = 0; x < MATRIX_W; x++) {
    setPixel(x, 14,        CRGB(20, 20, 20));
    setPixel(x, ROW_START, COL_SAFE);
  }

  // Logs
  for (int i = 0; i < TOTAL_LANES; i++) {
    if (!lanes[i].isLog) continue;
    for (int l = 0; l < lanes[i].len; l++)
      setPixel((lanes[i].pos + l) % MATRIX_W, lanes[i].row, COL_LOG);
  }

  // Autos
  for (int i = 0; i < TOTAL_LANES; i++) {
    if (lanes[i].isLog) continue;
    for (int l = 0; l < lanes[i].len; l++) {
      setPixel((lanes[i].pos + l) % MATRIX_W, lanes[i].row, COL_CAR);
      if (lanes[i].hasSecondCar)
        setPixel((lanes[i].pos2 + l) % MATRIX_W, lanes[i].row, COL_CAR);
    }
  }

  // Glühwürmchen
  drawFireflies();

  drawHUD();
  setPixel(frogX, frogY, COL_FROG);
}

// ── Startbildschirm ──────────────────────────────────────
void showStartScreen() {
  FastLED.clear();

  for (int y = 0; y < MATRIX_H; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      uint32_t c = titleBitmap[y][x];
      setPixel(x, y, CRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF));
    }
  }

  FastLED.show();
  Serial.println("Startbildschirm – beliebige Taste drücken");
}

// ── Game Over / Score-Screen ─────────────────────────────
void showGameOver() {
  FastLED.clear();

  CRGB borderCol = CRGB(0, 60, 0);
  CRGB titleCol  = CRGB(0, 255, 80);
  CRGB barBg     = CRGB(0, 30, 0);
  CRGB barFg     = CRGB(80, 255, 80);
  CRGB flyCol    = COL_FLY;

  // Rahmen
  for (int x = 0; x < MATRIX_W; x++) {
    setPixel(x, 0,  borderCol);
    setPixel(x, 15, borderCol);
  }
  for (int y = 0; y < MATRIX_H; y++) {
    setPixel(0,  y, borderCol);
    setPixel(15, y, borderCol);
  }

  // Trauriger Frosch
  CRGB frogSad = CRGB(0, 150, 40);
  for (int x = 6; x <= 9; x++) setPixel(x, 2, frogSad);
  setPixel(6, 3, frogSad); setPixel(9, 3, frogSad);
  setPixel(7, 2, CRGB(0,0,0));
  setPixel(8, 2, CRGB(0,0,0));
  for (int x = 6; x <= 9; x++) setPixel(x, 4, frogSad);

  // Trennlinie
  for (int x = 2; x < 14; x++) setPixel(x, 6, CRGB(0,80,0));

  // Level-Balken
  int levelBarLen = min(level, 14);
  for (int x = 0; x < 14; x++) {
    setPixel(1 + x, 8, (x < levelBarLen) ? barFg : barBg);
  }
  setPixel(0, 8, titleCol);

  // Fliegen-Punkte
  int flies = totalFliesCaught;
  int shown = min(flies, 14);
  for (int x = 0; x < 14; x++) {
    if (x < shown) {
      setPixel(1 + x, 10, flyCol);
    } else {
      setPixel(1 + x, 10, barBg);
    }
  }
  setPixel(0, 10, CRGB(255,200,0));

  // Deko
  for (int x = 2; x < 14; x += 3)
    setPixel(x, 13, CRGB(0,90,0));

  FastLED.show();
  Serial.print("Game Over – Level: "); Serial.print(level);
  Serial.print("  Fliegen: ");         Serial.println(totalFliesCaught);
}

// ── Matrix-Hilfsfunktionen ───────────────────────────────
int xyToIndex(int x, int y) {
  if (y % 2 == 0) return y * MATRIX_W + x;
  else            return y * MATRIX_W + (MATRIX_W - 1 - x);
}

void setPixel(int x, int y, CRGB color) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;
  leds[xyToIndex(x, y)] = color;
}