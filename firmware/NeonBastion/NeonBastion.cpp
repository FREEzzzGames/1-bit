#include <M5Cardputer.h>
#include <Preferences.h>
#include <math.h>

// NEON BASTION — FREEzzz Arcade / Cardputer ADV
// 240x135, landscape rotation 0. No dynamic allocation in the game loop.
static constexpr int W=240,H=135, CW=12, CH=9, CELL=13, OX=42, OY=16;
static constexpr uint16_t BLACK=0x0000, GREEN=0x07E0, DIM=0x0320, DARK=0x0100, WHITE=0xFFFF, AMBER=0xFD20, RED=0xF800, CYAN=0x07FF;
M5Canvas canvas(&M5Cardputer.Display);
Preferences prefs;

struct Tower { int8_t x,y,type; uint8_t level; bool live; uint32_t shot; };
struct Enemy { float x,y,hp,maxhp,speed; uint8_t type; bool live; };
struct Shot { float x,y,vx,vy; uint8_t damage; bool live; };
static Tower towers[18];
static Enemy enemies[24];
static Shot shots[32];
static const int8_t pathX[]={0,1,2,3,4,4,4,5,6,7,8,8,8,9,10,11};
static const int8_t pathY[]={3,3,3,3,3,4,5,5,5,5,5,4,3,3,3,3};
static int cursorX=2,cursorY=1, selected=0, money=90, lives=15, wave=0, spawned=0, kills=0;
static int waveTotal=0; static bool waveOn=false, gameOver=false, victory=false;
static uint32_t lastTick=0,lastSpawn=0,lastDraw=0,flashUntil=0;
static uint16_t accent=GREEN;
static const char* towerNames[]={"PULSE","LASER","FROST","BURST"};
static const int costs[]={35,55,45,65};
static const uint16_t towerCol[]={GREEN,CYAN,AMBER,RED};
static bool onPath(int x,int y){for(int i=0;i<16;i++)if(pathX[i]==x&&pathY[i]==y)return true;return false;}
static bool hasTower(int x,int y){for(auto &t:towers)if(t.live&&t.x==x&&t.y==y)return true;return false;}
static float dist2(float x,float y,float a,float b){float dx=x-a,dy=y-b;return dx*dx+dy*dy;}
static void text(int x,int y,const char*s,uint16_t c=GREEN){canvas.setTextColor(c);canvas.setCursor(x,y);canvas.print(s);}
static void center(const char*s,int y,uint16_t c=GREEN){int x=(W-(int)strlen(s)*6)/2;text(x,y,s,c);}
static void spawnEnemy(){
  for(auto &e:enemies)if(!e.live){e.live=true;e.x=pathX[0];e.y=pathY[0];e.type=(wave%5==0&&spawned==0)?2:(spawned%6==5?1:0);
    e.maxhp=e.hp=(e.type==2?16: e.type==1?7:3)+wave*1.4f;e.speed=e.type==1?0.27f:e.type==2?0.20f:0.42f;return;}
}
static void addTower(){
  if(onPath(cursorX,cursorY)||hasTower(cursorX,cursorY))return;
  if(money<costs[selected]){flashUntil=millis()+500;return;}
  for(auto &t:towers)if(!t.live){t={ (int8_t)cursorX,(int8_t)cursorY,(int8_t)selected,1,true,0};money-=costs[selected];return;}
}
static void sellTower(){for(auto &t:towers)if(t.live&&t.x==cursorX&&t.y==cursorY){money+=costs[t.type]*t.level/2;t.live=false;return;}}
static void startWave(){if(waveOn||gameOver)return;wave++;spawned=0;waveTotal=6+wave*2;waveOn=true;lastSpawn=0;}
static void updateGame(uint32_t now){
  float dt=(now-lastTick)/1000.f;if(dt>0.05f)dt=0.05f;lastTick=now;
  if(!waveOn||gameOver)return;
  if(spawned<waveTotal&&now-lastSpawn>(uint32_t)(850-wave*18>250?850-wave*18:250)){spawnEnemy();spawned++;lastSpawn=now;}
  for(auto &e:enemies)if(e.live){
    int seg=(int)e.x; if(seg<0)seg=0;if(seg>14)seg=14;float p=e.x-seg;
    float tx=pathX[seg+1],ty=pathY[seg+1];float dx=tx-e.x,dy=ty-e.y;float d=sqrtf(dx*dx+dy*dy);
    if(d>0.01f){e.x+=dx/d*e.speed*dt*8;e.y+=dy/d*e.speed*dt*8;}
    if(e.x>=10.8f&&e.y<=3.2f){e.live=false;lives--;if(lives<=0)gameOver=true;}
  }
  for(auto &t:towers)if(t.live){
    float range=t.type==1?3.6f:2.6f;int target=-1;float best=range*range;
    for(int i=0;i<24;i++)if(enemies[i].live){float d=dist2(t.x,t.y,enemies[i].x,enemies[i].y);if(d<best){best=d;target=i;}}
    if(target>=0&&now-t.shot>(uint32_t)(t.type==0?420:t.type==1?850:t.type==2?650:1050)){
      Enemy &e=enemies[target];e.hp-=t.type==1?4:t.type==3?5:2;if(t.type==2)e.speed*=0.72f;
      t.shot=now;
      if(e.hp<=0){e.live=false;money+=8;kills++;}
      if(t.type==3){for(auto &other:enemies)if(other.live&&dist2(e.x,e.y,other.x,other.y)<1.0f){other.hp-=2;if(other.hp<=0){other.live=false;money+=8;kills++;}}}
    }
  }
  bool any=false;for(auto &e:enemies)any|=e.live;
  if(spawned>=waveTotal&&!any){waveOn=false;money+=15;if(wave>=15)victory=true;}
}
static void drawMap(){
  canvas.fillSprite(BLACK);
  // CRT scan lines and subtle grid
  for(int y=0;y<H;y+=3)canvas.drawFastHLine(0,y,W,0x0841);
  for(int y=0;y<CH;y++)for(int x=0;x<CW;x++){
    int px=OX+x*CELL,py=OY+y*CELL;
    canvas.drawRect(px,py,CELL-1,CELL-1,0x0180);
  }
  for(int i=0;i<15;i++){int x=OX+pathX[i]*CELL+CELL/2,y=OY+pathY[i]*CELL+CELL/2;canvas.fillCircle(x,y,4,0x0200);canvas.drawCircle(x,y,4,DIM);}
  for(int i=0;i<16;i++){int x=OX+pathX[i]*CELL+CELL/2,y=OY+pathY[i]*CELL+CELL/2;canvas.fillCircle(x,y,2,GREEN);}
  for(auto &t:towers)if(t.live){int x=OX+t.x*CELL+6,y=OY+t.y*CELL+6;uint16_t c=towerCol[t.type];canvas.drawRect(x-4,y-4,9,9,c);canvas.drawPixel(x,y,c);canvas.drawFastHLine(x-2,y,5,c);}
  for(auto &e:enemies)if(e.live){int x=OX+(int)(e.x*CELL)+6,y=OY+(int)(e.y*CELL)+6;uint16_t c=e.type==2?RED:e.type==1?AMBER:WHITE;canvas.fillCircle(x,y,e.type==2?4:3,c);canvas.drawPixel(x-1,y-1,BLACK);canvas.drawPixel(x+1,y-1,BLACK);canvas.drawFastHLine(x-3,y-6,6,c);}
  for(auto &s:shots)if(s.live)canvas.drawPixel(OX+(int)(s.x*CELL)+6,OY+(int)(s.y*CELL)+6,CYAN);
  int cx=OX+cursorX*CELL+6,cy=OY+cursorY*CELL+6;canvas.drawRect(cx-6,cy-6,13,13,WHITE);
  // HUD
  canvas.fillRect(0,0,W,14,0x0841);text(3,3,"NEON BASTION",GREEN);text(112,3,"W:",WHITE);canvas.printf("%02d",wave);text(148,3,"$:",AMBER);canvas.printf("%03d",money);text(194,3,"CORE:",WHITE);canvas.printf("%02d",lives);
  canvas.fillRect(0, H-16, W,16,0x0841);
  canvas.setTextColor(towerCol[selected]);canvas.setCursor(3,H-12);canvas.printf("%s $%d",towerNames[selected],costs[selected]);
  canvas.setTextColor(WHITE);canvas.setCursor(112,H-12);canvas.print(waveOn?"DEFENDING":"SPACE: WAVE");
  if(millis()<flashUntil)text(178, H-12,"LOW $",RED);
  if(gameOver||victory){canvas.fillRect(25,43,190,48,BLACK);canvas.drawRect(25,43,190,48,victory?GREEN:RED);center(victory?"SECTOR CLEARED":"CORE DESTROYED",53,victory?GREEN:RED);center("ENTER: RESTART",70,WHITE);}
}
static void resetGame(){for(auto &t:towers)t.live=false;for(auto &e:enemies)e.live=false;money=90;lives=15;wave=0;kills=0;waveOn=false;gameOver=victory=false;cursorX=2;cursorY=1;}
static void handleInput(){
  if(!M5Cardputer.Keyboard.isChange())return;
  auto s=M5Cardputer.Keyboard.keysState();
  if(gameOver||victory){if(s.enter)resetGame();return;}
  for(char c:s.word){
    if(c=='w')cursorY=max(0,cursorY-1);if(c=='s')cursorY=min(CH-1,cursorY+1);
    if(c=='a')cursorX=max(0,cursorX-1);if(c=='d')cursorX=min(CW-1,cursorX+1);
    if(c=='q')selected=(selected+3)%4;if(c=='e')selected=(selected+1)%4;
    if(c=='1')selected=0;if(c=='2')selected=1;if(c=='3')selected=2;if(c=='4')selected=3;
    if(c==' ')startWave();if(c=='x')sellTower();
  }
  if(s.enter)addTower();
  if(s.del)sellTower();
}
void setup(){
  auto cfg=M5.config();M5Cardputer.begin(cfg,true);M5Cardputer.Display.setRotation(0);
  canvas.setColorDepth(16);canvas.createSprite(W,H);canvas.setTextSize(1);
  prefs.begin("nbastion",false);resetGame();lastTick=millis();
}
void loop(){
  M5Cardputer.update();handleInput();uint32_t now=millis();updateGame(now);
  if(now-lastDraw>=33){drawMap();canvas.pushSprite(0,0);lastDraw=now;}
  delay(2);
}
