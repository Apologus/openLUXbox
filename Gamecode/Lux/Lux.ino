#include <Arduino.h>
#include <FastLED.h>
#include <LittleFS.h>
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

// Geschwindigkeit in LEDs pro Sekunde
const float SPEED_INCREMENT = 0.25;
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



std::vector<String> bmpFiles;  

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
        //Serial.println();
        for (int x = 0; x < 16; x++) {
            int index = getIndexFromMatrix(x, y);
            if (ledMatrix[x][y] == 0) {
                leds[index] = CRGB::Black;
            } else if (ledMatrix[x][y] == 1) {
                leds[index] = CRGB::Green;
            }  else if (ledMatrix[x][y] == 2) {
                leds[index] = CRGB::Yellow;
            }  else if (ledMatrix[x][y] == 3) {
                leds[index] = CRGB::Red;
            }  else if (ledMatrix[x][y] == 4) {
                leds[index] = CRGB::Orange;
            }  else if (ledMatrix[x][y] == 5) {
                leds[index] = CRGB::Coral;
            }  else if (ledMatrix[x][y] == 6) {
                leds[index] = CRGB::White;
            }  else if (ledMatrix[x][y] == 7) {
                leds[index] = CRGB::Blue;
            }
            //leds[index] = CRGB(ledMatrix[x][y] ? CRGB::Yellow : CRGB::Black);  // Gelb für 1, Schwarz für 0
        }
    }
    
    FastLED.show();
}

void explodeAt(int x, int y, int colors[], int groesse) {
  
  for (int r = 0; r < 5; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        int newX = x + dx;
        int newY = y + dy;
        if (newX >= 0 && newX < 16 && newY >= 0 && newY < 16) {
          int distance = abs(dx) + abs(dy);
          if (distance <= r) {
            ledMatrix[newX][newY] = colors[min(distance, 2)]; // 
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

void loadBMP(String filename) {
    Serial.println("Displaying: " + filename);

    File bmpFile = LittleFS.open(filename, "r");
    if (!bmpFile) {
        Serial.println("Error opening BMP file.");
        return;
    }

    // BMP header parsing
    uint8_t header[54];
    bmpFile.read(header, 54);
    if (header[0] != 'B' || header[1] != 'M') {
        Serial.println("Invalid BMP file.");
        bmpFile.close();
        return;
    }

    uint32_t pixelArrayOffset = *(uint32_t*)&header[10];
    int width = *(int32_t*)&header[18];
    int height = *(int32_t*)&header[22];

    if (width != 16 || abs(height) != 16) {
        Serial.println("BMP dimensions do not match matrix.");
        bmpFile.close();
        return;
    }

    bmpFile.seek(pixelArrayOffset);

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            uint8_t b = bmpFile.read();
            uint8_t g = bmpFile.read();
            uint8_t r = bmpFile.read();
            if (b == 0 && g == 0 && r == 255) {
                ledMatrix[15-x][15-y] = 3;
            } else if (b == 0 && g == 255 && r == 0) { // Spielpunkt
                ledMatrix[15-x][15-y] = 1;
                posX = 15-x;
                posY = 15-y;
            } else if (b == 0 && g == 255 && r == 255) { // Snatch
                ledMatrix[15-x][15-y] = 2;
                targetX = 15-x;
                targetY = 15-y;
            } else if (b == 255 && g == 255 && r == 255) { // weiß
                ledMatrix[15-x][15-y] = 6;
            } else if (b == 255 && g == 0 && r == 0) { // blau
                ledMatrix[15-x][15-y] = 7;
            } else {
                ledMatrix[15-x][15-y] = 0;  //alle anderen schwarz
            }
        }
    }
    bmpFile.close();
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

    //Crash?
    if (ledMatrix[currentX][currentY] == 3) {
        //rote Explosion zeichnen
        int colors [3] = {2, 4, 3};
        explodeAt(currentX, currentY, colors, 3);
        lives -= 1;
        if (lives == 0) {
            score = 0;
            loadBMP("game_over.bmp");
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
        int colors [3] = {1, 5, 1};
        explodeAt(currentX, currentY, colors, 3);
        speedX = 0;
        speedY = 0;
    }

    // Timer am unteren Rand
    int timer = int((millis()-startTime) / timerSpeed);
    for (int i = 0; i < timer; i++) {
        ledMatrix[15-i][15] = 3;
    }
    if (timer == 16) {
        //rote Explosion zeichnen
        int colors [3] = {2, 4, 3};
        explodeAt(currentX, currentY, colors, 3);
        lives -= 1;
        if (lives == 0) {
            score = 0;
            loadBMP("game_over.bmp");            
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

void loadRandomBMP() {
    // List BMP files in LittleFS
    Serial.println("Listing BMP files:");
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
        String fileName = dir.fileName();
        //if (fileName.endsWith(".bmp")) {
        bmpFiles.push_back(fileName);
        Serial.println("Found: " + fileName);
        //}
    }

    if (bmpFiles.empty()) {
        Serial.println("No BMP files found in LittleFS.");
        return;
    }

    randomSeed(analogRead(A0));
    String filename = "1.bmp";
    
    // Display the first image
    if (!bmpFiles.empty()) {
        String selectedFile = bmpFiles[random(bmpFiles.size())];
        Serial.println("Displaying: " + selectedFile);
        filename = selectedFile;
    }

    File bmpFile = LittleFS.open(filename, "r");
    if (!bmpFile) {
        Serial.println("Error opening BMP file.");
        return;
    }

    // BMP header parsing
    uint8_t header[54];
    bmpFile.read(header, 54);
    if (header[0] != 'B' || header[1] != 'M') {
        Serial.println("Invalid BMP file.");
        bmpFile.close();
        return;
    }

    uint32_t pixelArrayOffset = *(uint32_t*)&header[10];
    int width = *(int32_t*)&header[18];
    int height = *(int32_t*)&header[22];

    if (width != 16 || abs(height) != 16) {
        Serial.println("BMP dimensions do not match matrix.");
        bmpFile.close();
        return;
    }

    bmpFile.seek(pixelArrayOffset);

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            uint8_t b = bmpFile.read();
            uint8_t g = bmpFile.read();
            uint8_t r = bmpFile.read();
            if (b == 0 && g == 0 && r == 255) {
                ledMatrix[15-x][15-y] = 3;
            } else if (b == 0 && g == 255 && r == 0) { // Spielpunkt
                ledMatrix[15-x][15-y] = 1;
                posX = 15-x;
                posY = 15-y;
            } else if (b == 0 && g == 255 && r == 255) { // Snatch
                ledMatrix[15-x][15-y] = 2;
                targetX = 15-x;
                targetY = 15-y;
            }
        /*int mirroredX = WIDTH - 1 - x; // Mirror the x-coordinate
        int index = height > 0 ? getIndex(mirroredX, HEIGHT - 1 - y) : getIndex(mirroredX, y);
        matrix.setPixelColor(index, matrix.Color(r, g, b));*/
        }
    }

    bmpFile.close();
}


// Zeigt die Anschlüsse
void setupScreen() {
    loadBMP("setup.bmp");
    displayMatrix();
    FastLED.show();
    waitForAnyButton();
    FastLED.clear();
}

// zeigt vor jedem Level die Level-Nummer
void showLevel() {
    loadBMP("level.bmp");

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
    loadBMP("lives.bmp");


    for (int i = 0; i < lives; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                ledMatrix[4 + i * 3 + k][10 + j] = 1;
                //Serial.println(5 + i * 2 + k);
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

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Failed to initialize LittleFS.");
        return;
    }

    // Button-Pins als Eingang konfigurieren
    pinMode(BUTTON_1, INPUT_PULLUP);
    pinMode(BUTTON_2, INPUT_PULLUP);
    pinMode(BUTTON_3, INPUT_PULLUP);
    pinMode(BUTTON_4, INPUT_PULLUP);
    pinMode(BUTTON_5, INPUT_PULLUP);

    // Initialisierung des LED-Matrix-Arrays
    memset(ledMatrix, 0, sizeof(ledMatrix));
 
    setupScreen();

    waitForAllButtons();
    //loadRandomBMP();
    // Matrix anzeigen

}

void loop() {
    //Game over?
    if (lives == 0) {
        score = 0;
        lives = 3; 
    }

    // Level anzeigen
    showLevel();
    showLives();

    String filename = String(score + 1) + ".bmp";
    loadBMP(filename);
    displayMatrix();
    delay(500);

    int oldScore = score;
    int oldLives = lives;
    startTime = millis();

    while (oldScore == score && oldLives == lives) {
        // Überprüfen, ob ein Button gedrückt ist und Geschwindigkeit anpassen
        if (digitalRead(BUTTON_1) == LOW) {
            speedX -= SPEED_INCREMENT;
        }
        if (digitalRead(BUTTON_2) == LOW) {
            speedX += SPEED_INCREMENT;
        }
        if (digitalRead(BUTTON_3) == LOW) {
            speedY += SPEED_INCREMENT;
        }
        if (digitalRead(BUTTON_4) == LOW) {
            speedY -= SPEED_INCREMENT;
        }
        
        // Position aktualisieren und Matrix neu zeichnen
        updatePosition();

        delay(20); // Überprüfung alle 20 ms
    }
}