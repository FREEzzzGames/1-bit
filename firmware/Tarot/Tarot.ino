#include <M5Cardputer.h>
#include <math.h>

static const int W=240,H=135,N=22;
static const uint16_t BG=0x0841,TAROT_GOLD=0xD6B3,TAROT_IVORY=0xEF7D,TAROT_INK=0x18C3,TAROT_RED=0xA145,TAROT_BLUE=0x43BF,TAROT_GREEN=0x4E68;
M5Canvas frame(&M5Cardputer.Display);
const char *names[N]={"THE FOOL","THE MAGICIAN","HIGH PRIESTESS","THE EMPRESS","THE EMPEROR","THE HIEROPHANT","THE LOVERS","THE CHARIOT","STRENGTH","THE HERMIT","WHEEL OF FORTUNE","JUSTICE","THE HANGED MAN","DEATH","TEMPERANCE","THE DEVIL","THE TOWER","THE STAR","THE MOON","THE SUN","JUDGEMENT","THE WORLD"};
const char *meaning[N]={"Beginnings / trust","Will / creation","Intuition / mystery","Growth / abundance","Order / authority","Tradition / faith","Choice / union","Drive / victory","Courage / patience","Wisdom / solitude","Cycles / change","Truth / balance","Pause / insight","Ending / renewal","Harmony / healing","Shadow / desire","Upheaval / release","Hope / guidance","Dreams / instinct","Joy / clarity","Awakening / calling","Completion / unity"};
enum Mode{DECK,CARD,SPREAD}; Mode mode=DECK;
float angle=0,velocity=0; int selected=0,revealed=0,spread[3]={0,1,2};
unsigned long lastFrame=0,lastImu=0; bool wasEnter=false,wasS=false;
bool pressed(bool now,bool &old){bool hit=now&&!old;old=now;return hit;}
int wrap(int v){while(v<0)v+=N;return v%N;}

void sun(int x,int y,int r,uint16_t c){frame.drawCircle(x,y,r,c);for(int i=0;i<8;i++){float a=i*0.785398f;frame.drawLine(x+cosf(a)*(r+2),y+sinf(a)*(r+2),x+cosf(a)*(r+8),y+sinf(a)*(r+8),c);}}
void star(int x,int y,int r,uint16_t c){for(int i=0;i<5;i++){float a=-1.5708f+i*1.25664f,b=a+2.51327f;frame.drawLine(x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r,y+sinf(b)*r,c);}}
void art(int x,int y,int w,int h,int id){
 int cx=x+w/2,cy=y+h/2; uint16_t a=(id%3==0?TAROT_RED:(id%3==1?TAROT_BLUE:TAROT_GREEN));
 frame.drawRect(x,y,w,h,TAROT_GOLD);frame.drawRect(x+3,y+3,w-6,h-6,TAROT_IVORY);
 switch(id){
 case 0: sun(x+w-13,y+14,5,TAROT_GOLD);frame.drawLine(x+5,y+h-12,x+w-5,y+h-12,TAROT_GOLD);frame.fillCircle(cx,cy-8,4,TAROT_IVORY);frame.drawLine(cx,cy-4,cx+3,cy+13,a);frame.drawLine(cx+2,cy+2,cx+13,cy-6,TAROT_GOLD);frame.drawLine(cx+3,cy+13,cx-5,cy+22,TAROT_GOLD);frame.drawLine(cx+3,cy+13,cx+12,cy+20,TAROT_GOLD);break;
 case 1: frame.drawCircle(cx,y+17,7,TAROT_GOLD);frame.drawLine(cx,y+24,cx,y+h-10,a);frame.drawLine(cx-14,y+30,cx+14,y+30,TAROT_GOLD);frame.drawCircle(cx,y+h-12,5,TAROT_GOLD);star(cx,y+17,5,TAROT_IVORY);break;
 case 2: frame.drawRect(cx-15,y+12,7,h-24,TAROT_IVORY);frame.drawRect(cx+8,y+12,7,h-24,TAROT_IVORY);frame.fillCircle(cx,y+23,8,TAROT_GOLD);frame.fillCircle(cx+4,y+20,8,TAROT_INK);frame.drawLine(cx-10,y+h-16,cx+10,y+h-16,a);break;
 case 3: sun(cx,y+19,9,TAROT_GOLD);frame.drawLine(cx-18,y+h-10,cx-10,y+31,a);frame.drawLine(cx+18,y+h-10,cx+10,y+31,a);for(int i=0;i<3;i++)frame.fillCircle(cx-10+i*10,y+h-20,3,TAROT_GREEN);break;
 case 4: frame.drawRect(cx-13,y+24,26,h-36,a);frame.drawLine(cx-16,y+24,cx,y+9,TAROT_GOLD);frame.drawLine(cx,y+9,cx+16,y+24,TAROT_GOLD);frame.drawLine(cx-18,y+h-11,cx+18,y+h-11,TAROT_GOLD);break;
 case 5: frame.drawCircle(cx,y+18,7,TAROT_GOLD);frame.drawLine(cx,y+25,cx,y+h-12,a);frame.drawLine(cx-15,y+31,cx+15,y+31,TAROT_GOLD);frame.drawLine(cx-10,y+39,cx-17,y+52,TAROT_GOLD);frame.drawLine(cx+10,y+39,cx+17,y+52,TAROT_GOLD);break;
 case 6: sun(cx,y+15,6,TAROT_GOLD);frame.drawLine(cx,y+24,cx-15,y+48,a);frame.drawLine(cx,y+24,cx+15,y+48,TAROT_GREEN);frame.drawLine(cx-15,y+48,cx+15,y+48,TAROT_GOLD);break;
 case 7: frame.drawRect(cx-16,y+24,32,22,a);frame.drawCircle(cx-12,y+50,6,TAROT_GOLD);frame.drawCircle(cx+12,y+50,6,TAROT_GOLD);frame.drawLine(cx,y+8,cx,y+24,TAROT_GOLD);break;
 case 8: frame.drawCircle(cx,y+15,7,TAROT_GOLD);frame.drawLine(cx,y+22,cx,y+43,a);frame.drawLine(cx,y+30,cx-14,y+42,TAROT_GOLD);frame.drawLine(cx,y+30,cx+14,y+42,TAROT_GOLD);frame.drawCircle(cx,y+50,9,TAROT_IVORY);break;
 case 9: frame.drawLine(cx,y+10,cx,y+h-10,TAROT_GOLD);frame.drawCircle(cx,y+18,10,TAROT_GOLD);frame.drawCircle(cx,y+18,5,TAROT_IVORY);frame.drawLine(cx-12,y+33,cx+12,y+33,a);break;
 case 10: frame.drawCircle(cx,cy,22,TAROT_GOLD);frame.drawCircle(cx,cy,13,TAROT_GOLD);frame.drawCircle(cx,cy,5,a);for(int i=0;i<8;i++){float q=i*.785398f;frame.drawLine(cx,cy,cx+cosf(q)*21,cy+sinf(q)*21,TAROT_GOLD);}break;
 case 11: frame.drawLine(cx,y+10,cx,y+h-8,TAROT_GOLD);frame.drawLine(cx-18,y+24,cx+18,y+24,TAROT_GOLD);frame.drawLine(cx-13,y+24,cx-17,y+43,TAROT_GOLD);frame.drawLine(cx+13,y+24,cx+17,y+43,TAROT_GOLD);frame.drawLine(cx-18,y+43,cx-8,y+43,TAROT_GOLD);frame.drawLine(cx+18,y+43,cx+8,y+43,TAROT_GOLD);break;
 case 12: frame.drawLine(cx-18,y+14,cx+18,y+14,TAROT_GOLD);frame.drawLine(cx,y+14,cx,y+29,TAROT_GOLD);frame.drawCircle(cx,y+35,6,TAROT_IVORY);frame.drawLine(cx,y+41,cx,y+56,a);frame.drawLine(cx,y+46,cx-12,y+54,TAROT_GOLD);frame.drawLine(cx,y+46,cx+12,y+54,TAROT_GOLD);break;
 case 13: frame.drawLine(cx-14,y+10,cx+14,y+h-10,TAROT_GOLD);frame.drawLine(cx+14,y+10,cx-14,y+h-10,TAROT_GOLD);frame.drawCircle(cx,cy,13,TAROT_IVORY);frame.drawCircle(cx,cy,5,a);break;
 case 14: frame.drawCircle(cx,y+16,6,TAROT_GOLD);frame.drawLine(cx,y+22,cx,y+52,TAROT_IVORY);frame.drawLine(cx-15,y+31,cx+15,y+43,TAROT_GOLD);frame.drawLine(cx+15,y+31,cx-15,y+43,a);break;
 case 15: frame.drawCircle(cx,y+24,11,a);frame.drawLine(cx-8,y+16,cx-16,y+6,TAROT_GOLD);frame.drawLine(cx+8,y+16,cx+16,y+6,TAROT_GOLD);frame.drawLine(cx,y+35,cx,y+57,TAROT_GOLD);frame.drawCircle(cx-14,y+54,4,TAROT_IVORY);frame.drawCircle(cx+14,y+54,4,TAROT_IVORY);break;
 case 16: frame.drawRect(cx-12,y+20,24,37,a);frame.drawLine(cx-16,y+20,cx,y+8,TAROT_GOLD);frame.drawLine(cx,y+8,cx+16,y+20,TAROT_GOLD);frame.drawLine(cx-18,y+5,cx-3,y+29,TAROT_IVORY);frame.drawLine(cx+18,y+5,cx+3,y+29,TAROT_IVORY);break;
 case 17: star(cx,y+31,20,TAROT_GOLD);star(cx,y+31,8,TAROT_IVORY);frame.drawCircle(cx-17,y+12,2,TAROT_GOLD);frame.drawCircle(cx+18,y+14,2,TAROT_GOLD);break;
 case 18: frame.fillCircle(cx,y+25,17,TAROT_GOLD);frame.fillCircle(cx+8,y+18,17,TAROT_INK);frame.drawCircle(cx-14,y+53,3,a);frame.drawCircle(cx+14,y+53,3,a);break;
 case 19: sun(cx,y+30,18,TAROT_GOLD);frame.fillCircle(cx,y+30,8,TAROT_IVORY);break;
 case 20: sun(cx,y+17,10,TAROT_GOLD);frame.drawLine(cx,y+27,cx,y+43,TAROT_IVORY);frame.drawLine(cx-16,y+36,cx+16,y+36,TAROT_GOLD);frame.drawLine(cx-16,y+36,cx-10,y+55,a);frame.drawLine(cx+16,y+36,cx+10,y+55,a);break;
 case 21: frame.drawEllipse(cx,cy,19,26,TAROT_GOLD);frame.drawCircle(cx,cy,12,TAROT_IVORY);frame.drawLine(cx-13,cy,cx+13,cy,a);frame.drawLine(cx,cy-17,cx,cy+17,a);star(cx,cy,6,TAROT_GOLD);break;
 }
}
void back(int x,int y,int w,int h,bool hi){frame.fillRoundRect(x,y,w,h,4,TAROT_INK);frame.drawRoundRect(x,y,w,h,4,hi?TAROT_GOLD:TAROT_IVORY);frame.drawRect(x+4,y+4,w-8,h-8,TAROT_GOLD);frame.drawLine(x+6,y+6,x+w-6,y+h-6,TAROT_GOLD);frame.drawLine(x+w-6,y+6,x+6,y+h-6,TAROT_GOLD);frame.drawCircle(x+w/2,y+h/2,5,TAROT_GOLD);}
void mini(int x,int y,int w,int h,int id){frame.fillRoundRect(x,y,w,h,3,TAROT_INK);frame.drawRoundRect(x,y,w,h,3,TAROT_GOLD);art(x+4,y+4,w-8,h-18,id);frame.setTextSize(1);frame.setTextColor(TAROT_GOLD,TAROT_INK);frame.setCursor(x+4,y+h-11);frame.printf("%02d",id);}
void drawDeck(){
 frame.fillScreen(BG);frame.setTextSize(1);frame.setTextColor(TAROT_GOLD,BG);frame.setCursor(7,5);frame.print("TAROT / MAJOR ARCANA");
 frame.setTextColor(TAROT_IVORY,BG);frame.setCursor(7,17);frame.print("TILT TO TURN  |  ENTER TO DRAW");
 for(int i=-3;i<=3;i++)back(108+i*17,69+abs(i)*3,27,43,i==0);
 frame.setTextColor(TAROT_GOLD,BG);frame.setCursor(7,119);frame.printf("CARD %02d / 21",selected);
 frame.setTextColor(TAROT_IVORY,BG);frame.setCursor(7,129);frame.print("S: THREE-CARD SPREAD");frame.pushSprite(0,0);
}
void drawCard(){
 frame.fillScreen(BG);frame.setTextSize(1);frame.setTextColor(TAROT_GOLD,BG);frame.setCursor(7,5);frame.print("MAJOR ARCANA");
 mini(8,20,83,105,revealed);
 frame.setTextSize(1);frame.setCursor(101,25);frame.setTextColor(TAROT_GOLD,BG);frame.printf("%02d / 21",revealed);
 frame.setTextSize(2);frame.setCursor(101,43);frame.print(names[revealed]);
 frame.drawFastHLine(101,64,130,TAROT_GOLD);frame.setTextSize(1);frame.setTextColor(TAROT_IVORY,BG);frame.setCursor(101,76);frame.print("MEANING");
 frame.setTextColor(TFT_WHITE,BG);frame.setCursor(101,91);frame.print(meaning[revealed]);
 frame.setTextColor(TAROT_GOLD,BG);frame.setCursor(101,117);frame.print("ENTER: DECK");frame.pushSprite(0,0);
}
void makeSpread(){int a=random(N),b=random(N-1);if(b>=a)b++;int c=random(N-2);while(c==a||c==b)c=random(N);spread[0]=a;spread[1]=b;spread[2]=c;}
void drawSpread(){
 frame.fillScreen(BG);frame.setTextSize(1);frame.setTextColor(TAROT_GOLD,BG);frame.setCursor(7,5);frame.print("THREE-CARD READING");
 const char *p[3]={"PAST","PRESENT","FUTURE"};
 for(int i=0;i<3;i++){int x=5+i*79;frame.setTextColor(TAROT_IVORY,BG);frame.setCursor(x+8,18);frame.print(p[i]);mini(x+3,31,70,79,spread[i]);}
 frame.setTextColor(TAROT_GOLD,BG);frame.setCursor(7,128);frame.print("ENTER: RETURN");frame.pushSprite(0,0);
}
void setup(){
 auto cfg=M5.config();M5Cardputer.begin(cfg,true);M5Cardputer.Display.setRotation(0);M5.Imu.begin();
 frame.setColorDepth(16);frame.createSprite(W,H);randomSeed((uint32_t)esp_random());frame.fillScreen(BG);frame.pushSprite(0,0);
}
void loop(){
 M5Cardputer.update();unsigned long now=millis();
 if(mode==DECK && now-lastImu>=20){float ax=0,ay=0,az=0;M5.Imu.getAccel(&ax,&ay,&az);if(fabsf(ay)>.12f)velocity+=ay*.012f;velocity*=.94f;if(velocity>.12f)velocity=.12f;if(velocity<-.12f)velocity=-.12f;angle+=velocity;selected=wrap((int)floorf(angle*2.f+.5f));lastImu=now;}
 auto keys=M5Cardputer.Keyboard.keysState();bool enter=pressed(keys.enter||keys.space,wasEnter);bool s=pressed(M5Cardputer.Keyboard.isKeyPressed('s'),wasS);
 if(mode==DECK){if(enter){revealed=selected;mode=CARD;}else if(s){makeSpread();mode=SPREAD;}}
 else if((mode==CARD||mode==SPREAD)&&enter)mode=DECK;
 if(now-lastFrame>=33){if(mode==DECK)drawDeck();else if(mode==CARD)drawCard();else drawSpread();lastFrame=now;}
}
