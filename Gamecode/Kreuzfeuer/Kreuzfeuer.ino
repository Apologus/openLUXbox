/*  ============================================================================
    Board / Treiber: "NodeMCU 1.0 (ESP-12E Module)"
    ----------------------------------------------------------------------------
    openLUXbox - KREUZFEUER v0.9
    Kooperatives Schiffe-Versenken fuer 4 Spieler
    ESP8266 (D1 Mini) + WS2812B 16x16 + 4 Taster
    Benoetigte Bibliothek: FastLED
    Autor: freaky-m0 & Claude
    ============================================================================ */

/* ---------------------------------------------------------------------------
   INCLUDES
   --------------------------------------------------------------------------- */
#include <FastLED.h>
#include <ESP8266WiFi.h>

/* ---------------------------------------------------------------------------
   DEFINES / KONSTANTEN
   --------------------------------------------------------------------------- */

/* --- Display / System --- */
#define OLB_LED_PIN              D4
#define OLB_MATRIX_W             16
#define OLB_MATRIX_H             16
#define OLB_NUM_LEDS             256
#define OLB_BRIGHTNESS           51      /* 20% von 255 */
#define OLB_FRAME_MS             33      /* ca. 30 FPS  */
#define OLB_SERIAL_BAUD          115200

/* --- Spielfeld --- */
#define OLB_SEA_MIN              1
#define OLB_SEA_MAX              14
#define OLB_SEA_SIZE             14

/* --- Taster (GPIO) --- */
#define OLB_BTN_GREEN            4       /* D2 - rechte Bahn, feuert nach links  */
#define OLB_BTN_YELLOW           14      /* D5 - linke  Bahn, feuert nach rechts */
#define OLB_BTN_WHITE            5       /* D1 - untere Bahn, feuert nach oben   */
#define OLB_BTN_BLUE             12      /* D6 - obere  Bahn, feuert nach unten  */
#define OLB_DEBOUNCE_MS          30
#define OLB_SEED_PIN             A0      /* unbeschaltet */

/* --- Geschuetze --- */
#define OLB_CANNON_STEP_MS_BASE  200
#define OLB_CANNON_STEP_DEC      20
#define OLB_CANNON_STEP_MIN      90
#define OLB_RELOAD_MS            600

/* --- Torpedos --- */
#define OLB_TORPEDO_STEP_MS      45
#define OLB_MAX_TORPEDOS         4

/* --- Flotte --- */
#define OLB_MAX_SHIPS            8
#define OLB_MAX_SHIP_LEN         5
#define OLB_PLACE_TRIES          200
#define OLB_PLACE_RESTARTS       20

/* --- Effekte --- */
#define OLB_MAX_FX               6
#define OLB_FX_HIT_MS            180
#define OLB_FX_SINK_MS           700
#define OLB_SONAR_TRIGGER        8
#define OLB_SONAR_MS             900

/* --- Bildschirme --- */
#define OLB_WAVECLEAR_MS         5000
#define OLB_GAMEOVER_LOCK_MS     1500
#define OLB_GAMEOVER_REVEAL_MS   6000
#define OLB_HOLD_START_MS        1500
#define OLB_TITLE_WAVE_MS        200
#define OLB_STARTFLASH_MS        600

/* --- Zellzustaende --- */
#define OLB_FOG                  0
#define OLB_WATER                1
#define OLB_HIT                  2
#define OLB_SUNK                 3

/* --- Kanten --- */
#define OLB_EDGE_LEFT            0
#define OLB_EDGE_RIGHT           1
#define OLB_EDGE_TOP             2
#define OLB_EDGE_BOTTOM          3

/* --- Spielzustaende --- */
#define OLB_STATE_TITLE          0
#define OLB_STATE_PLAY           1
#define OLB_STATE_WAVECLEAR      2
#define OLB_STATE_GAMEOVER       3

/* --- FX-Typen --- */
#define OLB_FX_HIT_TYPE          0
#define OLB_FX_WRECK_TYPE        1

/* --- Farben --- */
const CRGB OLB_COL_FOG      = CRGB(0, 0, 12);
const CRGB OLB_COL_WATER    = CRGB(0, 60, 90);
const CRGB OLB_COL_HIT      = CRGB(255, 30, 0);
const CRGB OLB_COL_SUNK     = CRGB(90, 35, 0);
const CRGB OLB_COL_YELLOW   = CRGB(255, 190, 0);
const CRGB OLB_COL_GREEN    = CRGB(0, 255, 40);
const CRGB OLB_COL_WHITE    = CRGB(255, 255, 255);
const CRGB OLB_COL_BLUE     = CRGB(0, 80, 255);
const CRGB OLB_COL_WRECKFX  = CRGB(120, 120, 120);
const CRGB OLB_COL_REVEAL   = CRGB(160, 160, 0);
const CRGB OLB_COL_AMMO_OK  = CRGB(0, 120, 20);
const CRGB OLB_COL_AMMO_MID = CRGB(140, 100, 0);
const CRGB OLB_COL_AMMO_LOW = CRGB(160, 0, 0);

/* --- Titelbild-Palette --- */
const CRGB OLB_COL_HULL     = CRGB(255, 255, 255);
const CRGB OLB_COL_SAIL     = CRGB(0, 180, 200);
const CRGB OLB_COL_MAST     = CRGB(90, 90, 90);
const CRGB OLB_COL_FLAG     = CRGB(255, 0, 0);
const CRGB OLB_COL_WAVE     = CRGB(0, 40, 120);

/* --- Bilddaten (14x14, logisch: Index 0 = oben links) --- */
const char OLB_FRAME_TITLE[OLB_SEA_SIZE][OLB_SEA_SIZE + 1] PROGMEM = {
  "..............",
  "......R.......",
  "......RR......",
  "......M.......",
  "...SSSMSSS....",
  "..SSSSMSSSS...",
  "......M.......",
  ".HHHHHHHHHHHH.",
  "..HHHHHHHHHH..",
  "...HHHHHHHH...",
  "..............",
  "WW.WWWW.WWWW.W",
  ".WWWW.WWWW.WWW",
  "WWW.WWWW.WWWW."
};
#define OLB_TITLE_WAVE_ROW0      11      /* ab dieser Zeile wird gescrollt */

const char OLB_FRAME_GAMEOVER[OLB_SEA_SIZE][OLB_SEA_SIZE + 1] PROGMEM = {
  "..............",
  ".X..........X.",
  "..X........X..",
  "...X......X...",
  "....X....X....",
  ".....X..X.....",
  "......XX......",
  "......XX......",
  ".....X..X.....",
  "....X....X....",
  "...X......X...",
  "..X........X..",
  ".X..........X.",
  ".............."
};

const char OLB_FRAME_CLEAR[OLB_SEA_SIZE][OLB_SEA_SIZE + 1] PROGMEM = {
  "..............",
  "..............",
  "............X.",
  "...........XX.",
  "..........XX..",
  ".........XX...",
  "...X....XX....",
  "...XX..XX.....",
  "....XXXX......",
  ".....XX.......",
  "..............",
  "..............",
  "..............",
  ".............."
};

/* --- Flottenaufstellung pro Welle (0 = kein Schiff) --- */
#define OLB_WAVE_TABLE_LEN       5
const uint8_t OLB_WAVE_FLEETS[OLB_WAVE_TABLE_LEN][OLB_MAX_SHIPS] = {
  {5, 4, 3, 3, 2, 0, 0, 0},
  {5, 4, 4, 3, 3, 2, 0, 0},
  {5, 5, 4, 3, 3, 2, 2, 0},
  {5, 5, 4, 4, 3, 3, 2, 2},
  {5, 5, 5, 4, 4, 3, 3, 2}
};

/* --- Munition ---
   munition = zellen * OLB_AMMO_PER_CELL + OLB_AMMO_BASE
              - (welle - 1) * OLB_AMMO_WAVE_DEC
   Untergrenze: zellen + OLB_AMMO_MIN_EXTRA
   Da fuer jede Schiffszelle zwingend ein Treffer noetig ist, entspricht
   (munition - zellen) der Zahl der erlaubten Fehlschuesse. Das ist der
   eigentliche Schwierigkeitsregler. */
#define OLB_AMMO_PER_CELL        2
#define OLB_AMMO_BASE            10
#define OLB_AMMO_WAVE_DEC        2
#define OLB_AMMO_MIN_EXTRA       12

/* --- Punkte --- */
#define OLB_SCORE_HIT            10
#define OLB_SCORE_SUNK           50
#define OLB_SCORE_WAVE           25
#define OLB_SCORE_AMMO_LEFT      2

/* ---------------------------------------------------------------------------
   STRUCTS
   --------------------------------------------------------------------------- */
struct OlbShip {
  uint8_t  x, y;
  uint8_t  len;
  bool     horiz;
  uint8_t  hits;
  bool     sunk;
  uint32_t sinkStartMs;
};

struct OlbCannon {
  uint8_t  pin;
  uint8_t  edge;
  uint8_t  pos;
  int8_t   dir;
  uint32_t lastStepMs;
  uint32_t reloadUntilMs;
  bool     lastLevel;        /* true = gedrueckt */
  uint32_t lastEdgeMs;
  uint32_t pressedSinceMs;
  bool     ready;
  CRGB     color;
};

struct OlbTorpedo {
  bool     active;
  int8_t   x, y;
  int8_t   dx, dy;
  uint32_t lastStepMs;
  uint8_t  owner;
};

struct OlbFx {
  bool     active;
  uint8_t  x, y;
  uint8_t  type;
  uint32_t startMs;
};

/* ---------------------------------------------------------------------------
   PROTOTYPEN
   --------------------------------------------------------------------------- */
uint16_t olb_xy(uint8_t x, uint8_t y);
void     olb_setPixel(int16_t x, int16_t y, CRGB color);
bool     olb_inSea(int16_t x, int16_t y);
CRGB     olb_scale(CRGB c, uint8_t s);

void     olb_initCannons();
void     olb_readInputs(uint32_t now);

void     olb_startGame(uint32_t now);
void     olb_startWave(uint32_t now);
void     olb_resetBoard();
bool     olb_placeFleet();
bool     olb_canPlace(uint8_t x, uint8_t y, uint8_t len, bool horiz);
void     olb_writeShip(uint8_t index);
void     olb_fallbackFleet();

void     olb_updateTitle(uint32_t now);
void     olb_updatePlay(uint32_t now);
void     olb_updateWaveClear(uint32_t now);
void     olb_updateGameOver(uint32_t now);

void     olb_updateCannons(uint32_t now);
void     olb_fire(uint8_t player, uint32_t now);
void     olb_updateTorpedos(uint32_t now);
void     olb_resolveCell(uint8_t t, uint32_t now);
void     olb_sinkShip(uint8_t index, uint32_t now);
void     olb_spawnFx(uint8_t x, uint8_t y, uint8_t type, uint32_t now);
void     olb_triggerSonar(uint32_t now);

void     olb_render(uint32_t now);
void     olb_drawTitle(uint32_t now);
void     olb_drawSea();
void     olb_drawSinkFx(uint32_t now);
void     olb_drawSonar(uint32_t now);
void     olb_drawFx(uint32_t now);
void     olb_drawTorpedos();
void     olb_drawCannons(uint32_t now);
void     olb_drawAmmoCorners(uint32_t now);
void     olb_drawWaveClear(uint32_t now);
void     olb_drawGameOver(uint32_t now);
void     olb_drawSymbolFrame(const char frame[][OLB_SEA_SIZE + 1], CRGB color);

uint8_t  olb_shipsRemaining();
bool     olb_torpedosActive();
uint16_t olb_cannonStepMs();
void     olb_logStats();

/* ---------------------------------------------------------------------------
   GLOBALE VARIABLEN
   --------------------------------------------------------------------------- */
CRGB       olb_leds[OLB_NUM_LEDS];

uint8_t    olb_state[OLB_MATRIX_H][OLB_MATRIX_W];   /* Sichtbarkeit */
uint8_t    olb_ship[OLB_MATRIX_H][OLB_MATRIX_W];    /* Schiffsindex + 1 */

OlbShip    olb_ships[OLB_MAX_SHIPS];
uint8_t    olb_shipCount = 0;

OlbCannon  olb_cannons[4];
OlbTorpedo olb_torpedos[OLB_MAX_TORPEDOS];
OlbFx      olb_fx[OLB_MAX_FX];

bool       olb_pressEvent[4];

uint8_t    olb_gameState   = OLB_STATE_TITLE;
uint8_t    olb_wave        = 1;
int16_t    olb_ammo        = 0;
int16_t    olb_ammoStart   = 0;
uint32_t   olb_score       = 0;
uint16_t   olb_shotsFired  = 0;
uint16_t   olb_shotsHit    = 0;
uint8_t    olb_dryShots    = 0;

uint32_t   olb_lastFrameMs   = 0;
uint32_t   olb_lastBeatMs    = 0;
uint16_t   olb_frameCounter  = 0;
uint32_t   olb_stateStartMs  = 0;
uint32_t   olb_titleScrollMs = 0;
uint8_t    olb_titleScroll   = 0;
uint32_t   olb_startFlashMs  = 0;
bool       olb_startPending  = false;

int8_t     olb_sonarX = -1;
int8_t     olb_sonarY = -1;
uint32_t   olb_sonarStartMs = 0;

/* ---------------------------------------------------------------------------
   SETUP
   --------------------------------------------------------------------------- */
void setup() {
  Serial.begin(OLB_SERIAL_BAUD);
  delay(100);
  Serial.println();
  Serial.println(F("[BOOT] openLUXbox KREUZFEUER v1.0"));

  /* WLAN abschalten. Das delay(1) ist noetig, damit der SDK-Sleep greift. */
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);

  /* Hinweis zum Watchdog:
     ESP.wdtDisable() stoppt den Software-Watchdog UND damit zugleich das
     Fuettern des Hardware-Watchdogs -> Reset nach ca. 8 Sekunden, dauerhaft.
     Die loop() ist vollstaendig nicht blockierend, der WDT loest daher nie
     aus. Er bleibt bewusst aktiv. */

  pinMode(OLB_BTN_GREEN,  INPUT_PULLUP);
  pinMode(OLB_BTN_YELLOW, INPUT_PULLUP);
  pinMode(OLB_BTN_WHITE,  INPUT_PULLUP);
  pinMode(OLB_BTN_BLUE,   INPUT_PULLUP);

  FastLED.addLeds<WS2812B, OLB_LED_PIN, GRB>(olb_leds, OLB_NUM_LEDS);
  FastLED.setBrightness(OLB_BRIGHTNESS);
  FastLED.clear(true);

  uint32_t seed = ((uint32_t)analogRead(OLB_SEED_PIN) << 16) ^ micros();
  randomSeed(seed);
  Serial.print(F("[SEED] "));
  Serial.println(seed);

  olb_initCannons();
  olb_resetBoard();

  olb_gameState    = OLB_STATE_TITLE;
  olb_stateStartMs = millis();
}

/* ---------------------------------------------------------------------------
   LOOP
   --------------------------------------------------------------------------- */
void loop() {
  uint32_t now = millis();

  olb_readInputs(now);

  switch (olb_gameState) {
    case OLB_STATE_TITLE:     olb_updateTitle(now);     break;
    case OLB_STATE_PLAY:      olb_updatePlay(now);      break;
    case OLB_STATE_WAVECLEAR: olb_updateWaveClear(now); break;
    case OLB_STATE_GAMEOVER:  olb_updateGameOver(now);  break;
  }

  if (now - olb_lastFrameMs >= OLB_FRAME_MS) {
    olb_lastFrameMs = now;
    olb_render(now);
    olb_frameCounter++;
  }

  /* Heartbeat: zeigt im Seriellen Monitor, dass die loop() laeuft.
     Zum Abschalten diesen Block auskommentieren. */
  if (now - olb_lastBeatMs >= 2000) {
    olb_lastBeatMs = now;
    Serial.print(F("[BEAT] state="));
    Serial.print(olb_gameState);
    Serial.print(F(" frames/2s="));
    Serial.print(olb_frameCounter);
    Serial.print(F(" heap="));
    Serial.println(ESP.getFreeHeap());
    olb_frameCounter = 0;
  }
}

/* ===========================================================================
   HILFSFUNKTIONEN / MAPPING
   =========================================================================== */

/* Serpentinen-Mapping laut TSD: Startpunkt oben rechts.
   Gerade Zeilen rechts->links, ungerade Zeilen links->rechts. */
uint16_t olb_xy(uint8_t x, uint8_t y) {
  if (y % 2 == 0) {
    return (uint16_t)y * OLB_MATRIX_W + (15 - x);
  }
  return (uint16_t)y * OLB_MATRIX_W + x;
}

void olb_setPixel(int16_t x, int16_t y, CRGB color) {
  if (x < 0 || x >= OLB_MATRIX_W || y < 0 || y >= OLB_MATRIX_H) {
    return;
  }
  olb_leds[olb_xy((uint8_t)x, (uint8_t)y)] = color;
}

bool olb_inSea(int16_t x, int16_t y) {
  return (x >= OLB_SEA_MIN && x <= OLB_SEA_MAX &&
          y >= OLB_SEA_MIN && y <= OLB_SEA_MAX);
}

CRGB olb_scale(CRGB c, uint8_t s) {
  CRGB out = c;
  out.nscale8(s);
  return out;
}

uint16_t olb_cannonStepMs() {
  int16_t step = (int16_t)OLB_CANNON_STEP_MS_BASE - (int16_t)(olb_wave - 1) * OLB_CANNON_STEP_DEC;
  if (step < OLB_CANNON_STEP_MIN) {
    step = OLB_CANNON_STEP_MIN;
  }
  return (uint16_t)step;
}

/* ===========================================================================
   EINGABEN
   =========================================================================== */
/* ---------------------------------------------------------------------------
   Zuordnung Spieler -> Kante.
   Nur diese vier edge-Zuweisungen bestimmen, wo ein Spieler sitzt. Schuss-
   richtung, Rendering und Bereitschaftsanzeige leiten sich daraus ab. Zum
   Drehen des Aufbaus genuegt es, die Kanten hier zu tauschen:
     LEFT <-> RIGHT   und   TOP <-> BOTTOM
   --------------------------------------------------------------------------- */
void olb_initCannons() {
  olb_cannons[0].pin  = OLB_BTN_GREEN;
  olb_cannons[0].edge = OLB_EDGE_RIGHT;
  olb_cannons[0].pos  = 1;
  olb_cannons[0].dir  = 1;
  olb_cannons[0].color = OLB_COL_GREEN;

  olb_cannons[1].pin  = OLB_BTN_YELLOW;
  olb_cannons[1].edge = OLB_EDGE_LEFT;
  olb_cannons[1].pos  = 14;
  olb_cannons[1].dir  = -1;
  olb_cannons[1].color = OLB_COL_YELLOW;

  olb_cannons[2].pin  = OLB_BTN_WHITE;
  olb_cannons[2].edge = OLB_EDGE_BOTTOM;
  olb_cannons[2].pos  = 5;
  olb_cannons[2].dir  = 1;
  olb_cannons[2].color = OLB_COL_WHITE;

  olb_cannons[3].pin  = OLB_BTN_BLUE;
  olb_cannons[3].edge = OLB_EDGE_TOP;
  olb_cannons[3].pos  = 10;
  olb_cannons[3].dir  = -1;
  olb_cannons[3].color = OLB_COL_BLUE;

  for (uint8_t i = 0; i < 4; i++) {
    olb_cannons[i].lastStepMs     = 0;
    olb_cannons[i].reloadUntilMs  = 0;
    olb_cannons[i].lastLevel      = false;
    olb_cannons[i].lastEdgeMs     = 0;
    olb_cannons[i].pressedSinceMs = 0;
    olb_cannons[i].ready          = false;
  }
}

/* Flankenauswertung HIGH->LOW mit 30ms Entprellung laut TSD. */
void olb_readInputs(uint32_t now) {
  for (uint8_t i = 0; i < 4; i++) {
    olb_pressEvent[i] = false;

    bool level = (digitalRead(olb_cannons[i].pin) == LOW);

    if (level != olb_cannons[i].lastLevel) {
      if (now - olb_cannons[i].lastEdgeMs >= OLB_DEBOUNCE_MS) {
        olb_cannons[i].lastEdgeMs = now;
        olb_cannons[i].lastLevel  = level;
        if (level) {
          olb_cannons[i].pressedSinceMs = now;
          olb_pressEvent[i] = true;
        }
      }
    }
  }
}

/* ===========================================================================
   SPIELAUFBAU
   =========================================================================== */
void olb_resetBoard() {
  for (uint8_t y = 0; y < OLB_MATRIX_H; y++) {
    for (uint8_t x = 0; x < OLB_MATRIX_W; x++) {
      olb_state[y][x] = OLB_FOG;
      olb_ship[y][x]  = 0;
    }
  }
  for (uint8_t i = 0; i < OLB_MAX_TORPEDOS; i++) {
    olb_torpedos[i].active = false;
  }
  for (uint8_t i = 0; i < OLB_MAX_FX; i++) {
    olb_fx[i].active = false;
  }
  olb_sonarX = -1;
  olb_sonarY = -1;
}

void olb_startGame(uint32_t now) {
  olb_wave       = 1;
  olb_score      = 0;
  olb_shotsFired = 0;
  olb_shotsHit   = 0;
  olb_startWave(now);
}

void olb_startWave(uint32_t now) {
  olb_resetBoard();

  uint8_t table = olb_wave;
  if (table > OLB_WAVE_TABLE_LEN) {
    table = OLB_WAVE_TABLE_LEN;
  }

  olb_shipCount = 0;
  uint8_t cells = 0;
  for (uint8_t i = 0; i < OLB_MAX_SHIPS; i++) {
    uint8_t len = OLB_WAVE_FLEETS[table - 1][i];
    if (len == 0) {
      continue;
    }
    olb_ships[olb_shipCount].len         = len;
    olb_ships[olb_shipCount].hits        = 0;
    olb_ships[olb_shipCount].sunk        = false;
    olb_ships[olb_shipCount].sinkStartMs = 0;
    olb_shipCount++;
    cells += len;
  }

  if (!olb_placeFleet()) {
    Serial.println(F("[WARN] Zufallsplatzierung fehlgeschlagen - Fallback"));
    olb_fallbackFleet();
  }

  int16_t ammo = (int16_t)cells * OLB_AMMO_PER_CELL + OLB_AMMO_BASE
                 - (int16_t)(olb_wave - 1) * OLB_AMMO_WAVE_DEC;
  int16_t ammoMin = (int16_t)cells + OLB_AMMO_MIN_EXTRA;
  if (ammo < ammoMin) {
    ammo = ammoMin;
  }
  olb_ammo      = ammo;
  olb_ammoStart = ammo;
  olb_dryShots  = 0;

  for (uint8_t i = 0; i < 4; i++) {
    olb_cannons[i].reloadUntilMs = 0;
    olb_cannons[i].lastStepMs    = now;
  }

  olb_gameState    = OLB_STATE_PLAY;
  olb_stateStartMs = now;

  Serial.print(F("[WAVE] "));
  Serial.print(olb_wave);
  Serial.print(F(" | Schiffe: "));
  Serial.print(olb_shipCount);
  Serial.print(F(" | Zellen: "));
  Serial.print(cells);
  Serial.print(F(" | Munition: "));
  Serial.print(olb_ammo);
  Serial.print(F(" | Fehlschuesse frei: "));
  Serial.println(olb_ammo - (int16_t)cells);

  for (uint8_t i = 0; i < olb_shipCount; i++) {
    Serial.print(F("[SHIP] #"));
    Serial.print(i);
    Serial.print(F(" len="));
    Serial.print(olb_ships[i].len);
    Serial.print(F(" pos=("));
    Serial.print(olb_ships[i].x);
    Serial.print(F(","));
    Serial.print(olb_ships[i].y);
    Serial.print(F(") "));
    Serial.println(olb_ships[i].horiz ? F("horiz") : F("vert"));
  }
}

bool olb_canPlace(uint8_t x, uint8_t y, uint8_t len, bool horiz) {
  for (uint8_t i = 0; i < len; i++) {
    int16_t cx = horiz ? (int16_t)x + i : (int16_t)x;
    int16_t cy = horiz ? (int16_t)y     : (int16_t)y + i;

    if (!olb_inSea(cx, cy)) {
      return false;
    }
    /* 8er-Nachbarschaft inklusive der Zelle selbst muss frei sein */
    for (int8_t dy = -1; dy <= 1; dy++) {
      for (int8_t dx = -1; dx <= 1; dx++) {
        int16_t nx = cx + dx;
        int16_t ny = cy + dy;
        if (!olb_inSea(nx, ny)) {
          continue;
        }
        if (olb_ship[ny][nx] != 0) {
          return false;
        }
      }
    }
  }
  return true;
}

void olb_writeShip(uint8_t index) {
  OlbShip *s = &olb_ships[index];
  for (uint8_t i = 0; i < s->len; i++) {
    uint8_t cx = s->horiz ? s->x + i : s->x;
    uint8_t cy = s->horiz ? s->y     : s->y + i;
    olb_ship[cy][cx] = index + 1;
  }
}

bool olb_placeFleet() {
  for (uint8_t restart = 0; restart < OLB_PLACE_RESTARTS; restart++) {

    for (uint8_t y = 0; y < OLB_MATRIX_H; y++) {
      for (uint8_t x = 0; x < OLB_MATRIX_W; x++) {
        olb_ship[y][x] = 0;
      }
    }

    bool allPlaced = true;

    for (uint8_t i = 0; i < olb_shipCount; i++) {
      bool placed = false;
      uint8_t len = olb_ships[i].len;

      for (uint16_t t = 0; t < OLB_PLACE_TRIES; t++) {
        bool    horiz = (random(2) == 0);
        uint8_t maxX  = horiz ? (OLB_SEA_MAX - len + 1) : OLB_SEA_MAX;
        uint8_t maxY  = horiz ? OLB_SEA_MAX : (OLB_SEA_MAX - len + 1);
        uint8_t x = (uint8_t)random(OLB_SEA_MIN, maxX + 1);
        uint8_t y = (uint8_t)random(OLB_SEA_MIN, maxY + 1);

        if (olb_canPlace(x, y, len, horiz)) {
          olb_ships[i].x     = x;
          olb_ships[i].y     = y;
          olb_ships[i].horiz = horiz;
          olb_writeShip(i);
          placed = true;
          break;
        }
      }

      if (!placed) {
        allPlaced = false;
        break;
      }
    }

    if (allPlaced) {
      return true;
    }
  }
  return false;
}

/* Deterministisches Regal-Layout als letzte Rueckfallebene.
   Zeilen 1,3,5,...,13 - dadurch immer mindestens 1 Zelle Abstand. */
void olb_fallbackFleet() {
  for (uint8_t y = 0; y < OLB_MATRIX_H; y++) {
    for (uint8_t x = 0; x < OLB_MATRIX_W; x++) {
      olb_ship[y][x] = 0;
    }
  }

  uint8_t cx = OLB_SEA_MIN;
  uint8_t cy = OLB_SEA_MIN;

  for (uint8_t i = 0; i < olb_shipCount; i++) {
    uint8_t len = olb_ships[i].len;
    if (cx + len - 1 > OLB_SEA_MAX) {
      cy += 2;
      cx = OLB_SEA_MIN;
    }
    if (cy > OLB_SEA_MAX) {
      cy = OLB_SEA_MIN;   /* Notbremse, sollte nie eintreten */
    }
    olb_ships[i].x     = cx;
    olb_ships[i].y     = cy;
    olb_ships[i].horiz = true;
    olb_writeShip(i);
    cx += len + 1;
  }
}

/* ===========================================================================
   ZUSTAND: TITEL
   =========================================================================== */
void olb_updateTitle(uint32_t now) {

  if (now - olb_titleScrollMs >= OLB_TITLE_WAVE_MS) {
    olb_titleScrollMs = now;
    olb_titleScroll   = (olb_titleScroll + 1) % OLB_SEA_SIZE;
  }

  if (olb_startPending) {
    if (now - olb_startFlashMs >= OLB_STARTFLASH_MS) {
      olb_startPending = false;
      for (uint8_t i = 0; i < 4; i++) {
        olb_cannons[i].ready = false;
      }
      olb_startGame(now);
    }
    return;
  }

  /* Bereitschaft umschalten */
  for (uint8_t i = 0; i < 4; i++) {
    if (olb_pressEvent[i]) {
      olb_cannons[i].ready = !olb_cannons[i].ready;
      Serial.print(F("[RDY ] Spieler "));
      Serial.print(i);
      Serial.println(olb_cannons[i].ready ? F(" bereit") : F(" nicht bereit"));
    }
  }

  /* Sofortstart durch Halten (einzige erlaubte Halte-Auswertung) */
  for (uint8_t i = 0; i < 4; i++) {
    if (olb_cannons[i].lastLevel &&
        (now - olb_cannons[i].pressedSinceMs >= OLB_HOLD_START_MS)) {
      olb_startPending = true;
      olb_startFlashMs = now;
      Serial.println(F("[STRT] Sofortstart (Halten)"));
      return;
    }
  }

  bool allReady = true;
  for (uint8_t i = 0; i < 4; i++) {
    if (!olb_cannons[i].ready) {
      allReady = false;
    }
  }
  if (allReady) {
    olb_startPending = true;
    olb_startFlashMs = now;
    Serial.println(F("[STRT] Alle vier bereit"));
  }
}

/* ===========================================================================
   ZUSTAND: SPIEL
   =========================================================================== */
void olb_updatePlay(uint32_t now) {

  olb_updateCannons(now);

  for (uint8_t i = 0; i < 4; i++) {
    if (olb_pressEvent[i]) {
      olb_fire(i, now);
    }
  }

  olb_updateTorpedos(now);

  if (olb_dryShots >= OLB_SONAR_TRIGGER) {
    olb_triggerSonar(now);
  }

  if (olb_shipsRemaining() == 0) {
    /* Sink-Animation des letzten Schiffs noch abwarten */
    bool animRunning = false;
    for (uint8_t i = 0; i < olb_shipCount; i++) {
      if (olb_ships[i].sinkStartMs != 0 &&
          now - olb_ships[i].sinkStartMs < OLB_FX_SINK_MS) {
        animRunning = true;
      }
    }
    if (!animRunning) {
      olb_score += (uint32_t)OLB_SCORE_WAVE * olb_wave;
      olb_score += (uint32_t)olb_ammo * OLB_SCORE_AMMO_LEFT;
      Serial.print(F("[WAVE] geschafft | Restmunition "));
      Serial.print(olb_ammo);
      Serial.print(F(" | Punkte: "));
      Serial.println(olb_score);
      olb_gameState    = OLB_STATE_WAVECLEAR;
      olb_stateStartMs = now;
    }
    return;
  }

  if (olb_ammo <= 0 && !olb_torpedosActive()) {
    Serial.println(F("[OVER] Munition erschoepft"));
    olb_logStats();
    olb_gameState    = OLB_STATE_GAMEOVER;
    olb_stateStartMs = now;
  }
}

void olb_updateCannons(uint32_t now) {
  uint16_t step = olb_cannonStepMs();

  for (uint8_t i = 0; i < 4; i++) {
    while (now - olb_cannons[i].lastStepMs >= step) {
      olb_cannons[i].lastStepMs += step;

      int8_t next = (int8_t)olb_cannons[i].pos + olb_cannons[i].dir;
      if (next < OLB_SEA_MIN) {
        next = OLB_SEA_MIN + 1;
        olb_cannons[i].dir = 1;
      } else if (next > OLB_SEA_MAX) {
        next = OLB_SEA_MAX - 1;
        olb_cannons[i].dir = -1;
      }
      olb_cannons[i].pos = (uint8_t)next;
    }
  }
}

void olb_fire(uint8_t player, uint32_t now) {
  OlbCannon *c = &olb_cannons[player];

  if (now < c->reloadUntilMs) {
    return;
  }
  if (olb_ammo <= 0) {
    return;
  }
  if (olb_torpedos[player].active) {
    return;
  }

  int8_t sx = 0, sy = 0, dx = 0, dy = 0;
  switch (c->edge) {
    case OLB_EDGE_LEFT:   sx = 0;  sy = c->pos; dx =  1; dy =  0; break;
    case OLB_EDGE_RIGHT:  sx = 15; sy = c->pos; dx = -1; dy =  0; break;
    case OLB_EDGE_TOP:    sx = c->pos; sy = 0;  dx =  0; dy =  1; break;
    case OLB_EDGE_BOTTOM: sx = c->pos; sy = 15; dx =  0; dy = -1; break;
  }

  olb_torpedos[player].active     = true;
  olb_torpedos[player].x          = sx;
  olb_torpedos[player].y          = sy;
  olb_torpedos[player].dx         = dx;
  olb_torpedos[player].dy         = dy;
  olb_torpedos[player].lastStepMs = now;
  olb_torpedos[player].owner      = player;

  c->reloadUntilMs = now + OLB_RELOAD_MS;
  olb_ammo--;
  olb_shotsFired++;

  Serial.print(F("[FIRE] P"));
  Serial.print(player);
  Serial.print(F(" von ("));
  Serial.print(sx);
  Serial.print(F(","));
  Serial.print(sy);
  Serial.print(F(") | Munition: "));
  Serial.println(olb_ammo);
}

void olb_updateTorpedos(uint32_t now) {
  for (uint8_t i = 0; i < OLB_MAX_TORPEDOS; i++) {
    if (!olb_torpedos[i].active) {
      continue;
    }
    while (olb_torpedos[i].active &&
           (now - olb_torpedos[i].lastStepMs >= OLB_TORPEDO_STEP_MS)) {
      olb_torpedos[i].lastStepMs += OLB_TORPEDO_STEP_MS;
      olb_torpedos[i].x += olb_torpedos[i].dx;
      olb_torpedos[i].y += olb_torpedos[i].dy;
      olb_resolveCell(i, now);
    }
  }
}

void olb_resolveCell(uint8_t t, uint32_t now) {
  OlbTorpedo *tp = &olb_torpedos[t];
  int16_t x = tp->x;
  int16_t y = tp->y;

  if (!olb_inSea(x, y)) {
    tp->active = false;
    olb_dryShots++;
    Serial.print(F("[MISS] Bahn frei bis ("));
    Serial.print(x - tp->dx);
    Serial.print(F(","));
    Serial.print(y - tp->dy);
    Serial.println(F(")"));
    return;
  }

  uint8_t st   = olb_state[y][x];
  uint8_t ship = olb_ship[y][x];

  if (st == OLB_HIT) {
    tp->active = false;
    olb_dryShots++;
    olb_spawnFx((uint8_t)x, (uint8_t)y, OLB_FX_WRECK_TYPE, now);
    Serial.print(F("[WRCK] Aufprall auf Wrack ("));
    Serial.print(x);
    Serial.print(F(","));
    Serial.print(y);
    Serial.println(F(") - Schuss verschwendet"));
    return;
  }

  if (ship > 0 && st == OLB_FOG) {
    olb_state[y][x] = OLB_HIT;
    tp->active      = false;
    olb_dryShots    = 0;
    olb_shotsHit++;
    olb_score += OLB_SCORE_HIT;
    olb_spawnFx((uint8_t)x, (uint8_t)y, OLB_FX_HIT_TYPE, now);

    uint8_t index = ship - 1;
    olb_ships[index].hits++;

    Serial.print(F("[HIT ] ("));
    Serial.print(x);
    Serial.print(F(","));
    Serial.print(y);
    Serial.print(F(") Schiff #"));
    Serial.print(index);
    Serial.print(F(" | Trefferstand "));
    Serial.print(olb_ships[index].hits);
    Serial.print(F("/"));
    Serial.println(olb_ships[index].len);

    if (olb_ships[index].hits >= olb_ships[index].len) {
      olb_sinkShip(index, now);
    }
    return;
  }

  /* Wasser oder gesunkenes Wrack: aufdecken und weiterfliegen */
  if (st == OLB_FOG) {
    olb_state[y][x] = OLB_WATER;
  }
}

void olb_sinkShip(uint8_t index, uint32_t now) {
  OlbShip *s = &olb_ships[index];
  s->sunk        = true;
  s->sinkStartMs = now;
  olb_score += OLB_SCORE_SUNK;

  for (uint8_t i = 0; i < s->len; i++) {
    int16_t cx = s->horiz ? (int16_t)s->x + i : (int16_t)s->x;
    int16_t cy = s->horiz ? (int16_t)s->y     : (int16_t)s->y + i;
    olb_state[cy][cx] = OLB_SUNK;

    /* Nachbarzellen freigeben - dort kann regelkonform kein Schiff liegen */
    for (int8_t dy = -1; dy <= 1; dy++) {
      for (int8_t dx = -1; dx <= 1; dx++) {
        int16_t nx = cx + dx;
        int16_t ny = cy + dy;
        if (!olb_inSea(nx, ny)) {
          continue;
        }
        if (olb_state[ny][nx] == OLB_FOG && olb_ship[ny][nx] == 0) {
          olb_state[ny][nx] = OLB_WATER;
        }
      }
    }
  }

  Serial.print(F("[SUNK] Schiff #"));
  Serial.print(index);
  Serial.print(F(" versenkt | Punkte: "));
  Serial.println(olb_score);
}

void olb_spawnFx(uint8_t x, uint8_t y, uint8_t type, uint32_t now) {
  for (uint8_t i = 0; i < OLB_MAX_FX; i++) {
    if (!olb_fx[i].active) {
      olb_fx[i].active  = true;
      olb_fx[i].x       = x;
      olb_fx[i].y       = y;
      olb_fx[i].type    = type;
      olb_fx[i].startMs = now;
      return;
    }
  }
}

void olb_triggerSonar(uint32_t now) {
  uint8_t cx[OLB_MAX_SHIPS * OLB_MAX_SHIP_LEN];
  uint8_t cy[OLB_MAX_SHIPS * OLB_MAX_SHIP_LEN];
  uint8_t count = 0;

  for (uint8_t y = OLB_SEA_MIN; y <= OLB_SEA_MAX; y++) {
    for (uint8_t x = OLB_SEA_MIN; x <= OLB_SEA_MAX; x++) {
      if (olb_ship[y][x] > 0 && olb_state[y][x] == OLB_FOG) {
        if (count < OLB_MAX_SHIPS * OLB_MAX_SHIP_LEN) {
          cx[count] = x;
          cy[count] = y;
          count++;
        }
      }
    }
  }

  olb_dryShots = 0;
  if (count == 0) {
    return;
  }

  uint8_t pick = (uint8_t)random(count);
  olb_sonarX      = cx[pick];
  olb_sonarY      = cy[pick];
  olb_sonarStartMs = now;

  Serial.print(F("[SONR] Hinweis auf ("));
  Serial.print(olb_sonarX);
  Serial.print(F(","));
  Serial.print(olb_sonarY);
  Serial.println(F(")"));
}

/* ===========================================================================
   ZUSTAND: WELLE GESCHAFFT / GAME OVER
   =========================================================================== */
void olb_updateWaveClear(uint32_t now) {
  bool anyPress = false;
  for (uint8_t i = 0; i < 4; i++) {
    if (olb_pressEvent[i]) {
      anyPress = true;
    }
  }

  if (anyPress || (now - olb_stateStartMs >= OLB_WAVECLEAR_MS)) {
    olb_wave++;
    olb_startWave(now);
  }
}

void olb_updateGameOver(uint32_t now) {
  if (now - olb_stateStartMs < OLB_GAMEOVER_LOCK_MS) {
    return;
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (olb_pressEvent[i]) {
      for (uint8_t k = 0; k < 4; k++) {
        olb_cannons[k].ready = false;
      }
      olb_startPending = false;
      olb_resetBoard();
      olb_gameState    = OLB_STATE_TITLE;
      olb_stateStartMs = now;
      Serial.println(F("[BOOT] zurueck zum Titelbildschirm"));
      return;
    }
  }
}

/* ===========================================================================
   RENDERING
   =========================================================================== */
void olb_render(uint32_t now) {
  FastLED.clear();

  switch (olb_gameState) {

    case OLB_STATE_TITLE:
      olb_drawTitle(now);
      break;

    case OLB_STATE_PLAY:
      olb_drawSea();
      olb_drawSinkFx(now);
      olb_drawSonar(now);
      olb_drawFx(now);
      olb_drawTorpedos();
      olb_drawCannons(now);
      olb_drawAmmoCorners(now);
      break;

    case OLB_STATE_WAVECLEAR:
      olb_drawWaveClear(now);
      break;

    case OLB_STATE_GAMEOVER:
      olb_drawGameOver(now);
      break;
  }

  FastLED.show();
}

void olb_drawTitle(uint32_t now) {
  for (uint8_t row = 0; row < OLB_SEA_SIZE; row++) {
    for (uint8_t col = 0; col < OLB_SEA_SIZE; col++) {

      uint8_t src = col;
      if (row >= OLB_TITLE_WAVE_ROW0) {
        src = (col + olb_titleScroll) % OLB_SEA_SIZE;
      }

      char c = (char)pgm_read_byte(&(OLB_FRAME_TITLE[row][src]));
      CRGB color = CRGB::Black;

      switch (c) {
        case 'H': color = OLB_COL_HULL; break;
        case 'S': color = OLB_COL_SAIL; break;
        case 'M': color = OLB_COL_MAST; break;
        case 'R': color = OLB_COL_FLAG; break;
        case 'W': color = OLB_COL_WAVE; break;
        default:  continue;
      }
      olb_setPixel(col + OLB_SEA_MIN, row + OLB_SEA_MIN, color);
    }
  }

  /* Startblitz */
  if (olb_startPending) {
    bool on = ((now / 150) % 2) == 0;
    if (on) {
      for (uint8_t p = OLB_SEA_MIN; p <= OLB_SEA_MAX; p++) {
        olb_setPixel(0,  p,  OLB_COL_WHITE);
        olb_setPixel(15, p,  OLB_COL_WHITE);
        olb_setPixel(p,  0,  OLB_COL_WHITE);
        olb_setPixel(p,  15, OLB_COL_WHITE);
      }
    }
    return;
  }

  /* Bereitschaftsanzeige: eigene Bahn leuchtet in Spielerfarbe */
  for (uint8_t i = 0; i < 4; i++) {
    CRGB c = olb_cannons[i].ready ? olb_cannons[i].color
                                  : olb_scale(olb_cannons[i].color, 38);
    for (uint8_t p = OLB_SEA_MIN; p <= OLB_SEA_MAX; p++) {
      switch (olb_cannons[i].edge) {
        case OLB_EDGE_LEFT:   olb_setPixel(0,  p,  c); break;
        case OLB_EDGE_RIGHT:  olb_setPixel(15, p,  c); break;
        case OLB_EDGE_TOP:    olb_setPixel(p,  0,  c); break;
        case OLB_EDGE_BOTTOM: olb_setPixel(p,  15, c); break;
      }
    }
  }
}

void olb_drawSea() {
  for (uint8_t y = OLB_SEA_MIN; y <= OLB_SEA_MAX; y++) {
    for (uint8_t x = OLB_SEA_MIN; x <= OLB_SEA_MAX; x++) {
      CRGB c;
      switch (olb_state[y][x]) {
        case OLB_WATER: c = OLB_COL_WATER; break;
        case OLB_HIT:   c = OLB_COL_HIT;   break;
        case OLB_SUNK:  c = OLB_COL_SUNK;  break;
        default:        c = OLB_COL_FOG;   break;
      }
      olb_setPixel(x, y, c);
    }
  }
}

void olb_drawSinkFx(uint32_t now) {
  for (uint8_t i = 0; i < olb_shipCount; i++) {
    if (olb_ships[i].sinkStartMs == 0) {
      continue;
    }
    uint32_t dt = now - olb_ships[i].sinkStartMs;
    if (dt >= OLB_FX_SINK_MS) {
      continue;
    }
    /* 3 Blinkzyklen ueber die Animationsdauer */
    bool on = ((dt / (OLB_FX_SINK_MS / 6)) % 2) == 0;
    CRGB c = on ? OLB_COL_WHITE : OLB_COL_SUNK;

    for (uint8_t k = 0; k < olb_ships[i].len; k++) {
      int16_t cx = olb_ships[i].horiz ? (int16_t)olb_ships[i].x + k : (int16_t)olb_ships[i].x;
      int16_t cy = olb_ships[i].horiz ? (int16_t)olb_ships[i].y     : (int16_t)olb_ships[i].y + k;
      olb_setPixel(cx, cy, c);
    }
  }
}

void olb_drawSonar(uint32_t now) {
  if (olb_sonarX < 0) {
    return;
  }
  uint32_t dt = now - olb_sonarStartMs;
  if (dt >= OLB_SONAR_MS) {
    olb_sonarX = -1;
    olb_sonarY = -1;
    return;
  }
  bool on = ((dt / (OLB_SONAR_MS / 6)) % 2) == 0;
  if (on) {
    olb_setPixel(olb_sonarX, olb_sonarY, OLB_COL_WHITE);
  }
}

void olb_drawFx(uint32_t now) {
  for (uint8_t i = 0; i < OLB_MAX_FX; i++) {
    if (!olb_fx[i].active) {
      continue;
    }
    uint32_t dt = now - olb_fx[i].startMs;
    if (dt >= OLB_FX_HIT_MS) {
      olb_fx[i].active = false;
      continue;
    }

    if (olb_fx[i].type == OLB_FX_HIT_TYPE) {
      olb_setPixel(olb_fx[i].x, olb_fx[i].y, OLB_COL_WHITE);
      CRGB halo = olb_scale(OLB_COL_WHITE, 128);
      if (olb_inSea(olb_fx[i].x + 1, olb_fx[i].y)) olb_setPixel(olb_fx[i].x + 1, olb_fx[i].y, halo);
      if (olb_inSea(olb_fx[i].x - 1, olb_fx[i].y)) olb_setPixel(olb_fx[i].x - 1, olb_fx[i].y, halo);
      if (olb_inSea(olb_fx[i].x, olb_fx[i].y + 1)) olb_setPixel(olb_fx[i].x, olb_fx[i].y + 1, halo);
      if (olb_inSea(olb_fx[i].x, olb_fx[i].y - 1)) olb_setPixel(olb_fx[i].x, olb_fx[i].y - 1, halo);
    } else {
      olb_setPixel(olb_fx[i].x, olb_fx[i].y, OLB_COL_WRECKFX);
    }
  }
}

void olb_drawTorpedos() {
  for (uint8_t i = 0; i < OLB_MAX_TORPEDOS; i++) {
    if (!olb_torpedos[i].active) {
      continue;
    }
    CRGB c = olb_cannons[olb_torpedos[i].owner].color;

    int16_t tx = olb_torpedos[i].x - olb_torpedos[i].dx;
    int16_t ty = olb_torpedos[i].y - olb_torpedos[i].dy;
    if (olb_inSea(tx, ty)) {
      olb_setPixel(tx, ty, olb_scale(c, 102));   /* Schweif 40% */
    }
    if (olb_inSea(olb_torpedos[i].x, olb_torpedos[i].y)) {
      olb_setPixel(olb_torpedos[i].x, olb_torpedos[i].y, c);
    }
  }
}

void olb_drawCannons(uint32_t now) {
  for (uint8_t i = 0; i < 4; i++) {
    CRGB c = olb_cannons[i].color;
    if (now < olb_cannons[i].reloadUntilMs) {
      c = olb_scale(c, 51);                      /* Nachladen: 20% */
    }
    switch (olb_cannons[i].edge) {
      case OLB_EDGE_LEFT:   olb_setPixel(0,  olb_cannons[i].pos, c); break;
      case OLB_EDGE_RIGHT:  olb_setPixel(15, olb_cannons[i].pos, c); break;
      case OLB_EDGE_TOP:    olb_setPixel(olb_cannons[i].pos, 0,  c); break;
      case OLB_EDGE_BOTTOM: olb_setPixel(olb_cannons[i].pos, 15, c); break;
    }
  }
}

void olb_drawAmmoCorners(uint32_t now) {
  if (olb_ammoStart <= 0) {
    return;
  }
  uint8_t pct = (uint8_t)(((int32_t)olb_ammo * 100) / olb_ammoStart);

  CRGB c;
  if (pct > 50) {
    c = OLB_COL_AMMO_OK;
  } else if (pct > 25) {
    c = OLB_COL_AMMO_MID;
  } else if (pct > 10) {
    c = OLB_COL_AMMO_LOW;
  } else {
    c = (((now / 500) % 2) == 0) ? OLB_COL_AMMO_LOW : CRGB::Black;
  }

  olb_setPixel(0,  0,  c);
  olb_setPixel(15, 0,  c);
  olb_setPixel(0,  15, c);
  olb_setPixel(15, 15, c);
}

void olb_drawWaveClear(uint32_t now) {
  CRGB c = olb_scale(OLB_COL_GREEN, (uint8_t)(80 + (cos8((uint8_t)(now / 6)) >> 1)));

  olb_drawSymbolFrame(OLB_FRAME_CLEAR, c);

  /* Wellenzaehler in der untersten Meereszeile */
  uint8_t dots = olb_wave;
  if (dots > OLB_SEA_SIZE) {
    dots = OLB_SEA_SIZE;
  }
  for (uint8_t i = 0; i < dots; i++) {
    olb_setPixel(OLB_SEA_MIN + i, OLB_SEA_MAX, OLB_COL_SUNK);
  }
}

void olb_drawGameOver(uint32_t now) {
  /* Erst die tatsaechliche Flottenlage aufdecken */
  if (now - olb_stateStartMs < OLB_GAMEOVER_REVEAL_MS) {
    for (uint8_t y = OLB_SEA_MIN; y <= OLB_SEA_MAX; y++) {
      for (uint8_t x = OLB_SEA_MIN; x <= OLB_SEA_MAX; x++) {
        if (olb_ship[y][x] > 0 && olb_state[y][x] == OLB_FOG) {
          olb_setPixel(x, y, OLB_COL_REVEAL);
        }
      }
    }
  }

  /* Wellenzaehler in der untersten Zeile */
  uint8_t dots = olb_wave;
  if (dots > OLB_SEA_SIZE) {
    dots = OLB_SEA_SIZE;
  }
  for (uint8_t i = 0; i < dots; i++) {
    olb_setPixel(OLB_SEA_MIN - 1 + i, OLB_SEA_MAX + 1, OLB_COL_SUNK);
  }

  CRGB c = olb_scale(CRGB(255, 0, 0), (uint8_t)(60 + (cos8((uint8_t)(now / 4)) >> 1)));
  olb_drawSymbolFrame(OLB_FRAME_GAMEOVER, c);
}

void olb_drawSymbolFrame(const char frame[][OLB_SEA_SIZE + 1], CRGB color) {
  for (uint8_t row = 0; row < OLB_SEA_SIZE; row++) {
    for (uint8_t col = 0; col < OLB_SEA_SIZE; col++) {
      char ch = (char)pgm_read_byte(&(frame[row][col]));
      if (ch == 'X') {
        olb_setPixel(col + OLB_SEA_MIN, row + OLB_SEA_MIN, color);
      }
    }
  }
}

/* ===========================================================================
   STATUS / STATISTIK
   =========================================================================== */
uint8_t olb_shipsRemaining() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < olb_shipCount; i++) {
    if (!olb_ships[i].sunk) {
      n++;
    }
  }
  return n;
}

bool olb_torpedosActive() {
  for (uint8_t i = 0; i < OLB_MAX_TORPEDOS; i++) {
    if (olb_torpedos[i].active) {
      return true;
    }
  }
  return false;
}

void olb_logStats() {
  uint16_t pct = 0;
  if (olb_shotsFired > 0) {
    pct = (uint16_t)(((uint32_t)olb_shotsHit * 100) / olb_shotsFired);
  }
  Serial.print(F("[OVER] Welle "));
  Serial.print(olb_wave);
  Serial.print(F(" | Punkte: "));
  Serial.print(olb_score);
  Serial.print(F(" | Treffer "));
  Serial.print(olb_shotsHit);
  Serial.print(F("/"));
  Serial.print(olb_shotsFired);
  Serial.print(F(" ("));
  Serial.print(pct);
  Serial.println(F("%)"));
}
