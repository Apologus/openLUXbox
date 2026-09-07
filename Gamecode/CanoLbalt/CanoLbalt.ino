// NodeMCU 1.0 (ESP-12E Module) - Board: "NodeMCU 1.0 (ESP-12E Module)"
// CanoLbalt - Co-op Runner gemäß GSD 

#include <user_interface.h>
#include <FastLED.h>
#include "olb_init_v1.h"

// ============================================================================
// 1. HARDWARE- UND PIN-DEFINITIONEN
// ============================================================================
#define PIN_LED           D4
#define NUM_LEDS          256
#define MATRIX_WIDTH      16
#define MATRIX_HEIGHT     16
#define COLOR_ORDER       GRB
#define CHIPSET           WS2812B
#define GLOBAL_BRIGHTNESS 80 // ~20% Helligkeit gemäß GSD

#define PIN_BTN_GELB      14 // GPIO 14 (D5) - Spieler 0 (GELB)
#define PIN_BTN_GRUEN     4  // GPIO 4  (D2) - Spieler 1 (GRÜN)
#define PIN_BTN_WEISS     5  // GPIO 5  (D1) - Spieler 2 (WEISS)
#define PIN_BTN_BLAU      12 // GPIO 12 (D6) - Spieler 3 (BLAU)

#define NUM_PLAYERS       4

// ============================================================================
// 2. SPIELFLUSS & CONFIGURATION (ALLE FEATURE-SWITCHES AN EINEM ORT)
// ============================================================================
// --- DEV MODE SCHALTER ---
const bool DEV_MODE                   = false; // true = 4-Tasten-Cheat/Testmodus erlaubt, false = Exakt die 2 Duo-Tasten nötig

// --- BASE SPEED & PROGRESSION ---
float    SCROLL_SPEED_INIT_PX_SEC     = 6.0f;  // Startgeschwindigkeit (Pixel / Sekunde)
float    SCROLL_SPEED_MIN_PX_SEC      = 3.5f;  // Absolute Untergrenze
float    SCROLL_SPEED_MAX_PX_SEC      = 10.0f; // Absolute Obergrenze
uint32_t PROGRESSION_INTERVAL_MS      = 12000; // Alle X ms wird das Spiel schneller
float    PROGRESSION_SPEED_FACTOR     = 1.08f; // Geschwindigkeitssteigerung (+8% pro Stufe)

// --- LÜCKEN UND DÄCHER (DAS PURE SPRING-SPIEL) ---
uint8_t  DACH_START_MIN               = 12;    // Mindestlänge Dach (in Spalten)
uint8_t  DACH_START_MAX               = 20;    // Maximallänge Dach (in Spalten)
uint8_t  DACH_ABSOLUT_MIN             = 6;     // Minimale Dachlänge bei hoher Progression
uint8_t  LUECKEN_START_MIN            = 1;     // Minimale Lückenbreite 
uint8_t  LUECKEN_START_MAX            = 3;     // Maximale Lückenbreite 

// --- ZUSATZFEATURES (SETZE AUF 0, UM SIE VOLLSTÄNDIG DEAKTIVIEREN) ---
uint8_t  KISTEN_SPAWN_CHANCE_PERCENT  = 3;     // Chance (%) für Kisten auf Dächern pro Pixel (0 = AUS)
uint8_t  BRUECHIG_SPAWN_CHANCE_PERCENT= 0;     // Chance (%) für einstürzenden Boden (0 = AUS)
uint8_t  TAUBEN_SPAWN_CHANCE_PERCENT  = 0;     // Chance (%) für Tauben (0 = AUS)
uint8_t  GEFAHR_SPAWN_CHANCE_PERCENT  = 15;    // Chance (%) pro Sekunde für Trümmer (0 = AUS)

// --- TIMINGS UND FEINABSTIMMUNG ---
const uint32_t FRAME_DAUER_MS           = 25;    // 40 FPS Target
const uint32_t DUO_WINDOW_MS            = 400;   // Zeitfenster für gleichzeitiges Drücken
const uint32_t DUO_SWITCH_MS            = 8000;  // Neuer Duo-Wechsel-Intervall
const uint32_t DEBOUNCE_MS              = 30;    // Taster-Entprellzeit
const uint32_t BRUECHIG_VERZOEGERUNG_MS = 150;   // Verzögerung bis brüchiger Boden wegbricht
const uint32_t KISTEN_BREMSZEIT_MS      = 1500;  // Dauer Geschwindigkeitsverlust nach Kiste
const float    KISTEN_BREMSFAKTOR       = 0.60f; // Geschwindigkeit nach Kistenkollision (-40%)
const uint32_t GEFAHR_WARNDAUER_MS      = 600;   // Dauer Warnblinken vor Trümmereinschlag
const uint32_t GEFAHR_COOLDOWN_MS       = 5000;  // Mindestabstand zwischen Trümmern

// --- POSITIONEN (GSD KONFORM) ---
const uint8_t  FIGUR_X                  = 4;     // Feste X-Position der Spielfigur
const uint8_t  BODEN_Y                  = 14;    // Y-Position der Dachoberkante
const uint8_t  FIGUR_STAND_Y            = 13;    // Y-Position Figur stehend (Y 13)

const uint8_t  MAX_PARTIKEL             = 24;    // Max. aktive Partikel (GSD Limit)
const uint8_t  RING_BUFFER_SIZE         = 60;    // Ringpuffergröße (GSD fordert min. 40)
const uint8_t  BG_PATTERN_SIZE          = 64;    // Parallax-Musterlänge

// GSD Farbdefinitionen
const CRGB COLOR_BACKGROUND   = CRGB(0, 0, 0);
const CRGB COLOR_PLAYER       = CRGB(255, 255, 255);
const CRGB COLOR_BODEN        = CRGB(60, 60, 60);
const CRGB COLOR_BRUECHIG     = CRGB(180, 80, 0);
const CRGB COLOR_KISTE        = CRGB(160, 80, 10);
const CRGB COLOR_MISSILE      = CRGB(255, 30, 0);
const CRGB COLOR_PIGEON       = CRGB(150, 150, 150);

const CRGB PLAYER_COLORS[NUM_PLAYERS] = {
  CRGB(255, 255, 0),   // 0: GELB
  CRGB(0, 255, 0),     // 1: GRÜN
  CRGB(255, 255, 255), // 2: WEISS
  CRGB(0, 0, 255)      // 3: BLAU
};

const uint8_t BUTTON_PINS[NUM_PLAYERS] = {
  PIN_BTN_GELB, PIN_BTN_GRUEN, PIN_BTN_WEISS, PIN_BTN_BLAU
};

// Verbindliche Sprungtrajektorie gemäß GSD v3 (30 Frames), halbierte Höhe
const int8_t JUMP_OFFSETS[30] = {
  0, -1, -2, -3, -4, -4, -4, -4, -4, -4,
 -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,
 -4, -4, -3, -2, -2, -1, -1,  0,  0,  0
};

// Full 3x5 Bitmap Schrift für Ziffern 0-9 (GSD v3 Specification)
const uint16_t DIGITS_3X5[10] = {
  0x7B6F, // 0
  0x2C97, // 1
  0x73E7, // 2
  0x73CF, // 3
  0x5BC9, // 4
  0x79CF, // 5
  0x79EF, // 6
  0x7249, // 7
  0x7BEF, // 8
  0x7BCF  // 9
};

// ============================================================================
// GROSSES 5x8 PIXEL FONT SYSTEM FÜR INTRO (CANOLBALT)
// ============================================================================
struct TextChar5x8 {
  uint8_t rows[8];
  CRGB color;
};

const TextChar5x8 CANOLBALT_TEXT[] = {
  { {0x0F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0F}, CRGB(200, 200, 200) }, // C
  { {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x19, 0x19, 0x0F}, CRGB(200, 200, 200) }, // a
  { {0x00, 0x00, 0x1A, 0x19, 0x19, 0x19, 0x19, 0x19}, CRGB(200, 200, 200) }, // n
  { {0x00, 0x00, 0x0E, 0x19, 0x19, 0x19, 0x19, 0x0E}, CRGB(0, 255, 230) },   // o <-- NEON-CYAN
  { {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F}, CRGB(255, 230, 0) },   // L <-- GELB
  { {0x18, 0x18, 0x1A, 0x19, 0x19, 0x19, 0x19, 0x1E}, CRGB(0, 255, 230) },   // b <-- NEON-CYAN
  { {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x19, 0x19, 0x0F}, CRGB(200, 200, 200) }, // a
  { {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0E}, CRGB(200, 200, 200) }, // l
  { {0x08, 0x08, 0x1E, 0x08, 0x08, 0x08, 0x08, 0x06}, CRGB(200, 200, 200) }  // t
};
const uint8_t CANOLBALT_LEN = 9;

// ============================================================================
// 4. STRUKTUREN UND DATENHALTUNG
// ============================================================================
enum GameState { STATE_INTRO, STATE_START, STATE_COUNTDOWN, STATE_PLAYING, STATE_GAMEOVER };

struct Spalte {
  bool boden;
  bool bruechig;
  uint32_t bruechigBetretenZeit;
  bool kiste;
  bool taube;
  bool gefahr;
};

struct BackgroundSpalte {
  uint8_t hoeheY;  
  CRGB farbe;      
  uint16_t windowMask;
};

struct Partikel {
  bool aktiv;
  float x, y;
  float vx, vy;
  uint32_t lebensdauerEnde;
  CRGB farbe;
};

struct GefahrObjekt {
  bool aktiv;
  uint32_t weltSpalte;
  uint32_t spawnZeit;
  bool istGefallen;
  uint8_t aktuellY;
};

// Globale Systemvariablen
CRGB leds[NUM_LEDS];
GameState currentState = STATE_INTRO;

uint32_t lastFrameTime = 0;
uint32_t introStartTime = 0;
uint32_t lastDuoSwitchTime = 0;
uint32_t duoFirstPressTime = 0;
uint32_t gameStartTime = 0;
uint32_t gameOverStartTime = 0;
uint32_t lastProgressionTime = 0;
uint32_t lastGefahrCheckTime = 0;

uint8_t activeDuoPlayer1 = 0;
uint8_t activeDuoPlayer2 = 1;
int8_t  firstPressPlayer = -1;

bool btnStatePrev[NUM_PLAYERS] = {HIGH, HIGH, HIGH, HIGH};
uint32_t btnDebounceTimer[NUM_PLAYERS] = {0, 0, 0, 0};

Spalte ringBuffer[RING_BUFFER_SIZE];
BackgroundSpalte bgSkyline[BG_PATTERN_SIZE];

uint32_t maxGeneratedWeltSpalte = 0; // Höchste Weltspalte, die bisher generiert wurde
uint32_t currentWeltSpalte = 0;       // Weltspalte, die sich visuell bei x=0 befindet
float    scrollAccu = 0.0f;
float    bgScrollAccu = 0.0f;
uint32_t scoreMeters = 0;

float    currentSpeed = 3.5f;
uint8_t  currentDachMin = 8;
uint8_t  currentDachMax = 14;

bool    isJumping = false;
uint8_t jumpFrame = 0;
int8_t  playerY = FIGUR_STAND_Y;

uint32_t kistenBremseEnde = 0;
GefahrObjekt aktiveGefahr = {false, 0, 0, false, 0};

Partikel partikelPuffer[MAX_PARTIKEL];

int8_t   shakeOffsetX = 0;
int8_t   shakeOffsetY = 0;
uint32_t shakeEnde = 0;
uint8_t  shakeIntensitaet = 0;

// ============================================================================
// 5. HELFER-FUNKTIONEN & PROTOTYPEN
// ============================================================================
void resetGame();
void initBackgroundSkyline();
void updateInput();
void updatePhysicsAndWorld(uint32_t now);
void checkGefahrenSpawns(uint32_t now);
void applyProgression(uint32_t now);
void spawnPartikel(float x, float y, float vx, float vy, uint32_t dauerMs, CRGB farbe);
void triggerShake(uint32_t dauerMs, uint8_t intensitaet);
void drawDigit(uint8_t digit, int8_t offsetX, int8_t offsetY, CRGB color);
void drawChar5x8(const uint8_t rows[8], int8_t offsetX, int8_t offsetY, CRGB color);
void selectNewDuo();
void generateSingleColumn(uint32_t targetWeltSpalte);
void renderIntroScreen(uint32_t now);
void renderStartScreen();
void renderCountdownScreen(uint32_t now);
void renderGameScreen();
void renderGameOverScreen(uint32_t now);

// ============================================================================
// 6. SETUP & MAIN LOOP
// ============================================================================
void setup() {
  wifi_set_opmode(NULL_MODE);
  wifi_fpm_set_sleep_type(MODEM_SLEEP_T);
  wifi_fpm_open();
  wifi_fpm_do_sleep(0xFFFFFFF);

  for (uint8_t i = 0; i < NUM_PLAYERS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  FastLED.addLeds<CHIPSET, PIN_LED, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(GLOBAL_BRIGHTNESS);
  FastLED.clear();

  //Startanimation
  olb_v1_playIntro(leds);

  randomSeed(analogRead(A0) + micros());
  initBackgroundSkyline();

  for (uint8_t i = 0; i < MAX_PARTIKEL; i++) partikelPuffer[i].aktiv = false;
}

void loop() {
  uint32_t now = millis();
  if (now - lastFrameTime < FRAME_DAUER_MS) return;
  lastFrameTime = now;

  updateInput();

  switch (currentState) {
    case STATE_INTRO:
      renderIntroScreen(now);
      break;
    case STATE_START:
      renderStartScreen();
      break;
    case STATE_COUNTDOWN:
      if (now - gameStartTime >= 3000) {
        resetGame();
        currentState = STATE_PLAYING;
      } else {
        renderCountdownScreen(now);
      }
      break;
    case STATE_PLAYING:
      updatePhysicsAndWorld(now);
      renderGameScreen();
      break;
    case STATE_GAMEOVER:
      renderGameOverScreen(now);
      break;
  }

  FastLED.show();
}

// ============================================================================
// 7. LEVEL-GENERATOR & PARALLAX-INIT
// ============================================================================
void initBackgroundSkyline() {
  uint8_t col = 0;
  while (col < BG_PATTERN_SIZE) {
    uint8_t hausBreite = random(3, 7);
    uint8_t hausHoeheY = random(2, 8); 
    uint8_t shade = random(10, 25);
    CRGB hausFarbe = CRGB(shade / 2, shade / 2, shade);

    for (uint8_t b = 0; b < hausBreite && col < BG_PATTERN_SIZE; b++) {
      bgSkyline[col].hoeheY = hausHoeheY;
      bgSkyline[col].farbe = hausFarbe;
      bgSkyline[col].windowMask = 0;
      if (b % 2 == 0) {
        for (uint8_t y = hausHoeheY + 1; y <= 12; y += 2) {
          if (random(100) < 25) { 
            bgSkyline[col].windowMask |= (1 << y);
          }
        }
      }
      col++;
    }

    uint8_t luecke = random(1, 3);
    for (uint8_t l = 0; l < luecke && col < BG_PATTERN_SIZE; l++) {
      bgSkyline[col].hoeheY = 16; 
      bgSkyline[col].farbe = CRGB(0, 0, 0);
      bgSkyline[col].windowMask = 0;
      col++;
    }
  }
}

static uint8_t abschnittRest = 0;
static bool    isDachState = true;

void generateSingleColumn(uint32_t targetWeltSpalte) {
  uint8_t bufferIndex = targetWeltSpalte % RING_BUFFER_SIZE;

  // Garantiere 30 Spalten freie Startzone
  if (targetWeltSpalte < 20) {
    Spalte s;
    s.boden = true;
    s.bruechig = false;
    s.bruechigBetretenZeit = 0;
    s.kiste = false;
    s.taube = false;
    s.gefahr = false;
    ringBuffer[bufferIndex] = s;
    maxGeneratedWeltSpalte = targetWeltSpalte;
    return;
  }

  if (abschnittRest == 0) {
    isDachState = !isDachState;
    if (isDachState) {
      abschnittRest = random(currentDachMin, currentDachMax + 1);
    } else {
      abschnittRest = random(LUECKEN_START_MIN, LUECKEN_START_MAX + 1);
    }
  }

  abschnittRest--;

  Spalte s;
  s.bruechigBetretenZeit = 0;
  s.gefahr = false;

  if (isDachState) {
    s.boden = true;
    s.bruechig = (BRUECHIG_SPAWN_CHANCE_PERCENT > 0) && (random(100) < BRUECHIG_SPAWN_CHANCE_PERCENT);
    s.kiste    = (KISTEN_SPAWN_CHANCE_PERCENT > 0) && (!s.bruechig) && (random(100) < KISTEN_SPAWN_CHANCE_PERCENT);
    s.taube    = (TAUBEN_SPAWN_CHANCE_PERCENT > 0) && (!s.bruechig) && (!s.kiste) && (random(100) < TAUBEN_SPAWN_CHANCE_PERCENT);
  } else {
    s.boden    = false;
    s.bruechig = false;
    s.kiste    = false;
    s.taube    = false;
  }

  ringBuffer[bufferIndex] = s;
  maxGeneratedWeltSpalte = targetWeltSpalte;
}

void resetGame() {
  currentWeltSpalte = 0;
  maxGeneratedWeltSpalte = 0;
  scrollAccu = 0.0f;
  bgScrollAccu = 0.0f;
  scoreMeters = 0;
  currentSpeed = SCROLL_SPEED_INIT_PX_SEC;
  currentDachMin = DACH_START_MIN;
  currentDachMax = DACH_START_MAX;

  isJumping = false;
  jumpFrame = 0;
  playerY = FIGUR_STAND_Y;

  firstPressPlayer = -1;
  kistenBremseEnde = 0;
  aktiveGefahr.aktiv = false;
  shakeEnde = 0;

  for (uint8_t i = 0; i < MAX_PARTIKEL; i++) partikelPuffer[i].aktiv = false;

  abschnittRest = 0;
  isDachState = true;

  for (uint32_t i = 0; i < 50; i++) {
    generateSingleColumn(i);
  }

  selectNewDuo();
  lastProgressionTime = millis();
  lastGefahrCheckTime = millis();
}

void applyProgression(uint32_t now) {
  if (now - lastProgressionTime >= PROGRESSION_INTERVAL_MS) {
    lastProgressionTime = now;

    currentSpeed *= PROGRESSION_SPEED_FACTOR;
    if (currentSpeed > SCROLL_SPEED_MAX_PX_SEC) {
      currentSpeed = SCROLL_SPEED_MAX_PX_SEC;
    }

    if (currentDachMin > DACH_ABSOLUT_MIN) currentDachMin--;
    if (currentDachMax > DACH_ABSOLUT_MIN + 2) currentDachMax--;
  }
}

void checkGefahrenSpawns(uint32_t now) {
  if (GEFAHR_SPAWN_CHANCE_PERCENT == 0) return;
  if (aktiveGefahr.aktiv) return;
  if (currentWeltSpalte < 30) return;

  if (now - lastGefahrCheckTime >= 1000) {
    lastGefahrCheckTime = now;

    if (now - aktiveGefahr.spawnZeit >= GEFAHR_COOLDOWN_MS) {
      if (random(100) < GEFAHR_SPAWN_CHANCE_PERCENT) {
        aktiveGefahr.aktiv = true;
        aktiveGefahr.weltSpalte = currentWeltSpalte + 22;
        aktiveGefahr.spawnZeit = now;
        aktiveGefahr.istGefallen = false;
        aktiveGefahr.aktuellY = 0;
      }
    }
  }
}

void updateInput() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < NUM_PLAYERS; i++) {
    bool rawState = digitalRead(BUTTON_PINS[i]);
    if (rawState != btnStatePrev[i] && (now - btnDebounceTimer[i] > DEBOUNCE_MS)) {
      btnDebounceTimer[i] = now;
      btnStatePrev[i] = rawState;

      if (rawState == LOW) { // Button gedrückt
        if (currentState == STATE_INTRO) {
          currentState = STATE_START;
          return;
        }

        if (currentState == STATE_START) {
          currentState = STATE_COUNTDOWN;
          gameStartTime = now;
          return;
        }

        if (currentState == STATE_GAMEOVER) {
          if (now - gameOverStartTime > 1000) {
            currentState = STATE_START;
          }
          return;
        }

        if (currentState == STATE_PLAYING) {
          if (!DEV_MODE) {
            // STRICT MODE: Prüfe wie viele und welche Taster gedrückt sind
            uint8_t pressedCount = 0;
            bool wrongPressed = false;

            for (uint8_t p = 0; p < NUM_PLAYERS; p++) {
              if (digitalRead(BUTTON_PINS[p]) == LOW) {
                pressedCount++;
                if (p != activeDuoPlayer1 && p != activeDuoPlayer2) {
                  wrongPressed = true;
                }
              }
            }

            // Wenn mehr als 2 Tasten oder eine falsche Taste gedrückt ist: Abbrechen!
            if (pressedCount > 2 || wrongPressed) {
              firstPressPlayer = -1;
              return;
            }

            // Exakt einer der beiden Duo-Spieler gedrückt
            if (i == activeDuoPlayer1 || i == activeDuoPlayer2) {
              if (firstPressPlayer == -1) {
                firstPressPlayer = i;
                duoFirstPressTime = now;
              } else if (firstPressPlayer != i) {
                if (now - duoFirstPressTime <= DUO_WINDOW_MS) {
                  if (!isJumping) {
                    isJumping = true;
                    jumpFrame = 0;
                  }
                }
                firstPressPlayer = -1;
              }
            }
          } else {
            // DEV MODE: Beliebige Tasten erlaubt
            if (i == activeDuoPlayer1 || i == activeDuoPlayer2) {
              if (firstPressPlayer == -1) {
                firstPressPlayer = i;
                duoFirstPressTime = now;
              } else if (firstPressPlayer != i) {
                if (now - duoFirstPressTime <= DUO_WINDOW_MS) {
                  if (!isJumping) {
                    isJumping = true;
                    jumpFrame = 0;
                  }
                }
                firstPressPlayer = -1;
              }
            }
          }
        }
      }
    }
  }

  if (firstPressPlayer != -1 && (now - duoFirstPressTime > DUO_WINDOW_MS)) {
    firstPressPlayer = -1;
  }
}

void updatePhysicsAndWorld(uint32_t now) {
  applyProgression(now);
  checkGefahrenSpawns(now);

  float effectiveSpeed = currentSpeed;
  if (now < kistenBremseEnde) {
    effectiveSpeed *= KISTEN_BREMSFAKTOR;
  }
  if (effectiveSpeed < SCROLL_SPEED_MIN_PX_SEC) {
    effectiveSpeed = SCROLL_SPEED_MIN_PX_SEC;
  }

  scrollAccu += effectiveSpeed * (FRAME_DAUER_MS / 1000.0f);
  bgScrollAccu += (effectiveSpeed * 0.25f) * (FRAME_DAUER_MS / 1000.0f); 

  while (scrollAccu >= 1.0f) {
    scrollAccu -= 1.0f;
    currentWeltSpalte++;
    scoreMeters++;

    while (maxGeneratedWeltSpalte < currentWeltSpalte + 25) {
      generateSingleColumn(maxGeneratedWeltSpalte + 1);
    }
  }

  if (isJumping) {
    bool duoHolding = (digitalRead(BUTTON_PINS[activeDuoPlayer1]) == LOW) || 
                      (digitalRead(BUTTON_PINS[activeDuoPlayer2]) == LOW);

    if (!duoHolding && jumpFrame < 15) {
      jumpFrame = 30 - jumpFrame;
    }

    playerY = FIGUR_STAND_Y + JUMP_OFFSETS[jumpFrame];
    jumpFrame++;

    if (jumpFrame >= 30) {
      isJumping = false;
      playerY = FIGUR_STAND_Y;
      selectNewDuo();
    }
  } else {
    playerY = FIGUR_STAND_Y;
  }

  uint32_t weltSpalteUnterFigur = currentWeltSpalte + FIGUR_X;
  uint8_t figurBufferIdx = weltSpalteUnterFigur % RING_BUFFER_SIZE;
  Spalte* curSpalte = &ringBuffer[figurBufferIdx];

  if (curSpalte->taube) {
    curSpalte->taube = false;
    spawnPartikel(FIGUR_X, FIGUR_STAND_Y, 0.5f, -0.8f, 350, COLOR_PIGEON);
    spawnPartikel(FIGUR_X, FIGUR_STAND_Y, 0.8f, -0.5f, 350, COLOR_PIGEON);
  }

  if (!isJumping) {
    if (curSpalte->gefahr) {
      currentState = STATE_GAMEOVER;
      gameOverStartTime = now;
      triggerShake(500, 2);
      return;
    }

    if (curSpalte->kiste) {
      curSpalte->kiste = false;
      kistenBremseEnde = now + KISTEN_BREMSZEIT_MS;
      spawnPartikel(FIGUR_X, FIGUR_STAND_Y, 0.2f, -0.4f, 300, COLOR_KISTE);
      spawnPartikel(FIGUR_X, FIGUR_STAND_Y, 0.4f, -0.2f, 300, COLOR_KISTE);
      triggerShake(150, 1);
    }

    if (curSpalte->bruechig) {
      if (curSpalte->bruechigBetretenZeit == 0) {
        curSpalte->bruechigBetretenZeit = now;
      } else if (now - curSpalte->bruechigBetretenZeit >= BRUECHIG_VERZOEGERUNG_MS) {
        curSpalte->boden = false;
        curSpalte->bruechig = false;
        spawnPartikel(FIGUR_X, BODEN_Y, 0.0f, 0.5f, 350, COLOR_BRUECHIG);
      }
    }

    if (!curSpalte->boden) {
      currentState = STATE_GAMEOVER;
      gameOverStartTime = now;
      triggerShake(500, 2);
      return;
    }
  }

  if (aktiveGefahr.aktiv) {
    if (now - aktiveGefahr.spawnZeit > GEFAHR_WARNDAUER_MS) {
      aktiveGefahr.istGefallen = true;
      aktiveGefahr.aktuellY = (now - (aktiveGefahr.spawnZeit + GEFAHR_WARNDAUER_MS)) / 40;

      if (aktiveGefahr.aktuellY >= BODEN_Y) {
        int32_t relX = (int32_t)aktiveGefahr.weltSpalte - (int32_t)currentWeltSpalte;
        if (relX >= 0 && relX < 16) {
          uint8_t bIdx = aktiveGefahr.weltSpalte % RING_BUFFER_SIZE;
          if (ringBuffer[bIdx].boden) {
            ringBuffer[bIdx].gefahr = true;
            spawnPartikel(relX, BODEN_Y, -0.5f, -0.5f, 300, COLOR_MISSILE);
            spawnPartikel(relX, BODEN_Y,  0.5f, -0.5f, 300, COLOR_MISSILE);
            triggerShake(200, 1);
          }
        }
        aktiveGefahr.aktiv = false;
      }
    }
  }

  for (uint8_t i = 0; i < MAX_PARTIKEL; i++) {
    if (partikelPuffer[i].aktiv) {
      if (now >= partikelPuffer[i].lebensdauerEnde) {
        partikelPuffer[i].aktiv = false;
      } else {
        partikelPuffer[i].x += partikelPuffer[i].vx;
        partikelPuffer[i].y += partikelPuffer[i].vy;
      }
    }
  }

  if (now < shakeEnde) {
    shakeOffsetX = random(-shakeIntensitaet, shakeIntensitaet + 1);
    shakeOffsetY = random(-shakeIntensitaet, shakeIntensitaet + 1);
  } else {
    shakeOffsetX = 0;
    shakeOffsetY = 0;
  }
}

void selectNewDuo() {
  uint8_t p1 = random(NUM_PLAYERS);
  uint8_t p2 = random(NUM_PLAYERS);
  while (p2 == p1) {
    p2 = random(NUM_PLAYERS);
  }
  activeDuoPlayer1 = p1;
  activeDuoPlayer2 = p2;
}

void spawnPartikel(float x, float y, float vx, float vy, uint32_t dauerMs, CRGB farbe) {
  for (uint8_t i = 0; i < MAX_PARTIKEL; i++) {
    if (!partikelPuffer[i].aktiv) {
      partikelPuffer[i].aktiv = true;
      partikelPuffer[i].x = x;
      partikelPuffer[i].y = y;
      partikelPuffer[i].vx = vx;
      partikelPuffer[i].vy = vy;
      partikelPuffer[i].lebensdauerEnde = millis() + dauerMs;
      partikelPuffer[i].farbe = farbe;
      return;
    }
  }
}

void triggerShake(uint32_t dauerMs, uint8_t intensitaet) {
  shakeEnde = millis() + dauerMs;
  shakeIntensitaet = intensitaet;
}

// ============================================================================
// 8. RENDERER
// ============================================================================
void drawChar5x8(const uint8_t rows[8], int8_t offsetX, int8_t offsetY, CRGB color) {
  for (uint8_t r = 0; r < 8; r++) {
    uint8_t rowVal = rows[r];
    for (uint8_t c = 0; c < 5; c++) {
      if ((rowVal >> (4 - c)) & 0x01) {
        int8_t px = offsetX + c;
        int8_t py = offsetY + r;
        if (px >= 0 && px < 16 && py >= 0 && py < 16) {
          leds[olb_xy(px, py)] = color;
        }
      }
    }
  }
}

void renderIntroScreen(uint32_t now) {
  FastLED.clear();

  if (introStartTime == 0) {
    introStartTime = now;
  }

  int16_t scrollX = 16 - (int16_t)((now - introStartTime) / 90);

  int16_t currentX = scrollX;
  for (uint8_t i = 0; i < CANOLBALT_LEN; i++) {
    drawChar5x8(CANOLBALT_TEXT[i].rows, currentX, 4, CANOLBALT_TEXT[i].color);
    currentX += 6;
  }

  if (currentX < -2) {
    currentState = STATE_START;
  }
}

void renderStartScreen() {
  FastLED.clear();
  for (uint8_t y = 0; y < 16; y++) {
    for (uint8_t x = 0; x < 16; x++) {
      uint8_t p = (y < 8) ? ((x < 8) ? 0 : 1) : ((x < 8) ? 2 : 3);
      leds[olb_xy(x, y)] = PLAYER_COLORS[p];
    }
  }
}

void renderCountdownScreen(uint32_t now) {
  FastLED.clear();
  uint32_t elapsed = now - gameStartTime;
  uint8_t step = 3 - (elapsed / 1000);

  for (uint8_t b = 0; b < step; b++) {
    for (uint8_t x = 2; x < 14; x++) {
      leds[olb_xy(x, 12 - (b * 4))] = CRGB(255, 0, 0);
      leds[olb_xy(x, 13 - (b * 4))] = CRGB(255, 0, 0);
    }
  }
}

void renderGameScreen() {
  FastLED.clear();

  uint32_t bgOffset = (uint32_t)bgScrollAccu;
  for (uint8_t x = 0; x < 16; x++) {
    uint8_t bgIdx = (bgOffset + x) % BG_PATTERN_SIZE;
    BackgroundSpalte bgCol = bgSkyline[bgIdx];

    for (uint8_t y = bgCol.hoeheY; y <= 13; y++) {
      leds[olb_xy(x, y)] = bgCol.farbe;
    }
    for (uint8_t y = bgCol.hoeheY + 1; y <= 13; y++) {
      if ((bgCol.windowMask >> y) & 0x01) {
        leds[olb_xy(x, y)] = CRGB(255, 240, 150);
      }
    }
  }

  CRGB farbeHell   = COLOR_BODEN;
  CRGB farbeDunkel = CRGB(farbeHell.r / 2, farbeHell.g / 2, farbeHell.b / 2);

  for (uint8_t rx = 0; rx < 16; rx++) {
    uint32_t targetWeltSpalte = currentWeltSpalte + rx;
    uint8_t bufferIdx = targetWeltSpalte % RING_BUFFER_SIZE;
    Spalte s = ringBuffer[bufferIdx];

    if (s.boden) {
      CRGB dachFarbeOben;
      CRGB dachFarbeUnten;

      if (s.gefahr) { 
        dachFarbeOben = COLOR_MISSILE;
        uint8_t patternStep = targetWeltSpalte % 3;
        dachFarbeUnten = (patternStep == 1) ? farbeDunkel : farbeHell;
      } else if (s.bruechig) {
        dachFarbeOben  = COLOR_BRUECHIG;
        dachFarbeUnten = COLOR_BRUECHIG;
      } else {
        uint8_t patternStep = targetWeltSpalte % 3;
        dachFarbeOben  = (patternStep == 0) ? farbeDunkel : farbeHell;
        dachFarbeUnten = (patternStep == 1) ? farbeDunkel : farbeHell;
      }

      int8_t ry1 = BODEN_Y + shakeOffsetY;       // y = 14
      int8_t ry2 = (BODEN_Y + 1) + shakeOffsetY; // y = 15

      if (ry1 >= 0 && ry1 < 16) leds[olb_xy(rx, ry1)] = dachFarbeOben;
      if (ry2 >= 0 && ry2 < 16) leds[olb_xy(rx, ry2)] = dachFarbeUnten;

      if (s.kiste) {
        int8_t ryKiste = (BODEN_Y - 1) + shakeOffsetY;
        if (ryKiste >= 0 && ryKiste < 16) leds[olb_xy(rx, ryKiste)] = COLOR_KISTE;
      }

      if (s.taube) {
        int8_t ryTaube = (BODEN_Y - 1) + shakeOffsetY;
        if (ryTaube >= 0 && ryTaube < 16) leds[olb_xy(rx, ryTaube)] = COLOR_PIGEON;
      }
    }
  }

  if (aktiveGefahr.aktiv) {
    int32_t relX = (int32_t)aktiveGefahr.weltSpalte - (int32_t)currentWeltSpalte;
    relX += shakeOffsetX;

    if (relX >= 0 && relX < 16) {
      if (!aktiveGefahr.istGefallen) {
        if ((millis() / 150) % 2 == 0) {
          leds[olb_xy(relX, 1)] = COLOR_MISSILE;
        }
      } else {
        int8_t ry = aktiveGefahr.aktuellY + shakeOffsetY;
        if (ry >= 0 && ry < 16) {
          leds[olb_xy(relX, ry)] = COLOR_MISSILE;
        }
      }
    }
  }

  int8_t prx = FIGUR_X + shakeOffsetX;
  int8_t pry = playerY + shakeOffsetY;
  if (prx >= 0 && prx < 16 && pry >= 0 && pry < 16) {
    leds[olb_xy(prx, pry)] = COLOR_PLAYER;
  }

  for (uint8_t i = 0; i < MAX_PARTIKEL; i++) {
    if (partikelPuffer[i].aktiv) {
      int16_t px = (int16_t)partikelPuffer[i].x + shakeOffsetX;
      int16_t py = (int16_t)partikelPuffer[i].y + shakeOffsetY;
      if (px >= 0 && px < 16 && py >= 0 && py < 16) {
        leds[olb_xy(px, py)] = partikelPuffer[i].farbe;
      }
    }
  }

  // Duo-Balken: Ganze oberste Reihe (Y=0), linke Hälfte Spieler 1, rechte Hälfte Spieler 2
  for (uint8_t x = 0; x < 8; x++) {
    leds[olb_xy(x, 0)] = PLAYER_COLORS[activeDuoPlayer1];
  }
  for (uint8_t x = 8; x < 16; x++) {
    leds[olb_xy(x, 0)] = PLAYER_COLORS[activeDuoPlayer2];
  }
}

void renderGameOverScreen(uint32_t now) {
  uint32_t elapsed = now - gameOverStartTime;
  FastLED.clear();

  if (elapsed < 600) {
    if ((elapsed / 150) % 2 == 0) {
      for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB(255, 0, 0);
    }
  } else if (elapsed < 1600) {
    for (uint8_t i = 0; i < 16; i++) {
      leds[olb_xy(i, i)] = CRGB(255, 0, 0);
      leds[olb_xy(15 - i, i)] = CRGB(255, 0, 0);
    }
  } else {
    uint16_t score = scoreMeters;
    if (score < 1000) {
      uint8_t d100 = score / 100;
      uint8_t d10 = (score / 10) % 10;
      uint8_t d1 = score % 10;

      if (score >= 100) {
        drawDigit(d100, 1, 5, CRGB(255, 255, 255));
        drawDigit(d10, 6, 5, CRGB(255, 255, 255));
        drawDigit(d1, 11, 5, CRGB(255, 255, 255));
      } else if (score >= 10) {
        drawDigit(d10, 4, 5, CRGB(255, 255, 255));
        drawDigit(d1, 9, 5, CRGB(255, 255, 255));
      } else {
        drawDigit(d1, 6, 5, CRGB(255, 255, 255));
      }
    } else {
      char buf[6];
      snprintf(buf, sizeof(buf), "%u", score);
      uint8_t len = strlen(buf);

      uint32_t digitPhase = (elapsed - 1600) / 600;
      uint8_t showDigitIndex = min((uint32_t)len - 1, digitPhase);

      drawDigit(buf[showDigitIndex] - '0', 6, 5, CRGB(255, 255, 255));
    }
  }
}

void drawDigit(uint8_t digit, int8_t offsetX, int8_t offsetY, CRGB color) {
  if (digit > 9) return;
  uint16_t bitmap = DIGITS_3X5[digit];
  for (uint8_t row = 0; row < 5; row++) {
    for (uint8_t col = 0; col < 3; col++) {
      uint8_t bitIndex = (4 - row) * 3 + (2 - col);
      if ((bitmap >> bitIndex) & 0x01) {
        int8_t x = offsetX + col;
        int8_t y = offsetY + row;
        if (x >= 0 && x < 16 && y >= 0 && y < 16) {
          leds[olb_xy(x, y)] = color;
        }
      }
    }
  }
}