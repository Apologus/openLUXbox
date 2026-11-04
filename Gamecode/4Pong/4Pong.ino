// NodeMCU 1.0 (ESP-12E Module)
// 4Pong - openLUXbox

#include <FastLED.h>
#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif

#include "olb_init_v1.h"

// ============================================================
// DEFINES / PARAMETER
// ============================================================
#define LED_PIN        2
#define NUM_LEDS       256
#define MATRIX_W       16
#define MATRIX_H       16
#define BRIGHTNESS     51   // 20% von 255

#if defined(ESP8266)
    #define BTN_LEFT       4    // D2 grün
    #define BTN_RIGHT      14   // D5 gelb
    #define BTN_DOWN       5    // D1 weiß
    #define BTN_UP         12   // D6 blau
    #define SEEDPIN        D3
#elif defined(ESP32)
    #define BTN_LEFT       19   // grün
    #define BTN_RIGHT      23   // gelb
    #define BTN_DOWN       26   // weiß
    #define BTN_UP         18   // blau
    #define SEEDPIN        33
#endif

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
// STARTBILDSCHIRM - FEST INTEGRIERTER HEXCODE
// ============================================================
const uint32_t PROGMEM SCREEN_START[16][16] = {
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x50FF50, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x73FF50, 0x73FF50, 0x000000, 0x000000},
    {0x000000, 0x96FF50, 0x96FF50, 0x96FF50, 0x96FF50, 0x96FF50, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x96FF50, 0x000000, 0x96FF50, 0x000000, 0x000000},
    {0x000000, 0xB9FF50, 0x000000, 0x000000, 0x000000, 0xB9FF50, 0xB9FF50, 0x000000, 0x000000, 0x000000, 0xB9FF50, 0x000000, 0x000000, 0xB9FF50, 0x000000, 0x000000},
    {0x000000, 0xDCFF50, 0x000000, 0x000000, 0x000000, 0x000000, 0xDCFF50, 0x000000, 0x000000, 0xDCFF50, 0x000000, 0x000000, 0x000000, 0xDCFF50, 0x000000, 0x000000},
    {0x000000, 0xFFFF50, 0x000000, 0x000000, 0x000000, 0x000000, 0xFFFF50, 0x000000, 0x000000, 0xFFFF50, 0xFFFF50, 0xFFFF50, 0xFFFF50, 0xFFFF50, 0xFFFF50, 0x000000},
    {0x000000, 0xECED73, 0x000000, 0x000000, 0x000000, 0xECED73, 0xECED73, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xECED73, 0x000000, 0x000000},
    {0x000000, 0xD9DC96, 0xD9DC96, 0xD9DC96, 0xD9DC96, 0xD9DC96, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xD9DC96, 0x000000, 0x000000},
    {0x000000, 0xC6CAB9, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0xB3B8DC, 0x000000, 0x000000, 0xB3B8DC, 0xB3B8DC, 0x000000, 0x000000, 0xB3B8DC, 0xB3B8DC, 0xB3B8DC, 0x000000, 0x000000, 0xB3B8DC, 0xB3B8DC, 0xB3B8DC},
    {0x000000, 0xA0A0FF, 0x000000, 0xA0A0FF, 0x000000, 0x000000, 0xA0A0FF, 0x000000, 0xA0A0FF, 0x000000, 0xA0A0FF, 0x000000, 0xA0A0FF, 0x000000, 0x000000, 0xA0A0FF},
    {0x000000, 0xB3B3FF, 0x000000, 0xB3B3FF, 0x000000, 0x000000, 0xB3B3FF, 0x000000, 0xB3B3FF, 0x000000, 0xB3B3FF, 0x000000, 0xB3B3FF, 0x000000, 0x000000, 0xB3B3FF},
    {0x000000, 0x000000, 0x000000, 0x000000, 0xC6C6FF, 0xC6C6FF, 0x000000, 0x000000, 0xC6C6FF, 0x000000, 0xC6C6FF, 0x000000, 0x000000, 0xC6C6FF, 0xC6C6FF, 0xC6C6FF},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xD9D9FF},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xECECFF, 0xECECFF, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000}
};

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
void drawGameOverScore();
CRGB getGradientColor(int y);

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
unsigned long gameStartTime;

enum GameState { STATE_START, STATE_PLAY, STATE_GAMEOVER };
GameState gameState;

int   gameOverPixels;
float gameOverAccum;
CRGB  gameOverColor;
int   gameOverRound;
bool  gameOverScreenCleared;

unsigned long lastFrameTime;

// Verhindert sofortigen Start nach Boot
bool startScreenJustShown = false;

// 4x7 Ziffern Font (0-9)
const uint8_t FONT_4x7[10][7] PROGMEM = {
  {0b1110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b1110}, // 0
  {0b0100, 0b1100, 0b0100, 0b0100, 0b0100, 0b0100, 0b1110}, // 1
  {0b1110, 0b0001, 0b0001, 0b1110, 0b1000, 0b1000, 0b1111}, // 2
  {0b1110, 0b0001, 0b0001, 0b0110, 0b0001, 0b0001, 0b1110}, // 3
  {0b1001, 0b1001, 0b1001, 0b1111, 0b0001, 0b0001, 0b0001}, // 4
  {0b1111, 0b1000, 0b1000, 0b1110, 0b0001, 0b0001, 0b1110}, // 5
  {0b0110, 0b1000, 0b1000, 0b1110, 0b1001, 0b1001, 0b0110}, // 6
  {0b1111, 0b0001, 0b0010, 0b0100, 0b0100, 0b0100, 0b0100}, // 7
  {0b0110, 0b1001, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110}, // 8
  {0b0110, 0b1001, 0b1001, 0b0111, 0b0001, 0b0001, 0b0110}  // 9
};

// 3x5 Font für "Sek." (S, e, k, .)
const uint8_t FONT_SEK[4][5] PROGMEM = {
  {0b111, 0b100, 0b111, 0b001, 0b111}, // S
  {0b111, 0b101, 0b111, 0b100, 0b111}, // e
  {0b101, 0b110, 0b100, 0b110, 0b101}, // k
  {0b000, 0b000, 0b000, 0b000, 0b010}  // .
};

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1500); // ESP boot stabilisieren
  Serial.println("[4Pong] Setup gestartet");

  WiFi.mode(WIFI_OFF);
  delay(100);
  // KEIN wdtDisable() – kann Boot blockieren

  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN,  INPUT_PULLUP);
  pinMode(BTN_UP,    INPUT_PULLUP);
  pinMode(SEEDPIN, INPUT);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);

  //Startanimation
  olb_v1_playIntro(leds);

  FastLED.setBrightness(BRIGHTNESS);

  // Verbesserter Random Seed
  uint32_t seed = 0;
  for (int i = 0; i < 32; i++) {
    seed ^= (uint32_t)analogRead(A0) << (i % 16);
    delay(1);
  }
  for (int i = 0; i < 16; i++) {
    seed ^= (uint32_t)digitalRead(SEEDPIN) << i;
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
        lastSpeedUp = millis();  // Zeit speichern
        Serial.println("[GAMEOVER] Screen geleert, starte Score-Animation");
      } else if (millis() - lastSpeedUp > 4000) {  // 4 Sekunden warten
        if (anyButtonPressed()) {
          Serial.println("[GAMEOVER] Taste gedrückt -> Startbildschirm");
          showStartScreen();
          gameState = STATE_START;
          startScreenJustShown = true;
        } else {
          drawGameOverScore();
          FastLED.show();
        }
      } else {
        drawGameOverScore();
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

void showStartScreen() {
  Serial.println("[4Pong] Zeige integrierten Startbildschirm...");
  for (int y = 0; y < MATRIX_H; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      uint32_t color = pgm_read_dword(&SCREEN_START[y][MATRIX_W - 1 - x]);
      uint8_t r = (color >> 16) & 0xFF;
      uint8_t g = (color >> 8)  & 0xFF;
      uint8_t b =  color        & 0xFF;
      int idx = xyToIndex(x, y);
      if (idx >= 0) leds[idx] = CRGB(r, g, b);
    }
  }
  FastLED.show();
  Serial.println("[4Pong] Startbildschirm angezeigt");
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
  gameStartTime = millis();
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
    score = (millis() - gameStartTime) / 1000;
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
    float rad2 = ballAngle * PI / 180.0f;
    ballX = nx + cos(rad2);
    ballY = ny + sin(rad2);
    Serial.println("[BALL] Schläger getroffen!");
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
// GAME OVER ANIMATION & SCORE ANZEIGE
// ============================================================
CRGB getGradientColor(int y) {
  // Verlauf der 4 Schlägerfarben von oben nach unten (y: 0 bis 15)
  // Grün -> Gelb -> Blau -> Weiß
  if (y <= 5) {
    fract8 f = (y * 255) / 5;
    return blend(COL_GREEN, COL_YELLOW, f);
  } else if (y <= 10) {
    fract8 f = ((y - 5) * 255) / 5;
    return blend(COL_YELLOW, COL_BLUE, f);
  } else {
    fract8 f = ((y - 10) * 255) / 5;
    return blend(COL_BLUE, COL_WHITE, f);
  }
}

void drawGameOverScore() {
  clearMatrix();

  char scoreStr[16];
  itoa(score, scoreStr, 10);
  int len = strlen(scoreStr);
  int totalWidth = len * 4 + (len - 1); // 4px Breite pro Ziffer + 1px Abstand

  int startX = 0;
  int startY = 1; // Ziffern oben (Y: 1 bis 7)

  if (totalWidth <= MATRIX_W) {
    startX = (MATRIX_W - totalWidth) / 2; // Horizontal zentriert
  } else {
    // Wenn Score breiter als 16 Pixel ist: sanftes horizontales Scrollen
    int scrollRange = totalWidth - MATRIX_W + 4;
    int scrollPos = (millis() / 250) % (scrollRange * 2);
    if (scrollPos >= scrollRange) {
      scrollPos = scrollRange * 2 - scrollPos;
    }
    startX = 2 - scrollPos;
  }

  // Ziffern zeichnen
  for (int i = 0; i < len; i++) {
    int digit = scoreStr[i] - '0';
    int curX = startX + i * 5;

    for (int row = 0; row < 7; row++) {
      uint8_t rowData = pgm_read_byte(&FONT_4x7[digit][row]);
      for (int col = 0; col < 4; col++) {
        if (rowData & (1 << (3 - col))) {
          // X-Achse gespiegelt korrigiert (Matrix X: MATRIX_W - 1 - px)
          int px = MATRIX_W - 1 - (curX + col);
          int py = startY + row;
          if (px >= 0 && px < MATRIX_W && py >= 0 && py < MATRIX_H) {
            int idx = xyToIndex(px, py);
            if (idx >= 0) leds[idx] = getGradientColor(py);
          }
        }
      }
    }
  }

  // "Sek." unten zeichnen (Y: 10 bis 14)
  int sekStartY = 10;
  int sekStartX = 1; // Startet bei X=1, passt perfekt für "Sek." (13px breit)
  for (int charIdx = 0; charIdx < 4; charIdx++) {
    int curX = sekStartX + charIdx * 4;
    for (int row = 0; row < 5; row++) {
      uint8_t rowData = pgm_read_byte(&FONT_SEK[charIdx][row]);
      for (int col = 0; col < 3; col++) {
        if (rowData & (1 << (2 - col))) {
          // X-Achse gespiegelt korrigiert
          int px = MATRIX_W - 1 - (curX + col);
          int py = sekStartY + row;
          if (px >= 0 && px < MATRIX_W && py >= 0 && py < MATRIX_H) {
            int idx = xyToIndex(px, py);
            if (idx >= 0) leds[idx] = getGradientColor(py);
          }
        }
      }
    }
  }
}
