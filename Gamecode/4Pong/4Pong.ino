#include <LittleFS.h>
#include <loadBMP.h>
#include "olb_init_v1.h"

// NodeMCU 1.0 (ESP-12E Module)
// 4Pong - openLUXbox

#include <FastLED.h>
#include <ESP8266WiFi.h>

// ============================================================
// DEFINES / PARAMETER
// ============================================================
#define LED_PIN        D4
#define NUM_LEDS       256
#define MATRIX_W       16
#define MATRIX_H       16
#define BRIGHTNESS     51   // 20% von 255

#define BTN_LEFT       4    // D2 grün
#define BTN_RIGHT      14   // D5 gelb
#define BTN_DOWN       5    // D1 weiß
#define BTN_UP         12   // D6 blau

#define FPS            30
#define FRAME_MS       (1000 / FPS)

#define BALL_START_SPEED     4.0f
#define BALL_SPEED_INC       1.0f
#define BALL_SPEED_INTERVAL  10000

#define PADDLE_SIZE           4
#define PADDLE_DEFLECT_ANGLE  10.0f

#define GAMEOVER_SPEED 12.0f

#define DEBOUNCE_MS    30

// Farben
#define COL_BLACK      CRGB(0,0,0)
#define COL_BALL       CRGB(180,0,255)
#define COL_YELLOW     CRGB(255,255,80)
#define COL_GREEN      CRGB(80,255,80)
#define COL_BLUE       CRGB(80,160,255)
#define COL_WHITE      CRGB(255,255,255)
#define COL_CYAN       CRGB(80,255,255)
#define COL_MAGENTA    CRGB(255,80,255)

// ============================================================
// STRUCTS
// ============================================================
struct Button {
  uint8_t pin;
  bool lastState;
  bool pressed;
  unsigned long lastDebounce;
};

struct Paddle {
  int pos;
  int minPos;
  int maxPos;
};

// ============================================================
// PROTOTYPEN
// ============================================================
void clearMatrix();
void showStartScreen();
void startGame();
void updateButtons();
bool anyButtonPressed();
void handleInput();
void updateBall(unsigned long delta_ms);
void drawPaddles();
void drawBall();
int  xyToIndex(int x, int y);
bool isOnPaddle(int x, int y, int &paddleId, int &half);
void reflectBall(int paddleId, int half);
void fillGameOverPixels();
CRGB randomColor();
void drawChar(const uint8_t ch[4][5], int startX, int startY, CRGB color);

// ============================================================
// GLOBALE VARIABLEN
// ============================================================
CRGB leds[NUM_LEDS];

Button btnLeft  = {BTN_LEFT,  HIGH, false, 0};
Button btnRight = {BTN_RIGHT, HIGH, false, 0};
Button btnDown  = {BTN_DOWN,  HIGH, false, 0};
Button btnUp    = {BTN_UP,    HIGH, false, 0};
Button* buttons[4] = {&btnLeft, &btnRight, &btnDown, &btnUp};

Paddle paddles[4];

float ballX, ballY;
float ballAngle;
float ballSpeed;

unsigned long lastSpeedUp;
int score;

enum GameState { STATE_START, STATE_PLAY, STATE_GAMEOVER };
GameState gameState;

int   gameOverPixels;
float gameOverAccum;
CRGB  gameOverColor;
int   gameOverRound;
bool  gameOverScreenCleared;

unsigned long lastFrameTime;

// NEU: Verhindert sofortigen Start nach Boot
bool startScreenJustShown = false;

// ============================================================
// FONT DATEN
// ============================================================
const uint8_t FONT_4[4][5] = {
  {0,0,0,0,0},
  {1,1,1,1,1},
  {0,0,1,0,0},
  {1,1,1,0,0}
};
const uint8_t FONT_P[4][5] = {
  {0,0,0,0,0},
  {1,1,1,0,0},
  {1,0,1,0,0},
  {1,1,1,1,1}
};
const uint8_t FONT_O[4][5] = {
  {0,0,0,0,0},
  {0,1,1,1,0},
  {1,0,0,0,1},
  {0,1,1,1,0}
};
const uint8_t FONT_N[4][5] = {
  {1,1,1,1,1},
  {0,0,1,0,0},
  {0,1,0,0,0},
  {1,1,1,1,1}
};
const uint8_t FONT_G[4][5] = {
  {0,0,1,1,1},
  {1,0,0,0,1},
  {0,1,1,1,0},
  {0,0,0,0,0}
};

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1500); // ESP boot stabilisieren
  Serial.println("[4Pong] Setup gestartet");

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(100);
  // KEIN wdtDisable() – kann Boot blockieren

  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN,  INPUT_PULLUP);
  pinMode(BTN_UP,    INPUT_PULLUP);
  pinMode(D3, INPUT);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);

  //Startanimation
  olb_v1_playIntro(leds);

  FastLED.setBrightness(BRIGHTNESS);

  // LittleFS starten
  if (!LittleFS.begin()) {
    Serial.println("[4Pong] FEHLER: LittleFS konnte nicht gestartet werden!");
  } else {
    Serial.println("[4Pong] LittleFS bereit");
  }

  LittleFS.begin();

  // Alle Dateien auflisten
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
      Serial.print("[FS] Datei: ");
      Serial.print(dir.fileName());
      Serial.print(" | Größe: ");
      Serial.println(dir.fileSize());
  }

  // Verbesserter Random Seed
  uint32_t seed = 0;
  for (int i = 0; i < 32; i++) {
    seed ^= (uint32_t)analogRead(A0) << (i % 16);
    delay(1);
  }
  for (int i = 0; i < 16; i++) {
    seed ^= (uint32_t)digitalRead(D3) << i;
  }
  randomSeed(seed);
  Serial.print("[4Pong] Random Seed: ");
  Serial.println(seed);

  // Startscreen zeichnen
  clearMatrix();
  FastLED.show();
  delay(100);
  showStartScreen();
  gameState = STATE_START;

  // Buttons NACH Startscreen initialisieren – Bounce vermeiden
  delay(300);
  for (int i = 0; i < 4; i++) {
    buttons[i]->lastState = digitalRead(buttons[i]->pin);
    buttons[i]->pressed   = false;
  }

  // Flag: ersten Loop-Durchlauf ignorieren
  startScreenJustShown = true;

  lastFrameTime = millis();
  Serial.println("[4Pong] Warte auf Tastendruck...");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  yield(); // WDT-freundlich, kein wdtFeed nötig

  unsigned long now = millis();
  unsigned long delta = now - lastFrameTime;
  if (delta < FRAME_MS) return;
  lastFrameTime = now;

  updateButtons();

  switch (gameState) {

    case STATE_START:
      // Ersten Frame nach Boot überspringen
      if (startScreenJustShown) {
        startScreenJustShown = false;
        return;
      }
      if (anyButtonPressed()) {
        Serial.println("[4Pong] Startbildschirm: Taste gedrückt -> Spiel startet");
        startGame();
      }
      break;

    case STATE_PLAY:
      handleInput();
      updateBall(delta);
      if (gameState == STATE_PLAY) {
        clearMatrix();
        drawPaddles();
        drawBall();
        FastLED.show();
      }
      break;

    case STATE_GAMEOVER:
      if (!gameOverScreenCleared) {
        clearMatrix();
        FastLED.show();
        gameOverScreenCleared = true;
        Serial.println("[GAMEOVER] Screen geleert, starte Score-Animation");
      } else if (anyButtonPressed()) {
        Serial.println("[GAMEOVER] Taste gedrückt -> Startbildschirm");
        showStartScreen();
        gameState = STATE_START;
        // Auch hier: Flag setzen damit kein Doppel-Trigger
        startScreenJustShown = true;
      } else {
        fillGameOverPixels();
        FastLED.show();
      }
      break;
  }
}

// ============================================================
// HILFSFUNKTIONEN
// ============================================================
int xyToIndex(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return -1;
  if (y % 2 == 0) {
    return y * MATRIX_W + x;
  } else {
    return y * MATRIX_W + (MATRIX_W - 1 - x);
  }
}

void clearMatrix() {
  fill_solid(leds, NUM_LEDS, COL_BLACK);
}

void drawChar(const uint8_t ch[4][5], int startX, int startY, CRGB color) {
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 5; row++) {
      if (ch[col][row]) {
        int idx = xyToIndex(startX + col, startY + row);
        if (idx >= 0) leds[idx] = color;
      }
    }
  }
}

void showStartScreen() {
  Serial.println("[4Pong] Lade Startbildschirm...");
  bool ok = showBMP("/start.bmp", leds, NUM_LEDS, MATRIX_W, MATRIX_H, xyToIndex);
  if (!ok) {
    // Fallback: Text anzeigen wenn BMP fehlt
    clearMatrix();
    drawChar(FONT_4, 6,  2, COL_BALL);
    drawChar(FONT_P, 12, 9, COL_BALL);
    drawChar(FONT_O, 8,  9, COL_BALL);
    drawChar(FONT_N, 4,  9, COL_BALL);
    drawChar(FONT_G, 0,  9, COL_BALL);
    FastLED.show();
    Serial.println("[4Pong] Fallback: Text-Startbildschirm angezeigt");
  } else {
    Serial.println("[4Pong] Startbildschirm (BMP) angezeigt");
  }
}


// ============================================================
// SPIEL INITIALISIEREN
// ============================================================
void startGame() {
  paddles[0].pos    = (MATRIX_W - PADDLE_SIZE) / 2;
  paddles[0].minPos = 0;
  paddles[0].maxPos = MATRIX_W - PADDLE_SIZE;

  paddles[1].pos    = (MATRIX_W - PADDLE_SIZE) / 2;
  paddles[1].minPos = 0;
  paddles[1].maxPos = MATRIX_W - PADDLE_SIZE;

  paddles[2].pos    = (MATRIX_H - PADDLE_SIZE) / 2;
  paddles[2].minPos = 0;
  paddles[2].maxPos = MATRIX_H - PADDLE_SIZE;

  paddles[3].pos    = (MATRIX_H - PADDLE_SIZE) / 2;
  paddles[3].minPos = 0;
  paddles[3].maxPos = MATRIX_H - PADDLE_SIZE;

  Serial.println("[4Pong] Schläger initialisiert (alle mittig)");

  ballX     = 7.5f;
  ballY     = 7.5f;
  ballSpeed = BALL_START_SPEED;
  lastSpeedUp = millis();
  score = 0;

  // Diagonaler Startwinkel, nie achsenparallel
  int baseAngles[] = {30, 60, 120, 150, 210, 240, 300, 330};
  ballAngle = baseAngles[random(0, 8)] + random(-10, 10);

  Serial.print("[4Pong] Ball startet: Winkel=");
  Serial.print(ballAngle);
  Serial.println(" Grad");

  gameOverPixels        = 0;
  gameOverAccum         = 0;
  gameOverColor         = COL_BALL;
  gameOverRound         = 0;
  gameOverScreenCleared = false;

  gameState = STATE_PLAY;
}

// ============================================================
// BUTTONS
// ============================================================
void updateButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    Button* b = buttons[i];
    b->pressed = false;
    bool current = digitalRead(b->pin);
    if (current == LOW && b->lastState == HIGH) {
      if (now - b->lastDebounce > DEBOUNCE_MS) {
        b->pressed = true;
        b->lastDebounce = now;
        Serial.print("[BTN] Taste gedrückt: Index=");
        Serial.println(i);
      }
    }
    b->lastState = current;
  }
}

bool anyButtonPressed() {
  for (int i = 0; i < 4; i++) {
    if (buttons[i]->pressed) return true;
  }
  return false;
}

// ============================================================
// INPUT -> SCHLÄGER BEWEGEN
// ============================================================
void handleInput() {
  if (btnLeft.pressed) {
    if (paddles[0].pos > paddles[0].minPos) { paddles[0].pos--; Serial.println("[INPUT] Oben nach links"); }
    else Serial.println("[INPUT] Oben bereits am linken Rand");
    if (paddles[1].pos > paddles[1].minPos) { paddles[1].pos--; Serial.println("[INPUT] Unten nach links"); }
    else Serial.println("[INPUT] Unten bereits am linken Rand");
  }
  if (btnRight.pressed) {
    if (paddles[0].pos < paddles[0].maxPos) { paddles[0].pos++; Serial.println("[INPUT] Oben nach rechts"); }
    else Serial.println("[INPUT] Oben bereits am rechten Rand");
    if (paddles[1].pos < paddles[1].maxPos) { paddles[1].pos++; Serial.println("[INPUT] Unten nach rechts"); }
    else Serial.println("[INPUT] Unten bereits am rechten Rand");
  }
  if (btnUp.pressed) {
    if (paddles[2].pos > paddles[2].minPos) { paddles[2].pos--; Serial.println("[INPUT] Links nach oben"); }
    else Serial.println("[INPUT] Links bereits am oberen Rand");
    if (paddles[3].pos > paddles[3].minPos) { paddles[3].pos--; Serial.println("[INPUT] Rechts nach oben"); }
    else Serial.println("[INPUT] Rechts bereits am oberen Rand");
  }
  if (btnDown.pressed) {
    if (paddles[2].pos < paddles[2].maxPos) { paddles[2].pos++; Serial.println("[INPUT] Links nach unten"); }
    else Serial.println("[INPUT] Links bereits am unteren Rand");
    if (paddles[3].pos < paddles[3].maxPos) { paddles[3].pos++; Serial.println("[INPUT] Rechts nach unten"); }
    else Serial.println("[INPUT] Rechts bereits am unteren Rand");
  }
}

// ============================================================
// BALL UPDATE
// ============================================================
void updateBall(unsigned long delta_ms) {
  unsigned long now = millis();
  if (now - lastSpeedUp >= BALL_SPEED_INTERVAL) {
    ballSpeed += BALL_SPEED_INC;
    lastSpeedUp = now;
    Serial.print("[BALL] Geschwindigkeit erhöht auf ");
    Serial.println(ballSpeed);
  }

  float rad = ballAngle * PI / 180.0f;
  float dx = cos(rad) * ballSpeed * delta_ms / 1000.0f;
  float dy = sin(rad) * ballSpeed * delta_ms / 1000.0f;

  float newX = ballX + dx;
  float newY = ballY + dy;

  int nx = (int)round(newX);
  int ny = (int)round(newY);

  if (nx < 0 || nx >= MATRIX_W || ny < 0 || ny >= MATRIX_H) {
    Serial.print("[BALL] Rand erreicht bei (");
    Serial.print(nx); Serial.print("/"); Serial.print(ny);
    Serial.print(") -> Game Over | Score: ");
    Serial.println(score);
    gameState             = STATE_GAMEOVER;
    gameOverPixels        = 0;
    gameOverAccum         = 0;
    gameOverColor         = COL_BALL;
    gameOverRound         = 0;
    gameOverScreenCleared = false;
    return;
  }

  int paddleId, half;
  if (isOnPaddle(nx, ny, paddleId, half)) {
    reflectBall(paddleId, half);
    score++;
    float rad2 = ballAngle * PI / 180.0f;
    ballX = nx + cos(rad2);
    ballY = ny + sin(rad2);
    Serial.print("[BALL] Schläger getroffen! Score=");
    Serial.println(score);
  } else {
    ballX = newX;
    ballY = newY;
  }
}

bool isOnPaddle(int x, int y, int &paddleId, int &half) {
  if (y == 0) {
    int p = paddles[0].pos;
    if (x >= p && x < p + PADDLE_SIZE) {
      paddleId = 0;
      half = (x < p + PADDLE_SIZE / 2) ? 0 : 1;
      return true;
    }
  }
  if (y == MATRIX_H - 1) {
    int p = paddles[1].pos;
    if (x >= p && x < p + PADDLE_SIZE) {
      paddleId = 1;
      half = (x < p + PADDLE_SIZE / 2) ? 0 : 1;
      return true;
    }
  }
  if (x == 0) {
    int p = paddles[2].pos;
    if (y >= p && y < p + PADDLE_SIZE) {
      paddleId = 2;
      half = (y < p + PADDLE_SIZE / 2) ? 0 : 1;
      return true;
    }
  }
  if (x == MATRIX_W - 1) {
    int p = paddles[3].pos;
    if (y >= p && y < p + PADDLE_SIZE) {
      paddleId = 3;
      half = (y < p + PADDLE_SIZE / 2) ? 0 : 1;
      return true;
    }
  }
  return false;
}

void reflectBall(int paddleId, int half) {
  float a = ballAngle;

  if (paddleId == 0 || paddleId == 1) {
    a = fmod(360.0f - a, 360.0f);
  } else {
    a = fmod(180.0f - a + 360.0f, 360.0f);
  }

  float deflect = 0;
  if (paddleId == 0 || paddleId == 1) {
    bool flyingLeft = (ballAngle > 90.0f && ballAngle < 270.0f);
    bool hitLeft    = (half == 0);
    deflect = (flyingLeft == hitLeft) ? PADDLE_DEFLECT_ANGLE : -PADDLE_DEFLECT_ANGLE;
  } else {
    bool flyingUp = (ballAngle > 180.0f && ballAngle < 360.0f);
    bool hitTop   = (half == 0);
    deflect = (flyingUp == hitTop) ? PADDLE_DEFLECT_ANGLE : -PADDLE_DEFLECT_ANGLE;
  }

  a = fmod(a + deflect + 360.0f, 360.0f);
  ballAngle = a;
}

// ============================================================
// ZEICHNEN
// ============================================================
void drawPaddles() {
  for (int i = 0; i < PADDLE_SIZE; i++) {
    int x = paddles[0].pos + i;
    CRGB col = (i < PADDLE_SIZE / 2) ? COL_GREEN : COL_YELLOW;
    int idx = xyToIndex(x, 0);
    if (idx >= 0) leds[idx] = col;
  }
  for (int i = 0; i < PADDLE_SIZE; i++) {
    int x = paddles[1].pos + i;
    CRGB col = (i < PADDLE_SIZE / 2) ? COL_GREEN : COL_YELLOW;
    int idx = xyToIndex(x, MATRIX_H - 1);
    if (idx >= 0) leds[idx] = col;
  }
  for (int i = 0; i < PADDLE_SIZE; i++) {
    int y = paddles[2].pos + i;
    CRGB col = (i < PADDLE_SIZE / 2) ? COL_BLUE : COL_WHITE;
    int idx = xyToIndex(0, y);
    if (idx >= 0) leds[idx] = col;
  }
  for (int i = 0; i < PADDLE_SIZE; i++) {
    int y = paddles[3].pos + i;
    CRGB col = (i < PADDLE_SIZE / 2) ? COL_BLUE : COL_WHITE;
    int idx = xyToIndex(MATRIX_W - 1, y);
    if (idx >= 0) leds[idx] = col;
  }

  // Ecken
  if (paddles[0].pos == 0 && paddles[2].pos == 0) {
    int idx = xyToIndex(0, 0);
    if (idx >= 0) leds[idx] = COL_MAGENTA;
  }
  if ((paddles[0].pos + PADDLE_SIZE - 1 >= MATRIX_W - 1) && paddles[3].pos == 0) {
    int idx = xyToIndex(MATRIX_W - 1, 0);
    if (idx >= 0) leds[idx] = COL_CYAN;
  }
  if (paddles[1].pos == 0 && (paddles[2].pos + PADDLE_SIZE - 1 >= MATRIX_H - 1)) {
    int idx = xyToIndex(0, MATRIX_H - 1);
    if (idx >= 0) leds[idx] = CRGB(255, 255, 128);
  }
  if ((paddles[1].pos + PADDLE_SIZE - 1 >= MATRIX_W - 1) && (paddles[3].pos + PADDLE_SIZE - 1 >= MATRIX_H - 1)) {
    int idx = xyToIndex(MATRIX_W - 1, MATRIX_H - 1);
    if (idx >= 0) leds[idx] = CRGB(128, 255, 128);
  }
}

void drawBall() {
  int bx = (int)round(ballX);
  int by = (int)round(ballY);
  int idx = xyToIndex(bx, by);
  if (idx >= 0) leds[idx] = COL_BALL;
}

// ============================================================
// GAME OVER ANIMATION
// ============================================================
CRGB randomColor() {
  return CRGB(random(100, 255), random(100, 255), random(100, 255));
}

void fillGameOverPixels() {
  gameOverAccum += GAMEOVER_SPEED / FPS;
  int toFill = (int)gameOverAccum;
  gameOverAccum -= toFill;

  for (int i = 0; i < toFill; i++) {
    if (gameOverRound == 0 && gameOverPixels >= score) {
      Serial.print("[GAMEOVER] Animation abgeschlossen, Score=");
      Serial.println(score);
      return;
    }

    int pixelInRound = gameOverPixels % NUM_LEDS;

    if (pixelInRound == 0 && gameOverPixels > 0) {
      gameOverRound++;
      gameOverColor = randomColor();
      Serial.print("[GAMEOVER] Neue Füllung, Runde=");
      Serial.println(gameOverRound);
    }

    int x = pixelInRound % MATRIX_W;
    int y = pixelInRound / MATRIX_W;
    int idx = xyToIndex(x, y);
    if (idx >= 0) leds[idx] = gameOverColor;

    gameOverPixels++;
  }
}
