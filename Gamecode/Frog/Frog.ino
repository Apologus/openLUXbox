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

#define COL_FROG       CRGB(0,255,0)
#define COL_CAR        CRGB(255,50,0)
#define COL_LOG        CRGB(139,69,19)
#define COL_WATER      CRGB(0,0,180)
#define COL_ROAD       CRGB(60,60,60)
#define COL_SAFE       CRGB(0,80,0)
#define COL_GOAL       CRGB(255,215,0)
#define COL_GOAL_REACH CRGB(255,255,0)
#define COL_FLY        CRGB(255,100,255)

// ── Structs ──────────────────────────────────────────────
struct Lane {
  int  row;
  int  pos;   // Auto 1 (Logs: einzige Position)
  int  pos2;  // Auto 2 (nur für Autos genutzt)
  int  len;
  int  dir;
  bool isLog;
};

struct Button {
  uint8_t       pin;
  bool          lastState;
  unsigned long lastTime;
  bool          pressed;
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

// ── Zielpunkte ───────────────────────────────────────────
const int goalX[NUM_GOALS] = {2, 6, 10, 14};
bool      goalReached[NUM_GOALS];
int       goalsLeft;

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

// ── Lane-Init (mit zufälligem Auto-Abstand) ───────────────
void initLanes() {
  for (int i = 0; i < TOTAL_LANES; i++) {
    lanes[i].row   = laneRows[i];
    lanes[i].dir   = laneDirs[i];
    lanes[i].isLog = laneIsLog[i];
    lanes[i].len   = laneLen[i];
    lanes[i].pos   = random(0, MATRIX_W);

    if (!laneIsLog[i]) {
      // Zweites Auto mit zufälligem Abstand hinter dem ersten
      int gap = random(CAR_GAP_MIN, CAR_GAP_MAX + 1);
      // "hinter" = entgegen der Fahrtrichtung
      // pos2 = pos - (CAR_LEN + gap) → modulo damit positiv
      lanes[i].pos2 = ((lanes[i].pos - (CAR_LEN + gap)) % MATRIX_W
                        + MATRIX_W) % MATRIX_W;
    } else {
      lanes[i].pos2 = 0; // unused für Logs
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
  }

  checkCollision();
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
  initLanes();   // neue Zufallspositionen + neue Abstände
  initGoals();

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

    if (!lanes[i].isLog) {
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
      if ((lanes[i].pos2 + l) % MATRIX_W == frogX) return true;
    }
  }
  return false;
}

// ── Kollision ────────────────────────────────────────────
void checkCollision() {

  // Zielzeile
  if (frogY == ROW_GOAL) {
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
    waitForRestart = true;
  } else {
    frogX = MATRIX_W / 2;
    frogY = ROW_START;
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
      setPixel(goalX[i]-1, ROW_GOAL, COL_GOAL);
      setPixel(goalX[i],   ROW_GOAL, COL_GOAL);
      setPixel(goalX[i]+1, ROW_GOAL, COL_GOAL);
      setPixel(goalX[i],   ROW_GOAL, COL_FLY);
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

  // Autos (2 pro Spur)
  for (int i = 0; i < TOTAL_LANES; i++) {
    if (lanes[i].isLog) continue;
    for (int l = 0; l < lanes[i].len; l++) {
      setPixel((lanes[i].pos  + l) % MATRIX_W, lanes[i].row, COL_CAR);
      setPixel((lanes[i].pos2 + l) % MATRIX_W, lanes[i].row, COL_CAR);
    }
  }

  drawHUD();
  setPixel(frogX, frogY, COL_FROG);
}

// ── Startbildschirm ──────────────────────────────────────
void showStartScreen() {
  FastLED.clear();

  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 0,  COL_GOAL);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 2,  COL_WATER);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 3,  COL_WATER);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 9,  COL_ROAD);
  for (int x = 0; x < MATRIX_W; x++) setPixel(x, 10, COL_ROAD);

  setPixel(2,  0, COL_FLY);
  setPixel(6,  0, COL_FLY);
  setPixel(10, 0, COL_FLY);
  setPixel(14, 0, COL_FLY);

  setPixel(7, 6, COL_FROG); setPixel(8, 6, COL_FROG);
  setPixel(7, 7, COL_FROG); setPixel(8, 7, COL_FROG);
  setPixel(6, 5, CRGB(255,255,255));
  setPixel(9, 5, CRGB(255,255,255));

  // 2 Autos als Vorschau
  setPixel(3, 9, COL_CAR); setPixel(4, 9, COL_CAR); setPixel(5, 9, COL_CAR);
  setPixel(9, 9, COL_CAR); setPixel(10,9, COL_CAR); setPixel(11,9, COL_CAR);

  setPixel(0, 15, CRGB(0,255,100));
  setPixel(1, 15, CRGB(0,255,100));
  setPixel(2, 15, CRGB(0,255,100));

  FastLED.show();
  Serial.println("Startbildschirm – beliebige Taste drücken");
}

// ── Game Over ────────────────────────────────────────────
void showGameOver() {
  FastLED.clear();

  for (int x = 0; x < MATRIX_W; x++) {
    setPixel(x, 0,  CRGB(180,0,0));
    setPixel(x, 15, CRGB(180,0,0));
  }
  for (int y = 0; y < MATRIX_H; y++) {
    setPixel(0,  y, CRGB(180,0,0));
    setPixel(15, y, CRGB(180,0,0));
  }
  for (int i = 0; i < 10; i++) {
    setPixel(3+i,  3+i, CRGB(255,0,0));
    setPixel(12-i, 3+i, CRGB(255,0,0));
  }
  for (int i = 0; i < level && i < 14; i++)
    setPixel(1+i, 13, COL_GOAL);

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
