#include <Arduino.h>
#include <FastLED.h>
#include <math.h>
#include "olb_init_v1.h"

#define LED_PIN     2
#define NUM_LEDS    256  // 16x16 Matrix
#define BRIGHTNESS  50   // 20% Helligkeit
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

// Pinbelegung der Buttons
#define BUTTON_1 D5 // gelb
#define BUTTON_2 D2 // grün
#define BUTTON_3 D6 // blau
#define BUTTON_4 D1 // weiß
#define BUTTON_5 D7 // unbenutzt

// Farben für die Buttons
CRGB buttonColors[] = {
    CRGB::Yellow,    
    CRGB::Green,  
    CRGB::Blue,   
    CRGB::White,  
};

// LED-Matrix Array (16x16)
int ledMatrix[16][16];
int matrixRotation = 0; // Rotation des Levels und des Timers 0, 1, 2 oder 3 (je 90°)

// Geschwindigkeit in LEDs pro Sekunde
const float SPEED_INCREMENT = 0.5;
float speedX = 0.0;
float speedY = 0.0;

// Position des Punkts
float posX = 7.0;
float posY = 8.0;

// Position des grünen Punkts
int targetX = -1;
int targetY = -1;

// Punkte zählen
int score = 0;

// Leben zählen
int lives = 3;

// für Timer pro Level
unsigned long startTime;
int timerSpeed = 3000;

const int digit_0[5] = {
    0b11110000,
    0b10010000,
    0b10010000,
    0b10010000,
    0b11110000
};

const int digit_1[5] = {
    0b00100000,
    0b01100000,
    0b00100000,
    0b00100000,
    0b00100000
};

const int digit_2[5] = {
    0b11110000,
    0b00010000,
    0b11110000,
    0b10000000,
    0b11110000
};

const int digit_3[5] = {
    0b11110000,
    0b00010000,
    0b11110000,
    0b00010000,
    0b11110000
};

const int digit_4[5] = {
    0b10010000,
    0b10010000,
    0b11110000,
    0b00010000,
    0b00010000
};

const int digit_5[5] = {
    0b11110000,
    0b10000000,
    0b11110000,
    0b00010000,
    0b11110000
};

const int digit_6[5] = {
    0b11110000,
    0b10000000,
    0b11110000,
    0b10010000,
    0b11110000
};

const int digit_7[5] = {
    0b11110000,
    0b00010000,
    0b00010000,
    0b00010000,
    0b00010000
};

const int digit_8[5] = {
    0b11110000,
    0b10010000,
    0b11110000,
    0b10010000,
    0b11110000
};

const int digit_9[5] = {
    0b11110000,
    0b10010000,
    0b11110000,
    0b00010000,
    0b11110000
};

// =============================================================================
// BITMAP-RASTER IM FLASH (PROGMEM)
// Farbcodes: 0:Schwarz, 1:Grün, 2:Gelb, 3:Rot, 6:Weiß, 7:Blau
// =============================================================================

// Screens
const uint8_t PROGMEM SCREEN_SETUP[16][16] = {
    {3,3,0,0,0,0,0,0,0,3,3,0,3,0,3,0},
    {3,0,3,0,0,0,0,0,3,0,0,0,3,0,3,0},
    {3,3,0,0,3,0,3,0,0,3,0,0,3,3,3,0},
    {3,0,0,0,3,0,3,0,0,0,3,0,3,0,3,0},
    {3,0,0,0,3,3,3,0,3,3,0,0,3,0,3,0},
    {7,7,0,0,0,0,0,7,7,7,0,0,7,7,0,0},
    {7,0,7,0,0,0,0,0,7,0,0,7,0,0,0,2},
    {7,7,0,0,7,0,7,0,7,0,0,0,7,0,0,0},
    {7,0,7,0,7,0,7,0,7,0,0,0,0,7,0,2},
    {7,7,0,0,7,7,7,0,7,0,0,7,7,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,0,1,1,1,0,1,0,0,1,1,0,1,1,1},
    {1,0,0,0,1,0,0,1,0,0,1,0,1,0,1,0},
    {0,1,0,0,1,0,1,0,1,0,1,1,0,0,1,0},
    {0,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0},
    {1,1,0,0,1,0,1,0,1,0,1,0,1,0,1,0}
};

const uint8_t PROGMEM SCREEN_GAMEOVER[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,0,0,0,1,0,0,1,0,1,0,1,1,1,0},
    {1,0,0,0,1,0,1,0,1,1,1,0,1,0,0,0},
    {1,0,0,0,1,0,1,0,1,0,1,0,1,1,0,0},
    {1,0,1,0,1,1,1,0,1,0,1,0,1,0,0,0},
    {0,1,0,0,1,0,1,0,1,0,1,0,1,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,3,0,0,3,0,3,0,3,3,3,0,3,3,0},
    {0,3,0,3,0,3,0,3,0,3,0,0,0,3,0,3},
    {0,3,0,3,0,3,0,3,0,3,3,0,0,3,3,0},
    {0,3,0,3,0,0,3,0,0,3,0,0,0,3,0,3},
    {0,0,3,0,0,0,3,0,0,3,3,3,0,3,0,3},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

const uint8_t PROGMEM SCREEN_LEVEL[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,1,0,0,1,0,1,0,0,1,0,0,1},
    {1,0,0,1,1,1,0,1,0,1,0,1,1,1,0,1},
    {1,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1},
    {1,1,0,0,1,1,0,0,1,0,0,0,1,1,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

const uint8_t PROGMEM SCREEN_LIVES[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,2,0,0,2,0,0,0,0,0,0,0,0,0,0,0},
    {0,2,0,0,0,0,2,0,2,0,0,2,0,0,0,2},
    {0,2,0,0,2,0,2,0,2,0,2,2,2,0,2,0},
    {0,2,0,0,2,0,2,0,2,0,2,0,0,0,0,2},
    {0,2,2,0,2,0,0,2,0,0,0,2,2,0,2,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

const uint8_t PROGMEM SCREEN_PKT[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,6,6,0,0,6,0,6,0,6,6,6,0,0,0},
    {0,0,6,0,6,0,6,0,6,0,0,6,0,0,0,0},
    {0,0,6,6,0,0,6,6,0,0,0,6,0,0,0,0},
    {0,0,6,0,0,0,6,0,6,0,0,6,0,0,0,0},
    {0,0,6,0,0,0,6,0,6,0,0,6,0,0,6,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};


// Level 01 bis 10
const uint8_t PROGMEM LEVELS[13][16][16] = {
    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,1,0,0,0,0,0,0,0,0,3,0,2,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    // Level 02: Zwei vertikale Balken
    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,3,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,2,0,3,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,0,3,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,0,3,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,0,3,0,0,0,0,0,0,0,0,3,0,1,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },

    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,3,3,0,0,3,3,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,3,0,2,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    // Level 04:
    {
        {3,0,0,0,3,0,0,2,0,0,3,0,0,0,3,0},
        {3,0,0,0,3,0,0,0,0,0,3,0,0,0,3,0},
        {0,3,0,3,0,0,0,0,0,0,0,3,0,3,0,0},
        {0,3,0,3,0,0,0,0,0,0,0,3,0,3,0,0},
        {0,0,3,0,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,3,0,0,0,0,0,0,0,0,0,3,0,0,0},
        {0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0},
        {0,0,0,0,0,0,3,0,3,0,0,0,0,0,0,0},
        {0,0,3,0,0,0,3,0,3,0,0,0,3,0,0,0},
        {0,0,3,0,0,0,0,3,0,0,0,0,3,0,0,0},
        {0,3,0,3,0,0,0,3,0,0,0,3,0,3,0,0},
        {0,3,0,3,0,0,0,0,0,0,0,3,0,3,0,0},
        {3,0,0,0,3,0,0,0,0,0,3,0,0,0,3,0},
        {3,0,0,0,3,0,0,0,0,0,3,0,0,0,3,0},
        {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0}
    },
    // Level 05:
    {
        {0,0,0,0,0,0,0,0,2,0,3,3,3,3,3,3},
        {0,0,3,3,0,3,3,0,0,0,3,3,3,3,3,3},
        {0,3,3,3,3,3,3,3,0,0,0,3,3,3,3,3},
        {0,3,3,3,3,3,3,3,0,0,0,0,3,3,3,0},
        {0,0,3,3,3,3,3,0,0,0,0,0,0,3,0,0},
        {0,0,0,3,3,3,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,3,3,0,3,3},
        {0,3,3,0,3,3,0,0,0,0,3,3,3,3,3,3},
        {3,3,3,3,3,3,3,0,0,0,3,3,3,3,3,3},
        {3,3,3,3,3,3,3,0,0,0,0,3,3,3,3,3},
        {0,3,3,3,3,3,0,0,0,0,0,0,3,3,3,0},
        {0,0,3,3,3,0,0,0,0,0,0,0,0,3,0,0},
        {0,1,0,3,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    // Level 06:
    {
        {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
        {0,3,0,0,0,0,0,0,2,0,0,0,0,0,3,0},
        {0,0,3,0,0,0,0,0,0,0,0,0,0,3,0,0},
        {0,0,0,3,0,0,0,3,3,0,0,0,3,0,0,0},
        {0,0,0,0,3,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,0,3,0,0,3,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,3,0,0,3,0,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,3,0,0,0,0,0,0,3,0,0,0,0},
        {0,0,0,3,0,0,0,3,3,0,0,0,3,0,0,0},
        {0,0,3,0,0,0,0,0,0,0,0,0,0,3,0,0},
        {0,3,0,0,0,0,0,1,0,0,0,0,0,0,3,0},
        {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    // Level 07:
    {
        {0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {3,0,3,0,3,0,3,0,3,0,3,0,3,0,3,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,3,0,3,0,3,0,3,0,3,0,3,0,3,0,3},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0},
        {0,0,3,3,0,0,0,3,3,0,0,0,3,3,0,0},
        {0,0,3,3,0,0,0,3,3,0,0,0,3,3,0,0},
        {0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,3,3,3,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    // Level 08:
    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0},
        {3,3,0,0,0,0,0,0,3,3,3,0,0,0,0,0},
        {0,3,3,3,0,0,0,0,0,0,3,3,3,0,0,0},
        {0,0,0,3,3,3,0,0,0,0,0,0,3,3,3,0},
        {0,0,0,0,0,3,3,3,0,0,0,0,0,0,3,3},
        {0,0,0,0,0,0,0,3,3,3,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,3,3,3,0,0,0,0},
        {0,0,3,3,3,0,0,0,0,0,0,3,3,3,0,0},
        {0,0,0,0,3,3,3,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,3,3,3,0,0,0,0,0,0,0},
        {3,3,0,0,0,0,0,0,3,3,3,0,0,0,0,0},
        {0,3,3,3,0,0,0,0,0,0,3,3,3,0,0,0},
        {0,0,0,3,3,3,0,0,0,0,0,0,3,3,3,0},
        {0,0,0,0,0,3,3,3,0,0,0,0,0,0,3,3},
        {0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0}
    },
    // Level 09:
    {
        {3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},
        {3,0,0,0,0,0,0,0,0,3,3,0,0,0,0,3},
        {3,0,2,0,0,0,0,0,0,3,0,0,0,3,0,3},
        {3,0,0,0,0,0,3,0,0,3,0,0,0,3,0,3},
        {3,0,0,0,0,0,3,0,0,0,0,0,3,3,0,3},
        {3,0,0,0,0,3,3,0,0,0,0,0,3,0,0,3},
        {3,0,0,0,0,3,0,0,0,3,0,0,3,0,0,3},
        {3,0,3,0,0,3,0,0,0,3,0,0,0,0,0,3},
        {3,0,3,0,0,0,0,0,3,3,0,0,0,0,0,3},
        {3,3,3,0,0,0,0,0,3,0,0,0,3,0,0,3},
        {3,3,0,0,0,3,0,0,3,0,0,0,3,0,0,3},
        {3,3,0,0,0,3,0,0,0,0,0,3,3,0,0,3},
        {3,0,0,0,3,3,0,0,0,0,0,3,0,0,0,3},
        {3,0,0,0,3,0,0,0,3,0,0,3,0,0,0,3},
        {3,0,0,0,3,0,0,0,3,0,0,0,0,0,0,3},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0}
    },
    // Level 10:
    {
        {3,3,0,0,3,3,0,0,3,3,0,0,3,3,0,2},
        {3,3,0,0,3,3,0,0,3,3,0,0,3,3,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,3,3,0,0,3,3,0,0,3,3,0,0,3,3},
        {0,0,3,3,0,0,3,3,0,0,3,3,0,0,3,3},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {3,3,0,0,3,3,0,0,3,3,0,0,3,3,0,0},
        {3,3,0,0,3,3,0,0,3,3,0,0,3,3,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,3,3,0,0,3,3,0,0,3,3,0,0,3,3},
        {1,0,3,3,0,0,3,3,0,0,3,3,0,0,3,3},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    // Level 11:
    {
        {3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},
        {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
        {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
        {3,0,0,0,3,3,3,0,0,3,3,3,0,0,0,3},
        {3,0,0,0,3,0,0,0,0,0,0,3,0,0,0,3},
        {3,3,0,0,3,0,0,2,0,0,0,3,0,0,3,3},
        {0,3,0,0,3,3,3,3,3,3,3,3,0,0,3,0},
        {0,3,0,0,0,0,0,0,0,0,0,0,0,0,3,0},
        {0,3,0,0,0,0,0,0,0,0,0,0,0,0,3,0},
        {0,3,0,0,3,3,3,3,3,3,3,3,0,0,3,0},
        {0,3,0,0,3,0,0,0,0,0,0,3,0,0,3,0},
        {3,3,0,0,3,3,0,0,0,0,3,3,0,0,3,3},
        {3,0,0,0,0,3,0,1,0,0,3,0,0,0,0,3},
        {3,0,0,0,0,3,0,0,0,0,3,0,0,0,0,3},
        {3,0,0,0,0,3,0,0,0,0,3,0,0,0,0,3},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    // Level 12:
    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0},
        {0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0},
        {3,3,0,0,3,0,0,3,3,3,3,3,3,3,3,3},
        {0,0,0,0,3,0,0,3,0,0,0,0,0,0,0,0},
        {0,0,0,0,3,0,0,3,0,0,0,0,0,0,0,0},
        {0,0,0,0,3,0,0,3,0,0,3,3,3,3,0,0},
        {0,0,0,0,3,0,0,3,0,0,3,0,0,0,0,0},
        {0,1,0,0,3,0,0,3,0,0,3,0,0,0,0,0},
        {0,0,0,0,3,0,0,3,0,0,3,0,0,3,3,3},
        {0,0,0,0,3,0,0,3,0,0,3,0,0,0,0,0},
        {0,0,0,0,3,0,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,3,3,3,3,3,3,3,3,3,3,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,2}
    },
    //Level 13:
    {
        {3,3,3,3,3,3,3,3,3,3,3,3,3,0,1,3},
        {3,0,0,0,0,0,0,0,0,0,0,0,3,0,0,3},
        {3,0,0,0,0,0,0,0,0,0,0,0,3,0,0,3},
        {3,0,0,3,3,3,3,3,3,3,0,0,3,0,0,3},
        {3,0,0,3,0,0,0,0,0,3,0,0,3,0,0,3},
        {3,0,0,3,0,0,0,0,0,3,0,0,3,0,0,3},
        {3,0,0,3,0,0,3,0,0,3,0,0,3,0,0,3},
        {3,0,0,3,0,0,3,0,2,3,0,0,3,0,0,3},
        {3,0,0,3,0,0,3,3,3,3,0,0,3,0,0,3},
        {3,0,0,3,0,0,0,0,0,0,0,0,3,0,0,3},
        {3,0,0,3,0,0,0,0,0,0,0,0,3,0,0,3},
        {3,0,0,3,3,3,3,3,3,3,3,3,3,0,0,3},
        {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
        {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
        {3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
};

// Funktion zur Berechnung des Index in der Schlangenlinien-Reihenfolge
int getIndexFromMatrix(int x, int y) {
    if (y % 2 == 0) {
        return y * 16 + x;  // Gerade Zeilen: von links nach rechts
    } else {
        return y * 16 + (15 - x);  // Ungerade Zeilen: von rechts nach links
    }
}

// Funktion zum Setzen der Matrixfarbe
void setMatrixColor(CRGB color) {
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
}

// Tastendruck
void waitForAnyButton() {
    while (digitalRead(BUTTON_1) == HIGH && digitalRead(BUTTON_2) == HIGH &&
           digitalRead(BUTTON_3) == HIGH && digitalRead(BUTTON_4) == HIGH) {
        delay(100);
    }
}

// Funktion zum Anzeigen der Farben, bis alle Buttons gleichzeitig gedrückt werden
void waitForAllButtons() {
    while (true) {
        // Matrix löschen
        fill_solid(leds, NUM_LEDS, CRGB::Black);

        if (digitalRead(BUTTON_4) == LOW) {
            // Unterer Rand einfärben
            for (int x = 0; x < 16; x++) {
                leds[getIndexFromMatrix(x, 15)] = buttonColors[3];
            }
        } 
        if (digitalRead(BUTTON_2) == LOW) {
            // Rechter Rand einfärben
            for (int y = 0; y < 16; y++) {
                leds[getIndexFromMatrix(0, y)] = buttonColors[1];
            }
        } 
        if (digitalRead(BUTTON_1) == LOW) {
            // Linker Rand einfärben
            for (int y = 0; y < 16; y++) {
                leds[getIndexFromMatrix(15, y)] = buttonColors[0];
            }
        } 
        if (digitalRead(BUTTON_3) == LOW) {
            // Oberer Rand einfärben
            for (int x = 0; x < 16; x++) {
                leds[getIndexFromMatrix(x, 0)] = buttonColors[2];
            }
        }

        FastLED.show();

        // Überprüfen, ob alle Buttons gleichzeitig gedrückt sind
        if (digitalRead(BUTTON_1) == LOW && digitalRead(BUTTON_2) == LOW &&
            digitalRead(BUTTON_3) == LOW && digitalRead(BUTTON_4) == LOW) {
            break;
        }

        delay(50); // Entprellung der Buttons
    }
}

// Funktion zur Ausgabe des Matrix-Arrays auf die LED-Matrix
void displayMatrix() {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int index = getIndexFromMatrix(x, y);
            if (ledMatrix[x][y] == 0) {
                leds[index] = CRGB::Black;
            } else if (ledMatrix[x][y] == 1) {
                if ((speedX == 0.0) && (speedY == 0.0)) {
                    leds[index] = CRGB(0, 70, 0);   // dunkelgrün
                } else {
                    leds[index] = CRGB(0, 170, 0);  // hellgrün bei Bewegung
                }
            } else if (ledMatrix[x][y] == 2) {
                leds[index] = CRGB::Yellow;
            } else if (ledMatrix[x][y] == 3) {
                leds[index] = CRGB::Red;
            } else if (ledMatrix[x][y] == 4) {
                leds[index] = CRGB::Orange;
            } else if (ledMatrix[x][y] == 5) {
                leds[index] = CRGB::Coral;
            } else if (ledMatrix[x][y] == 6) {
                leds[index] = CRGB::White;
            } else if (ledMatrix[x][y] == 7) {
                leds[index] = CRGB::Blue;
            }
        }
    }
    
    FastLED.show();
}

void explodeAt(int x, int y, int colors[]) {
    for (int r = 0; r < 5; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int newX = x + dx;
                int newY = y + dy;
                if (newX >= 0 && newX < 16 && newY >= 0 && newY < 16) {
                    int distance = abs(dx) + abs(dy);
                    if (distance <= r) {
                        ledMatrix[newX][newY] = colors[min(distance, 2)]; 
                    }
                }
            }
        }
        displayMatrix();
        FastLED.delay(100);
    }
  
    for (int i = 0; i < 256; i += 51) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                ledMatrix[x][y] = 0;
            }
        }
        displayMatrix();
        FastLED.delay(50);
    }
  
    FastLED.clear();
    FastLED.show();
}

// Lädt einen Vollbild-Screen aus dem Flash (mit 15-x / 15-y Mapping wie im alten loadBMP)
void loadScreen(const uint8_t screen[16][16]) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            ledMatrix[x][y] = pgm_read_byte(&(screen[y][15 - x]));
        }
    }
}


// Lädt ein Level aus dem Flash (mit 15-x / 15-y Mapping wie im alten loadBMP)
void loadLevel(int levelIdx) {
    int idx = (levelIdx - 1) % 13; // <-- HIER: auf 13 geändert!
    if (idx < 0) idx = 0;
    
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            uint8_t val = pgm_read_byte(&(LEVELS[idx][y][x]));
            ledMatrix[15 - x][15 - y] = val;
            if (val == 1) { // Spielpunkt
                posX = 15 - x;
                posY = 15 - y;
            } else if (val == 2) { // Snatch
                targetX = 15 - x;
                targetY = 15 - y;
            }
        }
    }
}

// dreht die Matrix / Level zufällig
void rotateMatrixRandom() {
    matrixRotation = random(4);
    int temp[16][16];
    float newPosX, newPosY;
    int newTargetX, newTargetY;

    for (int i = 0; i < matrixRotation; i++) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                temp[x][15 - y] = ledMatrix[y][x];
            }
        }
        memcpy(ledMatrix, temp, sizeof(ledMatrix));

        newPosX = posY;
        newPosY = 15 - posX;
        posX = newPosX;
        posY = newPosY;

        newTargetX = targetY;
        newTargetY = 15 - targetX;
        targetX = newTargetX;
        targetY = newTargetY;
    }
}

// Funktion zur Aktualisierung der Position des Punkts
void updatePosition() {
    // Löschen der aktuellen Position
    int currentX = round(posX);
    int currentY = round(posY);
    ledMatrix[currentX][currentY] = 0;

    // Neue Position berechnen
    posX += speedX * 0.02; // 20 ms = 0.02 Sekunden
    posY += speedY * 0.02;

    // Begrenzung auf den Bereich der Matrix
    if (posX < 0) {
        posX = 0;
        speedX = 0;
    } else if (posX > 15) {
        posX = 15;
        speedX = 0;
    }

    if (posY < 0) {
        posY = 0;
        speedY = 0;
    } else if (posY > 15) {
        posY = 15;
        speedY = 0;
    }

    currentX = round(posX);
    currentY = round(posY);

    // Crash?
    if (ledMatrix[currentX][currentY] == 3) {
        // rote Explosion zeichnen
        int colors[3] = {2, 4, 3};
        explodeAt(currentX, currentY, colors);
        lives -= 1;
        if (lives == 0) {
            score = 0;
            loadScreen(SCREEN_GAMEOVER);
            displayMatrix();
            delay(500);
            waitForAnyButton();
        }
        speedX = 0;
        speedY = 0;
        return;
    }

    // Neue Position setzen
    ledMatrix[currentX][currentY] = 1;

    // Wenn der gelbe Punkt den grünen Punkt erreicht, ein Punkt hinzufügen und neuen grünen Punkt setzen
    if (currentX == targetX && currentY == targetY) {
        score++;  // Punkt erhöhen
        // grüne Explosion 
        int colors[3] = {1, 5, 1};
        explodeAt(currentX, currentY, colors);
        speedX = 0;
        speedY = 0;
    }

    // Timer an der rotierten Seite
    int timer = int((millis() - startTime) / timerSpeed);
    for (int i = 0; i < timer; i++) {
        switch (matrixRotation) {
            case 0: ledMatrix[15 - i][0]  = 3; break; // oben, rechts nach links (vorher: unten)
            case 1: ledMatrix[0][i]     = 3; break; // links, oben nach unten (vorher: rechts)
            case 2: ledMatrix[i][15]    = 3; break; // unten, links nach rechts (vorher: oben)
            case 3: ledMatrix[15][15 - i] = 3; break; // rechts, unten nach oben (vorher: links)
        }
    }
    
    if (timer == 16) {
        // rote Explosion zeichnen
        int colors[3] = {2, 4, 3};
        explodeAt(currentX, currentY, colors);
        lives -= 1;
        if (lives == 0) {
            score = 0;
            loadScreen(SCREEN_GAMEOVER);            
            displayMatrix();
            waitForAnyButton();
        }
        speedX = 0;
        speedY = 0;
        startTime = millis();
    }

    // Matrix aktualisieren
    displayMatrix();
}

// Zeigt die Anschlüsse
void setupScreen() {
    loadScreen(SCREEN_SETUP);
    displayMatrix();
    FastLED.show();
    waitForAnyButton();
    FastLED.clear();
}

// zeigt vor jedem Level die Level-Nummer
void showLevel() {
    loadScreen(SCREEN_LEVEL);

    const int* digits[] = {digit_0, digit_1, digit_2, digit_3, digit_4, 
                           digit_5, digit_6, digit_7, digit_8, digit_9};
    
    int level = score + 1;
    int zehner = level / 10;
    int einser = level % 10;
    
    // Funktion zum Schreiben einer Ziffer
    auto writeDigit = [&](int digit, int xOffset) {
        const int* digitArray = digits[digit];
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 4; j++) {
                ledMatrix[xOffset - j][9 + i] = (digitArray[i] & (0x80 >> j)) ? 2 : 0;
            }
        }
    };
    
    // Zehner und Einser schreiben
    if (zehner > 0) writeDigit(zehner, 12);
    writeDigit(einser, 6);

    displayMatrix();
    delay(500);
    waitForAnyButton();
}

// zeigt die verbliebenen Leben
void showLives() {
    loadScreen(SCREEN_LIVES);

    for (int i = 0; i < lives; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                ledMatrix[4 + i * 3 + k][10 + j] = 1;
            }
        }
    }
    displayMatrix();
    delay(500);
    waitForAnyButton();
}

void setup() {
    // LED-Setup
    Serial.begin(115200);
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

    // Startanimation
    olb_v1_playIntro(leds);

    FastLED.setBrightness(BRIGHTNESS);

    // Button-Pins als Eingang konfigurieren
    pinMode(BUTTON_1, INPUT_PULLUP);
    pinMode(BUTTON_2, INPUT_PULLUP);
    pinMode(BUTTON_3, INPUT_PULLUP);
    pinMode(BUTTON_4, INPUT_PULLUP);

    // Initialisierung des LED-Matrix-Arrays
    memset(ledMatrix, 0, sizeof(ledMatrix));
 
    setupScreen();

    waitForAllButtons();
}

void loop() {
    // Game over?
    if (lives == 0) {
        score = 0;
        lives = 3; 
    }

    // Level anzeigen
    showLevel();
    showLives();

    loadLevel(score + 1);
    rotateMatrixRandom();
    displayMatrix();
    delay(500);

    int oldScore = score;
    int oldLives = lives;
    startTime = millis();

    int lastBtn1 = digitalRead(BUTTON_1);
    int lastBtn2 = digitalRead(BUTTON_2);
    int lastBtn3 = digitalRead(BUTTON_3);
    int lastBtn4 = digitalRead(BUTTON_4);

    unsigned long lastDebounceTime1 = 0;
    unsigned long lastDebounceTime2 = 0;
    unsigned long lastDebounceTime3 = 0;
    unsigned long lastDebounceTime4 = 0;
    const unsigned long DEBOUNCE_DELAY = 30; // 30 ms Sperrzeit laut TSD

    while (oldScore == score && oldLives == lives) {
        unsigned long currentMillis = millis();

        int btn1 = digitalRead(BUTTON_1);
        int btn2 = digitalRead(BUTTON_2);
        int btn3 = digitalRead(BUTTON_3);
        int btn4 = digitalRead(BUTTON_4);

        // Überprüfen auf Tastendruck (Flanke HIGH -> LOW) mit Entprell-Sperrzeit
        if (btn1 == LOW && lastBtn1 == HIGH && (currentMillis - lastDebounceTime1 > DEBOUNCE_DELAY)) {
            speedX -= SPEED_INCREMENT;
            lastDebounceTime1 = currentMillis;
        }
        if (btn2 == LOW && lastBtn2 == HIGH && (currentMillis - lastDebounceTime2 > DEBOUNCE_DELAY)) {
            speedX += SPEED_INCREMENT;
            lastDebounceTime2 = currentMillis;
        }
        if (btn3 == LOW && lastBtn3 == HIGH && (currentMillis - lastDebounceTime3 > DEBOUNCE_DELAY)) {
            speedY += SPEED_INCREMENT;
            lastDebounceTime3 = currentMillis;
        }
        if (btn4 == LOW && lastBtn4 == HIGH && (currentMillis - lastDebounceTime4 > DEBOUNCE_DELAY)) {
            speedY -= SPEED_INCREMENT;
            lastDebounceTime4 = currentMillis;
        }

        lastBtn1 = btn1;
        lastBtn2 = btn2;
        lastBtn3 = btn3;
        lastBtn4 = btn4;
        
        // Position aktualisieren und Matrix neu zeichnen
        updatePosition();

        delay(20); // Überprüfung alle 20 ms
    }
}