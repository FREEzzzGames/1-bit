#include <M5Cardputer.h>
#include <Preferences.h>

M5Canvas canvas(&M5Cardputer.Display);
unsigned long lastRender = 0;
const unsigned long FRAME_INTERVAL_MS = 33;

const int SCREEN_W = 240;
const int SCREEN_H = 135;
const int ROAD_W = 130;
const int ROAD_X = (SCREEN_W - ROAD_W) / 2;
const int MAX_ENEMIES = 5;

struct Car {
  float x, y;
  int w, h;
  bool active;
  float speed;
};

enum GameState { MENU, PLAYING, PAUSED, GAME_OVER };

Car player;
Car enemies[MAX_ENEMIES];
GameState state = MENU;
Preferences prefs;

int score = 0;
int highScore = 0;
int lives = 3;
float roadScroll = 0;
float playerVX = 0;
float baseSpeed = 1.8f;
unsigned long lastSpawn = 0;
unsigned long invulnUntil = 0;
unsigned long crashFlashUntil = 0;

struct Keys { bool enter, space, p, r, left, right; };
Keys prevK = {};

void readKeys(Keys& k) {
  auto s = M5Cardputer.Keyboard.keysState();
  k.enter = s.enter;
  k.space = s.space;
  k.p = M5Cardputer.Keyboard.isKeyPressed('p');
  k.r = M5Cardputer.Keyboard.isKeyPressed('r');
  k.left = M5Cardputer.Keyboard.isKeyPressed(',') || M5Cardputer.Keyboard.isKeyPressed('a');
  k.right = M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('d');
}

bool pressedOnce(bool cur, bool prev) { return cur && !prev; }
void beep(int f, int ms) { M5Cardputer.Speaker.tone(f, ms); }

void loadHS() {
  prefs.begin("race", true);
  highScore = prefs.getInt("hs", 0);
  prefs.end();
}

void saveHS() {
  if (score > highScore) {
    highScore = score;
    prefs.begin("race", false);
    prefs.putInt("hs", highScore);
    prefs.end();
  }
}

void resetGame() {
  player.w = 22;
  player.h = 32;
  player.x = ROAD_X + ROAD_W / 2 - player.w / 2;
  player.y = SCREEN_H - player.h - 6;
  playerVX = 0;
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
  score = 0;
  lives = 3;
  baseSpeed = 1.8f;
  roadScroll = 0;
  lastSpawn = millis();
  invulnUntil = 0;
  crashFlashUntil = 0;
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(0);
  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  canvas.fillScreen(TFT_BLACK);
  M5Cardputer.Speaker.setVolume(110);
  randomSeed(millis());
  loadHS();
}

void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].w = 20;
      enemies[i].h = 28;
      enemies[i].x = random(ROAD_X + 4, ROAD_X + ROAD_W - enemies[i].w - 4);
      enemies[i].y = -enemies[i].h - 5;
      enemies[i].speed = baseSpeed + random(-1, 3) * 0.3f;
      if (enemies[i].speed < 1.2f) enemies[i].speed = 1.2f;
      enemies[i].active = true;
      return;
    }
  }
}

bool collide(Car& a, Car& b) {
  return (a.x + 2 < b.x + b.w - 2 && a.x + a.w - 2 > b.x + 2 &&
          a.y + 2 < b.y + b.h - 2 && a.y + a.h - 2 > b.y + 2);
}

void drawCar(Car& c, uint16_t bodyColor) {
  int x = (int)c.x, y = (int)c.y;
  canvas.fillRoundRect(x, y, c.w, c.h, 3, bodyColor);
  canvas.fillRect(x + 3, y + 5, c.w - 6, 6, TFT_NAVY);
  canvas.fillRect(x + 3, y + 13, c.w - 6, 8, TFT_DARKGREY);
  canvas.fillRect(x + 3, y + c.h - 11, c.w - 6, 6, TFT_NAVY);
  canvas.drawRoundRect(x, y, c.w, c.h, 3, TFT_BLACK);
}

void drawGame() {
  canvas.fillScreen(TFT_BLACK);

  canvas.fillRect(0, 0, ROAD_X, SCREEN_H, TFT_DARKGREEN);
  canvas.fillRect(ROAD_X + ROAD_W, 0, SCREEN_W - ROAD_X - ROAD_W, SCREEN_H, TFT_DARKGREEN);

  canvas.fillRect(ROAD_X, 0, ROAD_W, SCREEN_H, TFT_DARKGREY);
  canvas.drawFastVLine(ROAD_X, 0, SCREEN_H, TFT_WHITE);
  canvas.drawFastVLine(ROAD_X + ROAD_W - 1, 0, SCREEN_H, TFT_WHITE);

  int dashOffset = ((int)roadScroll) % 20;
  for (int y = -20 + dashOffset; y < SCREEN_H; y += 20)
    canvas.fillRect(SCREEN_W / 2 - 1, y, 2, 10, TFT_YELLOW);

  for (int y = -10 + (((int)roadScroll * 2) % 15); y < SCREEN_H; y += 15) {
    canvas.fillRect(ROAD_X + 5, y, 3, 6, TFT_LIGHTGREY);
    canvas.fillRect(ROAD_X + ROAD_W - 8, y, 3, 6, TFT_LIGHTGREY);
  }

  for (int i = 0; i < MAX_ENEMIES; i++)
    if (enemies[i].active) drawCar(enemies[i], TFT_RED);

  bool blink = (millis() < invulnUntil) && ((millis() / 100) % 2);
  if (!blink) drawCar(player, TFT_GREEN);

  if (millis() < crashFlashUntil)
    canvas.fillRect(0, 0, SCREEN_W, SCREEN_H, TFT_RED);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(3, 3);
  canvas.printf("SCORE %d", score);
  canvas.setCursor(SCREEN_W - 75, 3);
  canvas.printf("HI %d", highScore);

  canvas.setCursor(3, SCREEN_H - 12);
  canvas.print("LIVES:");
  for (int i = 0; i < lives; i++)
    canvas.fillCircle(55 + i * 10, SCREEN_H - 8, 3, TFT_RED);
}

void updateGame(unsigned long now) {
  Keys k;
  readKeys(k);

  float target = 0;
  if (k.left) target = -3.5f;
  if (k.right) target = 3.5f;
  playerVX += (target - playerVX) * 0.35f;
  player.x += playerVX;

  if (player.x < ROAD_X + 2) {
    player.x = ROAD_X + 2;
    playerVX = 0;
  }
  if (player.x + player.w > ROAD_X + ROAD_W - 2) {
    player.x = ROAD_X + ROAD_W - 2 - player.w;
    playerVX = 0;
  }

  baseSpeed = 1.8f + score / 200.0f;
  if (baseSpeed > 5.0f) baseSpeed = 5.0f;

  int spawnInterval = 900 - (int)(baseSpeed * 80);
  if (spawnInterval < 350) spawnInterval = 350;
  if (now - lastSpawn > (unsigned long)spawnInterval) {
    spawnEnemy();
    lastSpawn = now;
  }

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    enemies[i].y += enemies[i].speed;

    if (enemies[i].y > SCREEN_H) {
      enemies[i].active = false;
      score += 10;
      continue;
    }

    if (now > invulnUntil && collide(player, enemies[i])) {
      enemies[i].active = false;
      lives--;
      score = max(0, score - 25);
      invulnUntil = now + 2000;
      crashFlashUntil = now + 100;
      beep(120, 250);
      if (lives <= 0) {
        crashFlashUntil = 0;
        saveHS();
        beep(80, 400);
        state = GAME_OVER;
        return;
      }
    }
  }
  roadScroll += baseSpeed;
}

void drawMenu() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.setTextSize(3);
  canvas.setCursor(55, 18);
  canvas.print("RACE");

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(30, 60);
  canvas.print("HIGH SCORE: ");
  canvas.print(highScore);

  canvas.setCursor(20, 80);
  canvas.print(", . or A D to drive");

  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.setCursor(35, 110);
  canvas.print("Press ENTER to play");
  canvas.pushSprite(0, 0);
}

void drawGameOver() {
  drawGame();
  canvas.fillRect(40, 40, SCREEN_W - 80, 60, TFT_BLACK);
  canvas.drawRect(40, 40, SCREEN_W - 80, 60, TFT_RED);
  canvas.setTextColor(TFT_RED, TFT_BLACK);
  canvas.setTextSize(2);
  canvas.setCursor(48, 46);
  canvas.print("GAME OVER");
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(60, 75);
  canvas.print("Score: ");
  canvas.print(score);
  canvas.setCursor(50, 88);
  canvas.print("Press R for menu");
  canvas.pushSprite(0, 0);
}

void loop() {
  M5Cardputer.update();
  unsigned long now = millis();

  Keys k;
  readKeys(k);

  if (state == MENU) {
    if (pressedOnce(k.enter, prevK.enter) || pressedOnce(k.space, prevK.space)) {
      resetGame();
      state = PLAYING;
      beep(600, 100);
      prevK = k;
      return;
    }
    if (now - lastRender >= FRAME_INTERVAL_MS) {
      drawMenu();
      lastRender = now;
    }
    prevK = k;
    delay(30);
    return;
  }

  if (state == PLAYING) {
    if (pressedOnce(k.p, prevK.p)) {
      state = PAUSED;
      prevK = k;
      return;
    }
    updateGame(now);
    if (state == PLAYING && now - lastRender >= FRAME_INTERVAL_MS) {
      drawGame();
      canvas.pushSprite(0, 0);
      lastRender = now;
    } else if (state == GAME_OVER && now - lastRender >= FRAME_INTERVAL_MS) {
      drawGameOver();
      lastRender = now;
    }
    prevK = k;
    delay(10);
    return;
  }

  if (state == PAUSED) {
    if (pressedOnce(k.p, prevK.p) || pressedOnce(k.enter, prevK.enter)) {
      state = PLAYING;
      prevK = k;
      return;
    }
    canvas.fillRect(60, 50, 120, 40, TFT_BLACK);
    canvas.drawRect(60, 50, 120, 40, TFT_YELLOW);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.setTextSize(2);
    canvas.setCursor(78, 58);
    canvas.print("PAUSED");
    if (now - lastRender >= FRAME_INTERVAL_MS) {
      canvas.pushSprite(0, 0);
      lastRender = now;
    }
    prevK = k;
    delay(30);
    return;
  }

  if (state == GAME_OVER) {
    if (pressedOnce(k.r, prevK.r)) state = MENU;
    if (now - lastRender >= FRAME_INTERVAL_MS) {
      drawGameOver();
      lastRender = now;
    }
    prevK = k;
    delay(30);
    return;
  }
}
