// Arduino IDE Board: "NodeMCU 1.0 (ESP-12E Module)"
// oLb – Team Tetris
//
// Vier Spieler teilen sich einen Tetris-Stein:
//   D5 / gelb / links   = nach links
//   D2 / grün / rechts  = nach rechts
//   D6 / blau / hoch    = drehen
//   D1 / weiß / unten   = abbremsen
//
// Taster: GPIO nach GND, INPUT_PULLUP, gedrückt = LOW.
// Matrix: WS2812B 16x16, Serpentinenlayout, Start oben rechts.
// Spielfeld: 12 Spalten breit, zentriert von x=2 bis x=13.
//
// Bilddaten sind logisch gespeichert: oben links beginnend,
// zeilenweise von links nach rechts.
//
// Benötigt: FastLED und olb_init_v1.h
// A0 bleibt unbeschaltet; keine externe Spannung an A0 anlegen.

#include <Arduino.h>
#include <FastLED.h>
#include <string.h>
#include "olb_init_v1.h"

// ----------------------------- Defines ---------------------------------

#define DATA_PIN      D4
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB

static const uint8_t MATRIX_W = 16;
static const uint8_t MATRIX_H = 16;
static const uint16_t NUM_LEDS = MATRIX_W * MATRIX_H;
static const uint8_t BRIGHTNESS = 51;  // ca. 20 %

static const uint8_t FIELD_W = 12;
static const uint8_t FIELD_X = 2;

static const uint8_t PIN_LEFT  = D5;
static const uint8_t PIN_RIGHT = D2;
static const uint8_t PIN_DOWN  = D1;
static const uint8_t PIN_UP    = D6;

static const uint16_t DEBOUNCE_MS = 30;
static const uint16_t INTRO_FRAME_MS = 45;
static const uint16_t CLEAR_FLASH_MS = 90;
static const uint8_t CLEAR_FLASH_COUNT = 4;
static const uint32_t MATRIX_SERIAL_INTERVAL_MS = 4000UL;

// ---------------------- Difficulty tuning (edit here) ------------------

static const uint16_t DIFFICULTY_START_DROP_MS = 300;  // Fallintervall zu Beginn des Spiels in Millisekunden.
static const uint16_t DIFFICULTY_MIN_DROP_MS = 75;    // Kürzestes normales Fallintervall bei hoher Schwierigkeit.
static const uint16_t DIFFICULTY_DROP_LEVEL_STEP_MS = 18; // Um so viele Millisekunden sinkt das normale Intervall pro Level.

static const uint16_t DIFFICULTY_BRAKE_STEP_MS = 55;  // So viele Millisekunden verlängert ein Druck auf Weiß das Fallintervall.

static const uint16_t DIFFICULTY_GRAVITY_TICK_MS = 120; // So oft prüft die Schwerkraft, ob sie das Intervall verkürzen soll.
static const uint16_t DIFFICULTY_GRAVITY_STEP_MS = 15;   // Um so viele Millisekunden verkürzt die Schwerkraft das Intervall pro Prüfung.

static const uint16_t DIFFICULTY_MAX_BRAKE_INTERVAL_MS = 900; // Höchstes erlaubtes Fallintervall beim Bremsen in Millisekunden.

static const uint8_t DIFFICULTY_LINES_PER_LEVEL = 4;
static const uint16_t DIFFICULTY_LINE_POINTS[5] = {
  0, 100, 300, 500, 800
};

static const uint32_t SCORE_COLOR_THRESHOLD = 1600UL;

// ------------------------------ Structs --------------------------------

struct Button {
  uint8_t pin;
  bool rawState;
  bool stableState;
  uint32_t changedAt;
  bool pressedEvent;
};

// --------------------------- Globals -----------------------------------

CRGB leds[NUM_LEDS];
uint8_t board[MATRIX_H][FIELD_W];

Button buttonLeft;
Button buttonRight;
Button buttonDown;
Button buttonUp;

enum GameState : uint8_t {
  STATE_INTRO,
  STATE_PLAYING,
  STATE_CLEARING,
  STATE_GAME_OVER
};

GameState gameState = STATE_INTRO;

uint8_t currentType = 0;
uint8_t currentRotation = 0;
int8_t pieceX = 4;
int8_t pieceY = 0;

uint8_t bag[7];
uint8_t bagIndex = 7;

uint32_t score = 0;
uint16_t linesCleared = 0;
uint8_t level = 0;

uint16_t currentDropInterval = DIFFICULTY_START_DROP_MS;
uint32_t lastDropAt = 0;
uint32_t lastGravityAt = 0;

uint32_t lastIntroFrameAt = 0;
uint32_t lastClearFlashAt = 0;
uint32_t gameOverStartedAt = 0;
uint32_t lastMatrixSerialAt = 0;

uint16_t introFrame = 0;
uint8_t clearFlashCount = 0;
bool clearVisible = true;
bool fullRows[MATRIX_H];

// ------------------------- Function prototypes -------------------------

void initButton(Button &button, uint8_t pin);
void updateButton(Button &button, uint32_t now);
void clearButtonEvents();

uint16_t olb_xy(uint8_t x, uint8_t y);
void clearLeds();
void setPixel(int8_t x, int8_t y, const CRGB &color);
void showFrame();

void startIntro();
void updateIntro(uint32_t now);
void drawIntro();
void drawIntroLetter(uint8_t letter, int8_t x, int8_t y,
                     const CRGB &color);
void drawIntroTitle();

void startGame();
void resetBoard();
void refillBag();
uint8_t nextPiece();
void spawnPiece();

uint16_t pieceRowMask(uint8_t type, uint8_t rotation, uint8_t row);
bool pieceFits(uint8_t type, uint8_t rotation, int8_t x, int8_t y);
bool movePiece(int8_t dx, int8_t dy);
void rotatePiece();
void brakePiece();
void lockPiece();
void findFullRows();
void finishClear();
void gameOver();

uint16_t baseDropInterval();
uint16_t maxDropInterval();

CRGB pieceColor(uint8_t type);
void drawPiece(uint8_t type, int8_t x, int8_t y, uint8_t rotation,
               const CRGB &color);
void drawScoreBar();
void drawSidePanels();
void drawGame();
void drawDigit(uint8_t digit, int8_t x, int8_t y, const CRGB &color);
void drawScoreDigits(uint32_t value, int8_t x, int8_t y,
                     uint8_t digitCount, const CRGB &color);
void drawGameOver(uint32_t now);

void printMatrixToSerial(uint32_t now);

// ------------------------------- Setup ---------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("oLb Team Tetris startet"));

  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);

  initButton(buttonLeft, PIN_LEFT);
  initButton(buttonRight, PIN_RIGHT);
  initButton(buttonDown, PIN_DOWN);
  initButton(buttonUp, PIN_UP);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  olb_v1_playIntro(leds);

  FastLED.clear();
  FastLED.show();
  delay(1000);

  startIntro();
}

// -------------------------------- Loop ---------------------------------

void loop() {
  const uint32_t now = millis();

  updateButton(buttonLeft, now);
  updateButton(buttonRight, now);
  updateButton(buttonDown, now);
  updateButton(buttonUp, now);

  switch (gameState) {
    case STATE_INTRO:
      updateIntro(now);
      break;

    case STATE_PLAYING:
      if (buttonLeft.pressedEvent) {
        movePiece(-1, 0);
      }

      if (buttonRight.pressedEvent) {
        movePiece(1, 0);
      }

      if (buttonUp.pressedEvent) {
        rotatePiece();
      }

      if (buttonDown.pressedEvent) {
        brakePiece();
      }

      // Die Schwerkraft zieht das Intervall allmählich zurück
      // zum normalen Wert. Sie verändert nicht den Fall-Timer.
      if ((uint32_t)(now - lastGravityAt) >=
          DIFFICULTY_GRAVITY_TICK_MS) {
        lastGravityAt = now;

        const uint16_t normalInterval = baseDropInterval();

        if (currentDropInterval > normalInterval) {
          const uint16_t difference =
              currentDropInterval - normalInterval;

          if (difference <= DIFFICULTY_GRAVITY_STEP_MS) {
            currentDropInterval = normalInterval;
          } else {
            currentDropInterval -= DIFFICULTY_GRAVITY_STEP_MS;
          }
        }
      }

      // Ein Schritt nach unten pro Fallintervall.
      // lastDropAt wird nur nach einem erfolgreichen Schritt gesetzt.
      // Die Bremse setzt diesen Timer ausdrücklich NICHT zurück.
      if (gameState == STATE_PLAYING &&
          (uint32_t)(now - lastDropAt) >= currentDropInterval) {
        if (movePiece(0, 1)) {
          lastDropAt = now;
        } else {
          lockPiece();
        }
      }

      if (gameState == STATE_PLAYING) {
        drawGame();
      }
      break;

    case STATE_CLEARING:
      if ((uint32_t)(now - lastClearFlashAt) >= CLEAR_FLASH_MS) {
        lastClearFlashAt = now;
        clearVisible = !clearVisible;
        ++clearFlashCount;

        if (clearFlashCount >= CLEAR_FLASH_COUNT * 2) {
          finishClear();
        } else {
          drawGame();
        }
      }
      break;

    case STATE_GAME_OVER:
      if (buttonLeft.pressedEvent || buttonRight.pressedEvent ||
          buttonDown.pressedEvent || buttonUp.pressedEvent) {
        startGame();
      } else {
        drawGameOver(now);
      }
      break;
  }

  // printMatrixToSerial(now);
  clearButtonEvents();
}

// ------------------------- Serial Matrix Output ------------------------

void printMatrixToSerial(uint32_t now) {
  if ((uint32_t)(now - lastMatrixSerialAt) <
      MATRIX_SERIAL_INTERVAL_MS) {
    return;
  }

  lastMatrixSerialAt = now;

  static const char hexDigits[] = "0123456789ABCDEF";

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      const CRGB &pixel = leds[olb_xy(x, y)];
      const uint8_t channels[3] = { pixel.r, pixel.g, pixel.b };

      for (uint8_t channel = 0; channel < 3; ++channel) {
        Serial.write(hexDigits[channels[channel] >> 4]);
        Serial.write(hexDigits[channels[channel] & 0x0F]);
      }

      if (x < MATRIX_W - 1) {
        Serial.write(' ');
      }
    }

    Serial.println();
  }
}

// --------------------------- Button Logic ------------------------------

void initButton(Button &button, uint8_t pin) {
  button.pin = pin;
  button.rawState = digitalRead(pin);
  button.stableState = button.rawState;
  button.changedAt = millis();
  button.pressedEvent = false;
}

void updateButton(Button &button, uint32_t now) {
  const bool reading = digitalRead(button.pin);

  if (reading != button.rawState) {
    button.rawState = reading;
    button.changedAt = now;
  }

  if (reading != button.stableState &&
      (uint32_t)(now - button.changedAt) >= DEBOUNCE_MS) {
    button.stableState = reading;

    if (button.stableState == LOW) {
      button.pressedEvent = true;
    }
  }
}

void clearButtonEvents() {
  buttonLeft.pressedEvent = false;
  buttonRight.pressedEvent = false;
  buttonDown.pressedEvent = false;
  buttonUp.pressedEvent = false;
}

// ------------------------- Matrix Rendering ----------------------------

uint16_t olb_xy(uint8_t x, uint8_t y) {
  if ((y & 1U) == 0) {
    return (uint16_t)y * MATRIX_W + (MATRIX_W - 1U - x);
  }

  return (uint16_t)y * MATRIX_W + x;
}

void clearLeds() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}

void setPixel(int8_t x, int8_t y, const CRGB &color) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) {
    return;
  }

  leds[olb_xy((uint8_t)x, (uint8_t)y)] = color;
}

void showFrame() {
  FastLED.show();
}

// -------------------------------- Intro --------------------------------

void startIntro() {
  gameState = STATE_INTRO;
  introFrame = 0;
  lastIntroFrameAt = millis();
  drawIntro();
}

void updateIntro(uint32_t now) {
  if (buttonLeft.pressedEvent || buttonRight.pressedEvent ||
      buttonDown.pressedEvent || buttonUp.pressedEvent) {
    startGame();
    return;
  }

  if ((uint32_t)(now - lastIntroFrameAt) >= INTRO_FRAME_MS) {
    lastIntroFrameAt = now;
    ++introFrame;
    drawIntro();
  }
}

void drawIntro() {
  clearLeds();

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      const uint8_t d =
          (uint8_t)(x + y + (uint8_t)(introFrame * 3U));
      const uint8_t hue = d * 7;
      const bool edge = (x == 0 || x == MATRIX_W - 1 ||
                         y == 0 || y == MATRIX_H - 1);

      if (edge || ((x + y + introFrame) % 5 == 0)) {
        setPixel(x, y, CHSV(hue, 255, edge ? 180 : 100));
      }
    }
  }

  for (uint8_t y = 2; y < 9; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      setPixel(x, y, CRGB(0, 0, 12));
    }
  }

  drawIntroTitle();
  showFrame();
}

void drawIntroLetter(uint8_t letter, int8_t x, int8_t y,
                     const CRGB &color) {
  static const uint8_t glyphs[6][5] = {
    { 0b100, 0b100, 0b100, 0b100, 0b111 }, // L
    { 0b111, 0b100, 0b110, 0b100, 0b111 }, // E
    { 0b110, 0b101, 0b101, 0b101, 0b110 }, // D
    { 0b110, 0b101, 0b110, 0b101, 0b101 }, // R
    { 0b111, 0b010, 0b010, 0b010, 0b111 }, // I
    { 0b111, 0b100, 0b111, 0b001, 0b111 }  // S
  };

  if (letter >= 6) {
    return;
  }

  for (uint8_t row = 0; row < 5; ++row) {
    for (uint8_t col = 0; col < 3; ++col) {
      if (glyphs[letter][row] & (1U << (2U - col))) {
        setPixel(x + col, y + row, color);
      }
    }
  }
}

void drawIntroTitle() {
  static const uint8_t title[6] = { 0, 1, 2, 3, 4, 5 };
  const int8_t titleWidth = 23;
  const uint16_t travel = MATRIX_W + titleWidth + 1U;

  const int8_t origin = (int8_t)MATRIX_W -
      (int8_t)((introFrame / 3U) % travel);

  for (uint8_t i = 0; i < 6; ++i) {
    const uint8_t hue = (uint8_t)(introFrame * 5U + i * 28U);

    drawIntroLetter(title[i], origin + (int8_t)(i * 4U), 3,
                    CHSV(hue, 255, 255));
  }
}

// ----------------------------- Game Setup ------------------------------

void startGame() {
  const uint32_t gameSeed = ESP.random() ^ micros() ^ millis();
  randomSeed(gameSeed);

  resetBoard();
  score = 0;
  linesCleared = 0;
  level = 0;
  bagIndex = 7;
  refillBag();

  currentDropInterval = DIFFICULTY_START_DROP_MS;
  gameState = STATE_PLAYING;
  spawnPiece();

  lastDropAt = millis();
  lastGravityAt = lastDropAt;

  if (gameState == STATE_PLAYING) {
    drawGame();
  }
}

void resetBoard() {
  memset(board, 0, sizeof(board));
  memset(fullRows, 0, sizeof(fullRows));
}

void refillBag() {
  for (uint8_t i = 0; i < 7; ++i) {
    bag[i] = i;
  }

  for (int8_t i = 6; i > 0; --i) {
    const uint8_t j = (uint8_t)random(i + 1);
    const uint8_t temp = bag[i];
    bag[i] = bag[j];
    bag[j] = temp;
  }

  bagIndex = 0;
}

uint8_t nextPiece() {
  if (bagIndex >= 7) {
    refillBag();
  }

  return bag[bagIndex++];
}

void spawnPiece() {
  currentType = nextPiece();
  currentRotation = 0;
  pieceX = 4;
  pieceY = 0;

  if (!pieceFits(currentType, currentRotation, pieceX, pieceY)) {
    gameOver();
    return;
  }

  currentDropInterval = baseDropInterval();

  // Jedes neue Teil startet mit einem frischen Fallintervall.
  lastDropAt = millis();
  lastGravityAt = lastDropAt;
  gameState = STATE_PLAYING;
}

// ---------------------------- Piece Shapes -----------------------------

uint16_t pieceRowMask(uint8_t type, uint8_t rotation, uint8_t row) {
  static const uint8_t base[7][4] = {
    { 0b0000, 0b1111, 0b0000, 0b0000 }, // I
    { 0b0110, 0b0110, 0b0000, 0b0000 }, // O
    { 0b0010, 0b0111, 0b0000, 0b0000 }, // T
    { 0b0110, 0b1100, 0b0000, 0b0000 }, // S
    { 0b1100, 0b0110, 0b0000, 0b0000 }, // Z
    { 0b1000, 0b1110, 0b0000, 0b0000 }, // J
    { 0b0010, 0b1110, 0b0000, 0b0000 }  // L
  };

  uint16_t result = 0;

  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      if ((base[type][y] & (1U << (3U - x))) == 0) {
        continue;
      }

      uint8_t rx = x;
      uint8_t ry = y;

      if (type != 1) {
        for (uint8_t r = 0; r < (rotation & 3U); ++r) {
          const uint8_t oldX = rx;
          rx = (uint8_t)(3U - ry);
          ry = oldX;
        }
      }

      if (ry == row) {
        result |= (uint16_t)1U << rx;
      }
    }
  }

  return result;
}

// ----------------------------- Gameplay --------------------------------

bool pieceFits(uint8_t type, uint8_t rotation, int8_t x, int8_t y) {
  for (uint8_t py = 0; py < 4; ++py) {
    const uint16_t mask = pieceRowMask(type, rotation, py);

    for (uint8_t px = 0; px < 4; ++px) {
      if ((mask & ((uint16_t)1U << px)) == 0) {
        continue;
      }

      const int8_t sx = x + (int8_t)px;
      const int8_t sy = y + (int8_t)py;

      if (sx < 0 || sx >= FIELD_W || sy >= MATRIX_H) {
        return false;
      }

      if (sy >= 0 && board[(uint8_t)sy][(uint8_t)sx] != 0) {
        return false;
      }
    }
  }

  return true;
}

bool movePiece(int8_t dx, int8_t dy) {
  const int8_t newX = pieceX + dx;
  const int8_t newY = pieceY + dy;

  if (!pieceFits(currentType, currentRotation, newX, newY)) {
    return false;
  }

  pieceX = newX;
  pieceY = newY;
  return true;
}

void rotatePiece() {
  if (currentType == 1) {
    return;
  }

  const uint8_t newRotation = (currentRotation + 1U) & 3U;

  if (pieceFits(currentType, newRotation, pieceX, pieceY)) {
    currentRotation = newRotation;
  } else if (pieceFits(currentType, newRotation, pieceX - 1, pieceY)) {
    --pieceX;
    currentRotation = newRotation;
  } else if (pieceFits(currentType, newRotation, pieceX + 1, pieceY)) {
    ++pieceX;
    currentRotation = newRotation;
  }
}

// Weiß verlängert nur das Fallintervall. Es setzt lastDropAt NICHT zurück.
// Dadurch kann schnelles Drücken bremsen, aber den Stein nicht anhalten.
void brakePiece() {
  const uint16_t normalInterval = baseDropInterval();
  const uint16_t ceiling = maxDropInterval();

  if (currentDropInterval < normalInterval) {
    currentDropInterval = normalInterval;
  }

  if (currentDropInterval < ceiling) {
    uint32_t slowed =
        (uint32_t)currentDropInterval + DIFFICULTY_BRAKE_STEP_MS;

    if (slowed > ceiling) {
      slowed = ceiling;
    }

    currentDropInterval = (uint16_t)slowed;
  }
}

void lockPiece() {
  bool aboveBoard = false;

  for (uint8_t py = 0; py < 4; ++py) {
    const uint16_t mask = pieceRowMask(currentType, currentRotation, py);

    for (uint8_t px = 0; px < 4; ++px) {
      if ((mask & ((uint16_t)1U << px)) == 0) {
        continue;
      }

      const int8_t sx = pieceX + (int8_t)px;
      const int8_t sy = pieceY + (int8_t)py;

      if (sy < 0) {
        aboveBoard = true;
      } else if (sx >= 0 && sx < FIELD_W && sy < MATRIX_H) {
        board[(uint8_t)sy][(uint8_t)sx] = currentType + 1;
      }
    }
  }

  if (aboveBoard) {
    gameOver();
    return;
  }

  findFullRows();

  bool found = false;

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    if (fullRows[y]) {
      found = true;
      break;
    }
  }

  if (found) {
    gameState = STATE_CLEARING;
    clearVisible = true;
    clearFlashCount = 0;
    lastClearFlashAt = millis();
    drawGame();
  } else {
    spawnPiece();
  }
}

void findFullRows() {
  memset(fullRows, 0, sizeof(fullRows));

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    bool full = true;

    for (uint8_t x = 0; x < FIELD_W; ++x) {
      if (board[y][x] == 0) {
        full = false;
        break;
      }
    }

    fullRows[y] = full;
  }
}

void finishClear() {
  uint8_t removedRows = 0;

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    if (fullRows[y]) {
      ++removedRows;
    }
  }

  int8_t writeY = MATRIX_H - 1;

  for (int8_t readY = MATRIX_H - 1; readY >= 0; --readY) {
    if (!fullRows[(uint8_t)readY]) {
      if (writeY != readY) {
        memcpy(board[(uint8_t)writeY], board[(uint8_t)readY], FIELD_W);
      }

      --writeY;
    }
  }

  while (writeY >= 0) {
    memset(board[(uint8_t)writeY], 0, FIELD_W);
    --writeY;
  }

  if (removedRows > 0) {
    const uint8_t index = removedRows > 4 ? 4 : removedRows;

    score += (uint32_t)DIFFICULTY_LINE_POINTS[index] * (level + 1U);
    linesCleared += removedRows;
    level = linesCleared / DIFFICULTY_LINES_PER_LEVEL;
  }

  memset(fullRows, 0, sizeof(fullRows));
  spawnPiece();
}

void gameOver() {
  gameState = STATE_GAME_OVER;
  gameOverStartedAt = millis();

  Serial.print(F("Game Over. Score: "));
  Serial.print(score);
  Serial.print(F(" | Reihen: "));
  Serial.println(linesCleared);
}

// --------------------------- Speed / Scoring ---------------------------

uint16_t baseDropInterval() {
  uint32_t interval = DIFFICULTY_START_DROP_MS;
  const uint32_t reduction =
      (uint32_t)level * DIFFICULTY_DROP_LEVEL_STEP_MS;

  if (reduction >=
      DIFFICULTY_START_DROP_MS - DIFFICULTY_MIN_DROP_MS) {
    interval = DIFFICULTY_MIN_DROP_MS;
  } else {
    interval -= reduction;
  }

  return (uint16_t)interval;
}

uint16_t maxDropInterval() {
  uint32_t result = (uint32_t)baseDropInterval() * 2U;

  if (result > DIFFICULTY_MAX_BRAKE_INTERVAL_MS) {
    result = DIFFICULTY_MAX_BRAKE_INTERVAL_MS;
  }

  return (uint16_t)result;
}

// ------------------------------ Drawing --------------------------------

CRGB pieceColor(uint8_t type) {
  switch (type) {
    case 0: return CRGB(30, 210, 255);
    case 1: return CRGB(255, 220, 20);
    case 2: return CRGB(190, 45, 255);
    case 3: return CRGB(50, 255, 90);
    case 4: return CRGB(255, 45, 65);
    case 5: return CRGB(50, 90, 255);
    default: return CRGB(255, 120, 20);
  }
}

void drawPiece(uint8_t type, int8_t x, int8_t y, uint8_t rotation,
               const CRGB &color) {
  for (uint8_t py = 0; py < 4; ++py) {
    const uint16_t mask = pieceRowMask(type, rotation, py);

    for (uint8_t px = 0; px < 4; ++px) {
      if (mask & ((uint16_t)1U << px)) {
        setPixel(FIELD_X + x + (int8_t)px, y + (int8_t)py, color);
      }
    }
  }
}

void drawScoreBar() {
  uint32_t unit;
  CRGB barColor;

  if (score > 10000UL) {
    unit = 10000UL;
    barColor = CRGB(255, 40, 180);
  } else if (score > 1000UL) {
    unit = 1000UL;
    barColor = CRGB(40, 100, 255);
  } else {
    unit = 100UL;
    barColor = CRGB(40, 255, 70);
  }

  uint32_t scorePixels = score / unit;

  if (scorePixels > MATRIX_H) {
    scorePixels = MATRIX_H;
  }

  for (uint8_t i = 0; i < scorePixels; ++i) {
    setPixel(0, MATRIX_H - 1 - i, barColor);
  }
}

void drawSidePanels() {
  drawScoreBar();

  uint8_t levelPixels = level + 1U;

  if (levelPixels > MATRIX_H) {
    levelPixels = MATRIX_H;
  }

  for (uint8_t i = 0; i < levelPixels; ++i) {
    setPixel(15, MATRIX_H - 1 - i, CHSV(160 + i * 7, 255, 220));
  }

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    setPixel(1, y, CRGB(20, 25, 45));
    setPixel(14, y, CRGB(20, 25, 45));
  }
}

void drawGame() {
  clearLeds();

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < FIELD_W; ++x) {
      if (board[y][x] != 0) {
        setPixel(FIELD_X + x, y, pieceColor(board[y][x] - 1));
      }
    }
  }

  if (gameState == STATE_CLEARING && clearVisible) {
    for (uint8_t y = 0; y < MATRIX_H; ++y) {
      if (!fullRows[y]) {
        continue;
      }

      for (uint8_t x = 0; x < FIELD_W; ++x) {
        setPixel(FIELD_X + x, y, CRGB::White);
      }
    }
  }

  if (gameState == STATE_PLAYING || gameState == STATE_CLEARING) {
    drawPiece(currentType, pieceX, pieceY, currentRotation,
              pieceColor(currentType));
  }

  drawSidePanels();
  showFrame();
}

void drawDigit(uint8_t digit, int8_t x, int8_t y, const CRGB &color) {
  static const uint8_t glyphs[10][5] = {
    { 0b111, 0b101, 0b101, 0b101, 0b111 },
    { 0b010, 0b110, 0b010, 0b010, 0b111 },
    { 0b111, 0b001, 0b111, 0b100, 0b111 },
    { 0b111, 0b001, 0b111, 0b001, 0b111 },
    { 0b101, 0b101, 0b111, 0b001, 0b001 },
    { 0b111, 0b100, 0b111, 0b001, 0b111 },
    { 0b111, 0b100, 0b111, 0b101, 0b111 },
    { 0b111, 0b001, 0b010, 0b010, 0b010 },
    { 0b111, 0b101, 0b111, 0b101, 0b111 },
    { 0b111, 0b101, 0b111, 0b001, 0b111 }
  };

  if (digit > 9) {
    return;
  }

  for (uint8_t row = 0; row < 5; ++row) {
    for (uint8_t col = 0; col < 3; ++col) {
      if (glyphs[digit][row] & (1U << (2U - col))) {
        setPixel(x + col, y + row, color);
      }
    }
  }
}

void drawScoreDigits(uint32_t value, int8_t x, int8_t y,
                     uint8_t digitCount, const CRGB &color) {
  uint32_t divisor = 1;

  for (uint8_t i = 1; i < digitCount; ++i) {
    divisor *= 10U;
  }

  for (uint8_t i = 0; i < digitCount; ++i) {
    const uint8_t digit = (uint8_t)((value / divisor) % 10U);

    drawDigit(digit, x + (int8_t)(i * 4U), y, color);

    if (divisor > 1) {
      divisor /= 10U;
    }
  }
}

void drawGameOver(uint32_t now) {
  clearLeds();

  const uint8_t phase = (uint8_t)((now - gameOverStartedAt) / 20U);

  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      const uint8_t hue = (uint8_t)(phase + x * 9U + y * 7U);
      const uint8_t value =
          ((x + y + phase / 4U) & 1U) ? 70 : 25;

      setPixel(x, y, CHSV(hue, 220, value));
    }
  }

  const CRGB scoreColor = (score > SCORE_COLOR_THRESHOLD)
      ? CRGB(255, 70, 25)
      : CHSV(phase + 80, 180, 255);

  if (score <= 9999UL) {
    drawScoreDigits(score, 0, 5, 4, scoreColor);
  } else {
    uint8_t digitCount = 1;
    uint32_t remaining = score;

    while (remaining >= 10U) {
      remaining /= 10U;
      ++digitCount;
    }

    uint32_t divisor = 1;

    for (uint8_t i = 1; i < digitCount; ++i) {
      divisor *= 10U;
    }

    const int16_t textWidth = (int16_t)digitCount * 4 - 1;
    const uint16_t travel = MATRIX_W + textWidth;
    const uint32_t scrollStep =
        (now - gameOverStartedAt) / 100U;

    const int16_t origin = (int16_t)MATRIX_W -
        (int16_t)(scrollStep % travel);

    uint32_t value = score;

    for (uint8_t i = 0; i < digitCount; ++i) {
      const uint8_t digit = (uint8_t)(value / divisor);

      drawDigit(digit, (int8_t)(origin + (int16_t)i * 4),
                5, scoreColor);

      value %= divisor;

      if (divisor > 1) {
        divisor /= 10U;
      }
    }
  }

  for (uint8_t x = 0; x < MATRIX_W; ++x) {
    setPixel(x, 0, CHSV(phase + x * 12U, 255, 180));
    setPixel(x, 15, CHSV(phase + x * 12U + 80, 255, 180));
  }

  showFrame();
}