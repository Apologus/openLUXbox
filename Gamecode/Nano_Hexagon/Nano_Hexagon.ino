/*****************************************************
 * NANO HEXAGON
 *****************************************************/

#include <FastLED.h>
#include "olb_init_v1.h"

/********************** DEFINES **********************/

#define MATRIX_W 16
#define MATRIX_H 16
#define NUM_LEDS 256
#define DATA_PIN D4
#define BRIGHTNESS 51

#define COLOR_PLAYER CRGB::Green
#define COLOR_BAR    CRGB::Red
#define COLOR_SCORE  CRGB::Blue

#define BTN_LEFT  D2
#define BTN_RIGHT D5
#define BTN_DOWN  D1
#define BTN_UP    D6

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

#define SCORE_DELAY 100

/*********************** STRUCTS *********************/

struct Button{
  int pin;
  bool stable,lastRaw;
  unsigned long lastChange;
};

struct Bar{
  int dir;
  int pos,offset,width;
  bool active;
  unsigned long startTime;
};

struct Particle{
  float x,y,vx,vy;
};

/******************** PROTOTYPEN *********************/

int XY(int,int);
bool readButton(Button&);

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
Bar bars[4];
Particle parts[10];

int playerX=7,playerY=7;

unsigned long gameStart,lastMove,lastSpeedUp,explodeStart;
float speed;

int partCount;
int collapseStep;
unsigned long lastCollapse;

enum State{PLAY,EXPLODE,SCORE,WAIT_INPUT,RESET};
State state;

/*********************** SETUP ***********************/

void setup(){

  FastLED.addLeds<WS2812B,DATA_PIN,GRB>(leds,NUM_LEDS);

  // Startanimation
  olb_v1_playIntro(leds);

  FastLED.setBrightness(BRIGHTNESS);


  pinMode(BTN_LEFT,INPUT_PULLUP);
  pinMode(BTN_RIGHT,INPUT_PULLUP);
  pinMode(BTN_DOWN,INPUT_PULLUP);
  pinMode(BTN_UP,INPUT_PULLUP);

  btns[0]={BTN_LEFT,HIGH,HIGH,0};
  btns[1]={BTN_RIGHT,HIGH,HIGH,0};
  btns[2]={BTN_DOWN,HIGH,HIGH,0};
  btns[3]={BTN_UP,HIGH,HIGH,0};

  randomSeed(ESP.getCycleCount());
  startGame();
}

/************************ LOOP ***********************/

void loop(){

  if(state==PLAY) playLoop();
  if(state==EXPLODE) explodeLoop();
  if(state==SCORE) scoreLoop();
  if(state==WAIT_INPUT) waitLoop();
  if(state==RESET) resetLoop();
}

/******************** GAME START *********************/

void startGame(){

  playerX=7;
  playerY=7;

  speed=START_SPEED;
  gameStart=millis();
  lastSpeedUp=millis();
  lastMove=millis();

  for(int i=0;i<4;i++){
    bars[i].dir=i;
    bars[i].active=(i==0);
    bars[i].pos=0;
    bars[i].width=random(BAR_MIN,BAR_MAX+1);
    bars[i].offset=random(0,16-bars[i].width);
    bars[i].startTime=gameStart + (i*BAR_DELAY);
  }

  state=PLAY;
}

/*********************** PLAY ************************/

void playLoop(){

  if(millis()-lastSpeedUp>SPEED_INTERVAL){
    speed+=SPEED_STEP;
    lastSpeedUp=millis();
  }

  handleButtons();
  updateBars();

  if(checkCollision()){
    startExplosion();
    return;
  }

  drawGame();
}

/********************* BUTTONS ***********************/

bool readButton(Button &b){

  bool raw=digitalRead(b.pin);

  if(raw!=b.lastRaw){
    b.lastChange=millis();
    b.lastRaw=raw;
  }

  if(millis()-b.lastChange>DEBOUNCE_TIME){
    if(raw!=b.stable){
      b.stable=raw;
      if(b.stable==LOW) return true;
    }
  }
  return false;
}

void handleButtons(){

  if(readButton(btns[0]) && playerX>0)  playerX--;
  if(readButton(btns[1]) && playerX<15) playerX++;
  if(readButton(btns[2]) && playerY<15) playerY++;
  if(readButton(btns[3]) && playerY>0)  playerY--;
}

/*********************** BARS ************************/

void updateBars(){

  unsigned long interval=1000/speed;
  if(millis()-lastMove<interval) return;
  lastMove=millis();

  for(int i=0;i<4;i++){

    if(!bars[i].active){
      if(millis()>bars[i].startTime)
        bars[i].active=true;
      else
        continue;
    }

    bars[i].pos++;

    if(bars[i].pos>15){

      bars[i].pos=0;
      bars[i].width=random(BAR_MIN,BAR_MAX+1);
      bars[i].offset=random(0,16-bars[i].width);
    }
  }
}

/*********************** RENDER **********************/

int XY(int x,int y){
  if(y%2==0) return y*16+x;
  return y*16+(15-x);
}

void drawGame(){

  FastLED.clear();

  for(int i=0;i<4;i++){

    if(!bars[i].active) continue;

    for(int w=0;w<bars[i].width;w++){

      int x,y;

      if(bars[i].dir==0){x=bars[i].offset+w; y=bars[i].pos;}
      if(bars[i].dir==1){x=bars[i].offset+w; y=15-bars[i].pos;}
      if(bars[i].dir==2){x=bars[i].pos; y=bars[i].offset+w;}
      if(bars[i].dir==3){x=15-bars[i].pos; y=bars[i].offset+w;}

      leds[XY(x,y)]=COLOR_BAR;
    }
  }

  leds[XY(playerX,playerY)]=COLOR_PLAYER;
  FastLED.show();
}

/********************* COLLISION *********************/

bool checkCollision(){

  for(int i=0;i<4;i++){

    if(!bars[i].active) continue;

    for(int w=0;w<bars[i].width;w++){

      int bx,by;

      if(bars[i].dir==0){bx=bars[i].offset+w; by=bars[i].pos;}
      if(bars[i].dir==1){bx=bars[i].offset+w; by=15-bars[i].pos;}
      if(bars[i].dir==2){bx=bars[i].pos; by=bars[i].offset+w;}
      if(bars[i].dir==3){bx=15-bars[i].pos; by=bars[i].offset+w;}

      if(bx==playerX && by==playerY)
        return true;
    }
  }
  return false;
}

/********************* EXPLOSION *********************/

void startExplosion(){

  partCount=random(6,8);

  for(int i=0;i<partCount;i++){
    parts[i].x=playerX;
    parts[i].y=playerY;
    parts[i].vx=random(-10,10)/10.0;
    parts[i].vy=random(-10,10)/10.0;
  }

  explodeStart=millis();
  state=EXPLODE;
}

void explodeLoop(){

  FastLED.clear();

  float fade=(millis()-explodeStart)*255.0/EXPLOSION_TIME;

  for(int i=0;i<partCount;i++){

    parts[i].x+=parts[i].vx;
    parts[i].y+=parts[i].vy;

    int x=parts[i].x;
    int y=parts[i].y;

    if(x<0||x>15||y<0||y>15) continue;

    CRGB c=COLOR_PLAYER;
    c.fadeToBlackBy(fade);
    leds[XY(x,y)]=c;
  }

  FastLED.show();

  if(millis()-explodeStart>EXPLOSION_TIME)
    state=SCORE;
}

/*********************** SCORE ***********************/

void scoreLoop(){

  FastLED.clear();

  int score=(millis()-gameStart)/1000;
  int filled=0;
  CRGB col=COLOR_SCORE;

  while(filled<score){

    for(int y=0;y<16;y++)
      for(int x=15;x>=0;x--){

        if(filled>=score) break;

        leds[XY(x,y)]=col;
        FastLED.show();
        delay(SCORE_DELAY);
        filled++;

        if(filled%256==0)
          col=CHSV(random(255),255,255);
      }
  }

  state=WAIT_INPUT;
}

/******************* WAIT FOR INPUT ******************/

void waitLoop(){

  for(int i=0;i<4;i++)
    if(readButton(btns[i])){
      collapseStep=0;
      lastCollapse=millis();
      state=RESET;
    }
}

/*********************** RESET ***********************/

void resetLoop(){

  if(millis()-lastCollapse < (COLLAPSE_TIME/COLLAPSE_STEPS))
    return;

  lastCollapse=millis();
  FastLED.clear();

  int s=collapseStep;

  for(int y=s;y<16-s;y++)
    for(int x=s;x<16-s;x++)
      leds[XY(x,y)]=COLOR_SCORE;

  FastLED.show();
  collapseStep++;

  if(collapseStep>=COLLAPSE_STEPS){

    FastLED.clear();
    leds[XY(7,7)]=COLOR_PLAYER;
    FastLED.show();
    delay(300);

    startGame();
  }
}
