// Treiber: "NodeMCU 1.0 (ESP-12E Module)"
/*****************************************************
 * NANO HEXAGON
 * openLUXbox 16x16 Matrix
 *****************************************************/

#include <FastLED.h>
#include "olb_init_v1.h"

/********************** DEFINES **********************/

#define MATRIX_W 16
#define MATRIX_H 16
#define NUM_LEDS 256
#define DATA_PIN 2
#define BRIGHTNESS 51   // 20% gem. TSD

#define COLOR_PLAYER CRGB::Green
#define COLOR_BAR    CRGB::Red
#define COLOR_SCORE  CRGB(0, 40, 0)   // Dunkelgrün

#if defined(ESP8266)
    #define BTN_LEFT  D5    // GPIO 14 (Gelb)
    #define BTN_RIGHT D2    // GPIO 4  (Grün)
    #define BTN_DOWN  D1    // GPIO 5  (Weiß)
    #define BTN_UP    D6    // GPIO 12 (Blau)
#elif defined(ESP32)
    #define BTN_LEFT  23    // Gelb
    #define BTN_RIGHT 19    // Grün
    #define BTN_DOWN  26    // Weiß
    #define BTN_UP    18    // Blau
#endif

#define DEBOUNCE_TIME 30

#define START_SPEED 1.0
#define SPEED_STEP 0.5
#define SPEED_INTERVAL 5000   // alle 5s

#define BAR_MIN 3
#define BAR_MAX 8

#define BAR_DELAY 2000        // Zeitversetzter Start

#define EXPLOSION_TIME 2000
#define COLLAPSE_TIME 1000
#define COLLAPSE_STEPS 8

/*********************** STRUCTS *********************/

struct Button {
  int pin;
  bool stable, lastRaw;
  unsigned long lastChange;
};

struct Bar {
  int dir;
  int pos, offset, width;
  bool active;
  unsigned long startTime;
};

struct Particle {
  float x, y, vx, vy;
};

/******************** PROTOTYPEN *********************/

int XY(int, int);
bool readButton(Button&);

void drawChar3x5(char c, int startX, int startY, uint8_t brightness, int gradMinY, int gradMaxY);
void drawTextNANO(uint8_t brightness);
void drawTextHEXAGON(uint8_t brightness);
void drawScoreScreen(int score, uint8_t brightness);
void showNanoHexagonIntro();
void playCollapseAnimation();

void startGame();
void playLoop();
void explodeLoop();
void scoreLoop();
void waitLoop();
void resetLoop();

void handleButtons();
void updateBars();
void drawGame();
bool checkCollision();
void startExplosion();

/******************** VARIABLEN **********************/

CRGB leds[NUM_LEDS];
Button btns[4];
Bar hbars[4];
Particle parts[10];

int playerX = 7, playerY = 7;

unsigned long gameStart, lastMove, lastSpeedUp, explodeStart;
float speed;

int partCount;
int collapseStep;
unsigned long lastCollapse;

enum State { PLAY, EXPLODE, SCORE, WAIT_INPUT, RESET };
State state;

/*********************** SETUP ***********************/

void setup() {
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // 1. oLb Startanimation
  olb_v1_playIntro(leds);

  // 1 Sekunde Pause nach olb_init
  FastLED.clear();
  FastLED.show();
  delay(1000);

  // 2. Nano Hexagon Intro
  showNanoHexagonIntro();

  // 3. Dunkelgrüne Schrumpf-Animation auch vor dem allerersten Spiel
  playCollapseAnimation();

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);

  btns[0] = {BTN_LEFT, HIGH, HIGH, 0};
  btns[1] = {BTN_RIGHT, HIGH, HIGH, 0};
  btns[2] = {BTN_DOWN, HIGH, HIGH, 0};
  btns[3] = {BTN_UP, HIGH, HIGH, 0};

  randomSeed(analogRead(A0) ^ ESP.getCycleCount());
  startGame();
}

/************************ LOOP ***********************/

void loop() {
  if (state == PLAY) playLoop();
  if (state == EXPLODE) explodeLoop();
  if (state == SCORE) scoreLoop();
  if (state == WAIT_INPUT) waitLoop();
  if (state == RESET) resetLoop();
}

/******************* INTRO & SCORE SCHRIFT ***********/

// Zeichnet ein 3x5 Zeichen mit vertikalem Farbverlauf von COLOR_PLAYER nach COLOR_BAR
void drawChar3x5(char c, int startX, int startY, uint8_t brightness, int gradMinY, int gradMaxY) {
  uint8_t glyph[5] = {0, 0, 0, 0, 0};

  switch (c) {
    // Buchstaben
    case 'N': glyph[0] = 0b101; glyph[1] = 0b111; glyph[2] = 0b101; glyph[3] = 0b101; glyph[4] = 0b101; break;
    case 'A': glyph[0] = 0b010; glyph[1] = 0b101; glyph[2] = 0b111; glyph[3] = 0b101; glyph[4] = 0b101; break;
    case 'O': glyph[0] = 0b111; glyph[1] = 0b101; glyph[2] = 0b101; glyph[3] = 0b101; glyph[4] = 0b111; break;
    case 'H': glyph[0] = 0b101; glyph[1] = 0b101; glyph[2] = 0b111; glyph[3] = 0b101; glyph[4] = 0b101; break;
    case 'E': glyph[0] = 0b111; glyph[1] = 0b100; glyph[2] = 0b111; glyph[3] = 0b100; glyph[4] = 0b111; break;
    case 'X': glyph[0] = 0b101; glyph[1] = 0b101; glyph[2] = 0b010; glyph[3] = 0b101; glyph[4] = 0b101; break;
    case 'G': glyph[0] = 0b111; glyph[1] = 0b100; glyph[2] = 0b101; glyph[3] = 0b101; glyph[4] = 0b111; break;
    case 'S': glyph[0] = 0b111; glyph[1] = 0b100; glyph[2] = 0b111; glyph[3] = 0b001; glyph[4] = 0b111; break;
    case 'K': glyph[0] = 0b101; glyph[1] = 0b110; glyph[2] = 0b100; glyph[3] = 0b110; glyph[4] = 0b101; break;

    // Ziffern
    case '0': glyph[0] = 0b111; glyph[1] = 0b101; glyph[2] = 0b101; glyph[3] = 0b101; glyph[4] = 0b111; break;
    case '1': glyph[0] = 0b010; glyph[1] = 0b110; glyph[2] = 0b010; glyph[3] = 0b010; glyph[4] = 0b111; break;
    case '2': glyph[0] = 0b111; glyph[1] = 0b001; glyph[2] = 0b111; glyph[3] = 0b100; glyph[4] = 0b111; break;
    case '3': glyph[0] = 0b111; glyph[1] = 0b001; glyph[2] = 0b111; glyph[3] = 0b001; glyph[4] = 0b111; break;
    case '4': glyph[0] = 0b101; glyph[1] = 0b101; glyph[2] = 0b111; glyph[3] = 0b001; glyph[4] = 0b001; break;
    case '5': glyph[0] = 0b111; glyph[1] = 0b100; glyph[2] = 0b111; glyph[3] = 0b001; glyph[4] = 0b111; break;
    case '6': glyph[0] = 0b111; glyph[1] = 0b100; glyph[2] = 0b111; glyph[3] = 0b101; glyph[4] = 0b111; break;
    case '7': glyph[0] = 0b111; glyph[1] = 0b001; glyph[2] = 0b010; glyph[3] = 0b010; glyph[4] = 0b010; break;
    case '8': glyph[0] = 0b111; glyph[1] = 0b101; glyph[2] = 0b111; glyph[3] = 0b101; glyph[4] = 0b111; break;
    case '9': glyph[0] = 0b111; glyph[1] = 0b101; glyph[2] = 0b111; glyph[3] = 0b001; glyph[4] = 0b111; break;
  }

  for (int row = 0; row < 5; row++) {
    int curY = startY + row;
    if (curY < 0 || curY > 15) continue;

    // Vertikaler Farbverlauf von Grün nach Rot
    uint8_t blendAmount = map(curY, gradMinY, gradMaxY, 0, 255);
    CRGB pixelColor = blend(COLOR_PLAYER, COLOR_BAR, blendAmount);
    pixelColor.nscale8_video(brightness);

    for (int col = 0; col < 3; col++) {
      if (glyph[row] & (1 << (2 - col))) {
        int curX = startX + col;
        if (curX >= 0 && curX < 16) {
          leds[XY(curX, curY)] = pixelColor;
        }
      }
    }
  }
}

void drawTextNANO(uint8_t brightness) {
  drawChar3x5('N', 0, 5, brightness, 5, 9);
  drawChar3x5('A', 4, 5, brightness, 5, 9);
  drawChar3x5('N', 8, 5, brightness, 5, 9);
  drawChar3x5('O', 12, 5, brightness, 5, 9);
}

void drawTextHEXAGON(uint8_t brightness) {
  drawChar3x5('H', 0, 2, brightness, 2, 13);
  drawChar3x5('E', 4, 2, brightness, 2, 13);
  drawChar3x5('X', 8, 2, brightness, 2, 13);
  drawChar3x5('A', 12, 2, brightness, 2, 13);

  drawChar3x5('G', 2, 9, brightness, 2, 13);
  drawChar3x5('O', 6, 9, brightness, 2, 13);
  drawChar3x5('N', 10, 9, brightness, 2, 13);
}

void drawScoreScreen(int score, uint8_t brightness) {
  char buf[8];
  sprintf(buf, "%d", score);
  int len = strlen(buf);

  // Zahl zentrieren in Zeile oben (Y=2..6)
  int startX = (16 - (len * 4 - 1)) / 2;
  if (startX < 0) startX = 0;

  for (int i = 0; i < len; i++) {
    drawChar3x5(buf[i], startX + (i * 4), 2, brightness, 2, 13);
  }

  // "SEK" zentriert in Zeile unten (Y=9..13)
  drawChar3x5('S', 2, 9, brightness, 2, 13);
  drawChar3x5('E', 6, 9, brightness, 2, 13);
  drawChar3x5('K', 10, 9, brightness, 2, 13);
}

void showNanoHexagonIntro() {
  for (int b = 0; b <= 255; b += 8) {
    FastLED.clear();
    drawTextNANO(b);
    FastLED.show();
    delay(20);
  }
  delay(400);
  for (int b = 255; b >= 0; b -= 8) {
    FastLED.clear();
    drawTextNANO(b);
    FastLED.show();
    delay(20);
  }

  delay(250);

  for (int b = 0; b <= 255; b += 8) {
    FastLED.clear();
    drawTextHEXAGON(b);
    FastLED.show();
    delay(20);
  }
  delay(500);
  for (int b = 255; b >= 0; b -= 8) {
    FastLED.clear();
    drawTextHEXAGON(b);
    FastLED.show();
    delay(20);
  }

  delay(300);
  FastLED.clear();
  FastLED.show();
}

// Führt die Schrumpf-Animation synchron aus (z.B. nach dem Intro)
void playCollapseAnimation() {
  for (int s = 0; s < COLLAPSE_STEPS; s++) {
    FastLED.clear();
    for (int y = s; y < 16 - s; y++) {
      for (int x = s; x < 16 - s; x++) {
        leds[XY(x, y)] = COLOR_SCORE;
      }
    }
    FastLED.show();
    delay(COLLAPSE_TIME / COLLAPSE_STEPS);
  }

  FastLED.clear();
  leds[XY(7, 7)] = COLOR_PLAYER;
  FastLED.show();
  delay(300);
}

/******************** GAME START *********************/

void startGame() {
  playerX = 7;
  playerY = 7;

  speed = START_SPEED;
  gameStart = millis();
  lastSpeedUp = millis();
  lastMove = millis();

  for (int i = 0; i < 4; i++) {
    hbars[i].dir = i;
    hbars[i].active = (i == 0);
    hbars[i].pos = 0;
    hbars[i].width = random(BAR_MIN, BAR_MAX + 1);
    hbars[i].offset = random(0, 16 - hbars[i].width);
    hbars[i].startTime = gameStart + (i * BAR_DELAY);
  }

  state = PLAY;
}

/*********************** PLAY ************************/

void playLoop() {
  if (millis() - lastSpeedUp > SPEED_INTERVAL) {
    speed += SPEED_STEP;
    lastSpeedUp = millis();
  }

  handleButtons();
  updateBars();

  if (checkCollision()) {
    startExplosion();
    return;
  }

  drawGame();
}

/********************* BUTTONS ***********************/

bool readButton(Button &b) {
  bool raw = digitalRead(b.pin);

  if (raw != b.lastRaw) {
    b.lastChange = millis();
    b.lastRaw = raw;
  }

  if (millis() - b.lastChange > DEBOUNCE_TIME) {
    if (raw != b.stable) {
      b.stable = raw;
      if (b.stable == LOW) return true;
    }
  }
  return false;
}

void handleButtons() {
  if (readButton(btns[0]) && playerX > 0)  playerX--;
  if (readButton(btns[1]) && playerX < 15) playerX++;
  if (readButton(btns[2]) && playerY < 15) playerY++;
  if (readButton(btns[3]) && playerY > 0)  playerY--;
}

/*********************** BARS ************************/

void updateBars() {
  unsigned long interval = 1000 / speed;
  if (millis() - lastMove < interval) return;
  lastMove = millis();

  for (int i = 0; i < 4; i++) {
    if (!hbars[i].active) {
      if (millis() > hbars[i].startTime)
        hbars[i].active = true;
      else
        continue;
    }

    hbars[i].pos++;

    if (hbars[i].pos > 15) {
      hbars[i].pos = 0;
      hbars[i].width = random(BAR_MIN, BAR_MAX + 1);
      hbars[i].offset = random(0, 16 - hbars[i].width);
    }
  }
}

/*********************** RENDER **********************/

int XY(int x, int y) {
  if (y % 2 == 0) return y * 16 + (15 - x);
  return y * 16 + x;
}

void drawGame() {
  FastLED.clear();

  unsigned long now = millis();

  for (int i = 0; i < 4; i++) {
    if (!hbars[i].active) continue;

    for (int w = 0; w < hbars[i].width; w++) {
      int x, y;

      if (hbars[i].dir == 0) { x = hbars[i].offset + w; y = hbars[i].pos; }
      if (hbars[i].dir == 1) { x = hbars[i].offset + w; y = 15 - hbars[i].pos; }
      if (hbars[i].dir == 2) { x = hbars[i].pos; y = hbars[i].offset + w; }
      if (hbars[i].dir == 3) { x = 15 - hbars[i].pos; y = hbars[i].offset + w; }

      // Flammen-/Wabereffekt:
      // Moduliert Helligkeit & Rot/Orange-Nuance basierend auf Zeit und Pixel-Position
      uint8_t wave = sin8((now / 6) + (x * 25) + (y * 35));
      uint8_t redVal = map(wave, 0, 255, 90, 255);    // Dunkelrot bis sattes Hellrot
      uint8_t greenVal = map(wave, 0, 255, 0, 28);    // Leichtes Glühen bei Spitzen

      leds[XY(x, y)] = CRGB(redVal, greenVal, 0);
    }
  }

  leds[XY(playerX, playerY)] = COLOR_PLAYER;
  FastLED.show();
}

/********************* COLLISION *********************/

bool checkCollision() {
  for (int i = 0; i < 4; i++) {
    if (!hbars[i].active) continue;

    for (int w = 0; w < hbars[i].width; w++) {
      int bx, by;

      if (hbars[i].dir == 0) { bx = hbars[i].offset + w; by = hbars[i].pos; }
      if (hbars[i].dir == 1) { bx = hbars[i].offset + w; by = 15 - hbars[i].pos; }
      if (hbars[i].dir == 2) { bx = hbars[i].pos; by = hbars[i].offset + w; }
      if (hbars[i].dir == 3) { bx = 15 - hbars[i].pos; by = hbars[i].offset + w; }

      if (bx == playerX && by == playerY)
        return true;
    }
  }
  return false;
}

/********************* EXPLOSION *********************/

void startExplosion() {
  partCount = random(6, 8);

  for (int i = 0; i < partCount; i++) {
    parts[i].x = playerX;
    parts[i].y = playerY;
    parts[i].vx = random(-10, 10) / 10.0;
    parts[i].vy = random(-10, 10) / 10.0;
  }

  explodeStart = millis();
  state = EXPLODE;
}

void explodeLoop() {
  FastLED.clear();

  float fade = (millis() - explodeStart) * 255.0 / EXPLOSION_TIME;

  for (int i = 0; i < partCount; i++) {
    parts[i].x += parts[i].vx;
    parts[i].y += parts[i].vy;

    int x = parts[i].x;
    int y = parts[i].y;

    if (x < 0 || x > 15 || y < 0 || y > 15) continue;

    CRGB c = COLOR_PLAYER;
    c.fadeToBlackBy(fade);
    leds[XY(x, y)] = c;
  }

  FastLED.show();

  if (millis() - explodeStart > EXPLOSION_TIME)
    state = SCORE;
}

/*********************** SCORE ***********************/

void scoreLoop() {
  // Kurze Pause nach der Explosion (500ms)
  FastLED.clear();
  FastLED.show();
  delay(300);

  int score = (explodeStart - gameStart) / 1000;

  // Sanftes Einblenden der Score-Anzeige
  for (int b = 0; b <= 255; b += 15) {
    FastLED.clear();
    drawScoreScreen(score, b);
    FastLED.show();
    delay(20);
  }

  // Display bleibt mit vollem Score stehen
  FastLED.clear();
  drawScoreScreen(score, 255);
  FastLED.show();

  // Taster-Zustände vor dem Warten synchronisieren (verhindert sofortiges Überspringen)
  for (int i = 0; i < 4; i++) {
    btns[i].lastRaw = digitalRead(btns[i].pin);
    btns[i].stable = btns[i].lastRaw;
    btns[i].lastChange = millis();
  }

  state = WAIT_INPUT;
}

/******************* WAIT FOR INPUT ******************/

void waitLoop() {
  for (int i = 0; i < 4; i++) {
    if (readButton(btns[i])) {
      collapseStep = 0;
      lastCollapse = millis();
      state = RESET;
    }
  }
}

/*********************** RESET ***********************/

void resetLoop() {
  if (millis() - lastCollapse < (COLLAPSE_TIME / COLLAPSE_STEPS))
    return;

  lastCollapse = millis();
  FastLED.clear();

  int s = collapseStep;

  for (int y = s; y < 16 - s; y++) {
    for (int x = s; x < 16 - s; x++) {
      leds[XY(x, y)] = COLOR_SCORE;
    }
  }

  FastLED.show();
  collapseStep++;

  if (collapseStep >= COLLAPSE_STEPS) {
    FastLED.clear();
    leds[XY(7, 7)] = COLOR_PLAYER;
    FastLED.show();
    delay(300);

    startGame();
  }
}
