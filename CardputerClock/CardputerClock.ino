/*
  CardputerClock.ino  -  Desk clock for M5Stack Cardputer ADV (80s desktop electronics style)

  Libraries (Library Manager):  M5Cardputer (pulls M5Unified + M5GFX),  AnimatedGIF (Larry Bank)
  Board: M5Stack-StampS3 / M5Cardputer.   SD card: FAT32.

  SD layout:
    /gifs/*.gif          background animations (also scanned: *.gif in SD root)
    /clock/clock.cfg     settings (created automatically)
    /clock/cache/*.cbg   converted backgrounds (created automatically, safe to delete)

  BACKGROUND PIPELINE (why big GIFs do not lag)
    The GIF is NOT decoded while the clock runs. The first time a GIF is chosen it is converted once
    (progress screen, any key cancels) into a cache file: 240x135 (cover-scaled, full screen),
    256-color palette, constant frame rate (<=12 fps), fixed-size frames.
    Playback is then just "read the next 32 KB frame from SD":
      - cost per frame is the same for any source GIF size / length / frame count
      - position comes from a 64-bit wall clock, so a slow SD can only drop frames, never accumulate lag
      - no heap allocation in the run loop, no GIF decoder, no LZW
      - brightness / color / glass are applied to the 256 palette entries only (not per pixel)
      - SD read errors -> automatic remount + reopen; missing card -> black background, retries every 30 s

  KEYS
    TAB ........ next screen (CLOCK > CALENDAR > TIMER > ALARM > STOPWATCH > SETTINGS)
    ` (ESC) .... back to CLOCK
    ; . , /  ... up / down / left / right
  CLOCK:  , /  dial color     ; .  dial brightness
          g  next GIF         c  GIF color mode     v b  GIF brightness - / +
          s  sync time over Wi-Fi
  CALENDAR: , / month   ; . year   t today
  TIMER:  digits (microwave style)  ENTER start/pause  DEL back/clear
  ALARM:  ; . select  digits HHMM + ENTER set   ENTER toggle   , / repeat mode
  STOPW:  ENTER/SPACE start-stop   l lap   DEL reset
  SETTINGS: ; . select   , / change   ENTER action
  GIF PICKER: ; . select  ENTER=use  r=forget cache (rebuild on next use)  DEL=cancel
  RINGING: any key = stop,  ENTER/SPACE on alarm = snooze 5 min
*/

#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AnimatedGIF.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>
#include <vector>
#include <algorithm>

#define SD_SCK  40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS   12

static const int SCR_W = 240, SCR_H = 135;
static const int PX = 4, PY = 4, PW = 232, PH = 127;   // "glass" panel behind the UI

M5Canvas S(&M5Cardputer.Display);
AnimatedGIF gif;     // used only by the one-time converter

using DT = lgfx::textdatum::textdatum_t;
static const DT D_TL = lgfx::textdatum::top_left;
static const DT D_TC = lgfx::textdatum::top_center;
static const DT D_TR = lgfx::textdatum::top_right;
static const DT D_MC = lgfx::textdatum::middle_center;

// ------------------------------------------------------------------ palette
struct Pal { const char* name; uint8_t r, g, b; };
const Pal PAL[] = {
  {"RED", 255, 30, 20}, {"AMBER", 255, 150, 0}, {"YELLOW", 255, 225, 40},
  {"GREEN", 50, 255, 70}, {"VFD", 80, 255, 210}, {"CYAN", 0, 215, 255},
  {"BLUE", 70, 120, 255}, {"MAGENTA", 255, 50, 200}, {"WHITE", 240, 240, 255}};
const int NPAL = 9;

const char* WD[7]  = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* MON[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
const char* MONF[12] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
const char* REP[4] = {"ONCE", "DAILY", "MO-FR", "SA-SU"};

// ------------------------------------------------------------------ settings
uint8_t dialColor = 1, dialBr = 100, gifTint = 0, gifBr = 80, glassLvl = 1, backlight = 80;
bool h24 = true, autoTz = true;
String gifPath = "", ssid = "", wpass = "";
int tzMin = 180;
struct Alarm { uint8_t h, m; bool on; uint8_t rep; };
Alarm al[4] = {{7, 0, false, 1}, {8, 0, false, 2}, {12, 0, false, 0}, {22, 0, false, 1}};
bool cfgDirty = false; uint32_t cfgDirtyT = 0;
bool sdOK = false;
const char* CFG = "/clock/clock.cfg";

uint16_t cOn, cMid, cGhost, cSel;

// ------------------------------------------------------------------ state
enum { SC_CLOCK, SC_CAL, SC_TIMER, SC_ALARM, SC_STOPW, SC_SET, SC_COUNT };
enum { M_NORMAL, M_TEXT, M_PICK };
uint8_t screen = SC_CLOCK, mode = M_NORMAL;
bool dirty = true; uint32_t lastRender = 0;
uint32_t lastSecKey = 0;

// time sync
uint8_t syncState = 0;   // 0 idle 1 wifi 2 ntp
uint32_t syncT0 = 0, nextSyncAt = 0;
bool synced = false; String syncMsg = "-";

// ringing
uint8_t ringKind = 0, ringIdx = 0; uint32_t ringT0 = 0; int32_t lastSlot = -1;
time_t snoozeAt = 0; uint8_t snoozeIdx = 0;
uint32_t lastAlarmKey = 0;

// timer
uint8_t tmState = 0; uint32_t tmDigits = 0, tmEnd = 0; int32_t tmRem = 0;
// stopwatch
bool swRun = false; uint32_t swAcc = 0, swStart = 0; uint32_t laps[20]; int lapN = 0;
// calendar
int viewY = 2026, viewM = 0;
// alarm screen
int alCur = 0; String alBuf = "";
// settings screen
int sCur = 0; const int NSET = 14;
// text input
String inBuf; uint8_t inTarget = 0;
// gif picker
std::vector<String> gifList; std::vector<uint8_t> gifCached; int pickCur = 0;
int battLvl = -1; uint32_t battT = 0;

// ------------------------------------------------------------------ helpers
void markCfg() { cfgDirty = true; cfgDirtyT = millis(); }

uint16_t mk(const Pal& p, int pct) {
  return S.color565(p.r * pct / 100, p.g * pct / 100, p.b * pct / 100);
}
void updateColors() {
  const Pal& p = PAL[dialColor];
  cOn = mk(p, dialBr);
  cMid = mk(p, dialBr * 55 / 100);
  cGhost = mk(p, max(5, (int)dialBr / 11));
  cSel = mk(p, dialBr * 30 / 100);
}
void applyBacklight() { M5Cardputer.Display.setBrightness(backlight * 255 / 100); }

int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
int daysInMonth(int y, int m) {
  static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return dm[m];
}
void getLocal(struct tm& t) { time_t n = time(nullptr) + (time_t)tzMin * 60; gmtime_r(&n, &t); }

String baseName(const String& p) { int i = p.lastIndexOf('/'); return i >= 0 ? p.substring(i + 1) : p; }
String fit(const String& s, int n) { return s.length() <= (unsigned)n ? s : s.substring(0, n - 1) + "~"; }

// ------------------------------------------------------------------ config
void saveCfg() {
  cfgDirty = false;
  if (!sdOK) return;
  SD.mkdir("/clock");
  SD.remove(CFG);
  File f = SD.open(CFG, FILE_WRITE);
  if (!f) return;
  f.printf("dialColor=%d\n", dialColor); f.printf("dialBr=%d\n", dialBr);
  f.print("gif="); f.println(gifPath);
  f.printf("gifTint=%d\n", gifTint); f.printf("gifBr=%d\n", gifBr);
  f.printf("glass=%d\n", glassLvl); f.printf("backlight=%d\n", backlight);
  f.printf("h24=%d\n", h24); f.printf("autoTz=%d\n", autoTz); f.printf("tzMin=%d\n", tzMin);
  f.print("ssid="); f.println(ssid);
  f.print("pass="); f.println(wpass);
  for (int i = 0; i < 4; i++) f.printf("al%d=%d,%d,%d,%d\n", i, al[i].h, al[i].m, al[i].on, al[i].rep);
  time_t n = time(nullptr);
  f.printf("epoch=%lu\n", (unsigned long)(n > 1700000000 ? n : 0));
  f.close();
}
void loadCfg() {
  if (!sdOK || !SD.exists(CFG)) return;
  File f = SD.open(CFG);
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n'); line.trim();
    int e = line.indexOf('='); if (e < 0) continue;
    String k = line.substring(0, e), v = line.substring(e + 1);
    if (k == "dialColor") dialColor = constrain(v.toInt(), 0, NPAL - 1);
    else if (k == "dialBr") dialBr = constrain(v.toInt(), 10, 100);
    else if (k == "gif") gifPath = v;
    else if (k == "gifTint") gifTint = constrain(v.toInt(), 0, NPAL);
    else if (k == "gifBr") gifBr = constrain(v.toInt(), 10, 100);
    else if (k == "glass") glassLvl = constrain(v.toInt(), 0, 2);
    else if (k == "backlight") backlight = constrain(v.toInt(), 10, 100);
    else if (k == "h24") h24 = v.toInt();
    else if (k == "autoTz") autoTz = v.toInt();
    else if (k == "tzMin") tzMin = constrain(v.toInt(), -720, 840);
    else if (k == "ssid") ssid = v;
    else if (k == "pass") wpass = v;
    else if (k.startsWith("al") && k.length() == 3) {
      int i = k[2] - '0';
      if (i >= 0 && i < 4) {
        int a, b, c, d;
        if (sscanf(v.c_str(), "%d,%d,%d,%d", &a, &b, &c, &d) == 4) {
          al[i].h = constrain(a, 0, 23); al[i].m = constrain(b, 0, 59); al[i].on = c; al[i].rep = constrain(d, 0, 3);
        }
      }
    } else if (k == "epoch") {
      uint32_t ep = strtoul(v.c_str(), nullptr, 10);
      if (ep > 1700000000) { timeval tv = {(time_t)ep, 0}; settimeofday(&tv, nullptr); }
    }
  }
  f.close();
}

// =====================================================================================
//  BACKGROUND: GIF -> cached 8-bit stream
// =====================================================================================
static const uint32_t BG_FS = (uint32_t)SCR_W * SCR_H;   // bytes per frame (1 byte / pixel)
static const uint32_t BG_HDR = 32;                       // header, then 256 x RGB565 palette, then frames
static const uint32_t BG_PAL = 512;
static const uint32_t BG_MAXF = 400;                     // max frames in cache (~13 MB)
static const char* CACHE_DIR = "/clock/cache";

struct BgHdr {
  char magic[4];                    // "CBG1" valid, "CBG0" being written
  uint16_t w, h, frames, frameMs;
  uint32_t srcSize, srcTime;
  uint8_t rsv[12];
} __attribute__((packed));
static_assert(sizeof(BgHdr) == 32, "BgHdr size");

// playback (all static: no heap use while running)
File bgFile; bool bgOn = false;
uint16_t bgFrames = 0, bgFrameMs = 83;
int bgCur = -2; uint64_t bgT0 = 0; int bgFail = 0; uint32_t bgRetryAt = 0;
uint8_t bgIdx[BG_FS];
uint16_t palRaw[256], palN[256], palG[256];

// converter temporaries (allocated only during conversion)
uint16_t* cvBuf = nullptr; uint32_t* cvHist = nullptr; uint8_t* cvLut = nullptr;
uint8_t cvPass = 0; bool cvCancel = false; volatile int32_t cvPos = 0, cvSize = 1;
uint16_t sxMap[SCR_W], syMap[SCR_H];
String cvName;

String cachePath(const String& src) { return String(CACHE_DIR) + "/" + baseName(src) + ".cbg"; }
static inline uint16_t bin444(uint16_t p) { return ((p >> 12) << 8) | (((p >> 7) & 0xF) << 4) | ((p >> 1) & 0xF); }

// palette entries -> brightness / tint, plus darker "glass" variant for the panel area
void rebuildPal() {
  int m = gifBr * 256 / 100;
  const Pal& tp = PAL[gifTint > 0 ? gifTint - 1 : 0];
  for (int i = 0; i < 256; i++) {
    uint16_t p = palRaw[i];
    int r8 = (p >> 11) << 3, g8 = ((p >> 5) & 63) << 2, b8 = (p & 31) << 3, r, g, b;
    // Keep the animation visible but dark enough for readable foreground text.
    const int bgGain = (m * 112) >> 8;
    if (gifTint == 0) { r = (r8 * bgGain) >> 8; g = (g8 * bgGain) >> 8; b = (b8 * bgGain) >> 8; }
    else {
      int y = (r8 * 77 + g8 * 150 + b8 * 29) >> 8;
      r = ((tp.r * y / 255) * bgGain) >> 8;
      g = ((tp.g * y / 255) * bgGain) >> 8;
      b = ((tp.b * y / 255) * bgGain) >> 8;
    }
    uint16_t c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    palN[i] = c;
    // Text-card regions receive a second darkening pass, independent of GIF colors.
    palG[i] = ((c >> 2) & 0x39E7);
  }
}

// ---- converter: AnimatedGIF callbacks
void* GIFOpenFile(const char* fname, int32_t* pSize) {
  static File f;
  f = SD.open(fname);
  if (f) { *pSize = f.size(); return (void*)&f; }
  return NULL;
}
void GIFCloseFile(void* h) { File* f = (File*)h; if (f) f->close(); }
int32_t GIFReadFile(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  int32_t n = iLen;
  File* f = (File*)pFile->fHandle;
  if ((pFile->iSize - pFile->iPos) < iLen) n = pFile->iSize - pFile->iPos - 1;
  if (n <= 0) return 0;
  n = (int32_t)f->read(pBuf, n);
  pFile->iPos = f->position();
  cvPos = pFile->iPos; cvSize = pFile->iSize > 0 ? pFile->iSize : 1;
  return n;
}
int32_t GIFSeekFile(GIFFILE* pFile, int32_t pos) {
  File* f = (File*)pFile->fHandle;
  f->seek(pos);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}
void GIFDraw(GIFDRAW* d) {
  uint8_t* s = d->pPixels; uint16_t* pal = d->pPalette;
  bool hasT = d->ucHasTransparency; uint8_t tr = d->ucTransparent;
  if (cvPass == 1) {                       // pass 1: color histogram only (every 2nd line is enough)
    if (!cvHist || (d->y & 1)) return;
    for (int x = 0; x < d->iWidth; x++) {
      uint8_t ix = s[x]; if (hasT && ix == tr) continue;
      cvHist[bin444(pal[ix])]++;
    }
    return;
  }
  if (!cvBuf) return;                      // pass 2: composite into full-screen buffer (cover-scaled)
  int sy = d->iY + d->y;
  int x0 = d->iX, x1 = d->iX + d->iWidth;
  bool restore = (d->ucDisposalMethod == 2);
  uint16_t bg = pal[d->ucBackground];
  for (int dy = 0; dy < SCR_H; dy++) {
    if (syMap[dy] != sy) continue;
    uint16_t* o = cvBuf + dy * SCR_W;
    for (int dx = 0; dx < SCR_W; dx++) {
      int sx = sxMap[dx];
      if (sx < x0 || sx >= x1) continue;
      uint8_t ix = s[sx - x0];
      if (hasT && ix == tr) { if (restore) o[dx] = bg; continue; }
      o[dx] = pal[ix];
    }
  }
}

// ---- converter UI (draws straight to the display; sprite is freed during conversion)
void convUI(const char* phase, int pct, bool force = false) {
  static uint32_t last = 0; uint32_t now = millis();
  if (!force && now - last < 200) return;
  last = now;
  auto& D = M5Cardputer.Display;
  D.startWrite();
  if (force) {
    D.fillScreen(TFT_BLACK);
    D.setFont(&fonts::Font2); D.setTextSize(1); D.setTextDatum(D_TC); D.setTextColor(cOn);
    D.drawString("CONVERTING GIF", 120, 8);
    D.setFont(&fonts::Font0); D.setTextColor(cMid);
    D.drawString(fit(baseName(cvName), 34).c_str(), 120, 30);
    D.drawString("FIRST USE ONLY - RESULT IS CACHED ON SD", 120, 104);
    D.drawString("ANY KEY = CANCEL", 120, 118);
  }
  D.fillRect(0, 44, 240, 56, TFT_BLACK);
  D.setFont(&fonts::Font2); D.setTextDatum(D_TC); D.setTextColor(cOn);
  D.drawString(phase, 120, 48);
  D.drawRect(20, 74, 200, 16, cMid);
  D.fillRect(22, 76, (196 * constrain(pct, 0, 100)) / 100, 12, cOn);
  D.endWrite();
}
bool convPoll(const char* phase, int pct) {
  convUI(phase, pct);
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) cvCancel = true;
  delay(1);
  return !cvCancel;
}

// ---- median-cut: histogram (4096 bins of RGB444) -> 256-color palette + bin->index LUT
bool buildPalette() {
  uint16_t* ids = (uint16_t*)malloc(4096 * 2);
  if (!ids) return false;
  int n = 0;
  for (int i = 0; i < 4096; i++) if (cvHist[i]) ids[n++] = i;
  if (n == 0) { ids[n++] = 0; cvHist[0] = 1; }
  struct Box { int lo, hi; };
  Box bx[256]; int nb = 1; bx[0] = {0, n};
  auto ch = [](uint16_t b, int c) -> int { return c == 0 ? ((b >> 8) & 15) : (c == 1 ? ((b >> 4) & 15) : (b & 15)); };
  while (nb < 256) {
    int best = -1; uint64_t bp = 0;
    for (int k = 0; k < nb; k++) {
      if (bx[k].hi - bx[k].lo < 2) continue;
      uint64_t pop = 0; for (int i = bx[k].lo; i < bx[k].hi; i++) pop += cvHist[ids[i]];
      if (best < 0 || pop > bp) { bp = pop; best = k; }
    }
    if (best < 0) break;
    int lo = bx[best].lo, hi = bx[best].hi;
    int mn[3] = {15, 15, 15}, mx[3] = {0, 0, 0};
    for (int i = lo; i < hi; i++) for (int c = 0; c < 3; c++) { int v = ch(ids[i], c); if (v < mn[c]) mn[c] = v; if (v > mx[c]) mx[c] = v; }
    int axis = 0, ext = mx[0] - mn[0];
    for (int c = 1; c < 3; c++) if (mx[c] - mn[c] > ext) { ext = mx[c] - mn[c]; axis = c; }
    std::sort(ids + lo, ids + hi, [&](uint16_t a, uint16_t b) { return ch(a, axis) < ch(b, axis); });
    uint64_t half = bp / 2, acc = 0; int mid = lo + 1;
    for (int i = lo; i < hi - 1; i++) { acc += cvHist[ids[i]]; mid = i + 1; if (acc >= half) break; }
    if (mid <= lo) mid = lo + 1;
    if (mid >= hi) mid = hi - 1;
    bx[nb] = {mid, hi}; bx[best].hi = mid; nb++;
  }
  uint8_t pr[256], pg[256], pb[256];
  memset(palRaw, 0, sizeof(palRaw));
  for (int k = 0; k < 256; k++) { pr[k] = pg[k] = pb[k] = 0; }
  for (int k = 0; k < nb; k++) {
    uint64_t pop = 0, sr = 0, sg = 0, sb = 0;
    for (int i = bx[k].lo; i < bx[k].hi; i++) {
      uint32_t c = cvHist[ids[i]]; pop += c;
      sr += (uint64_t)c * ch(ids[i], 0); sg += (uint64_t)c * ch(ids[i], 1); sb += (uint64_t)c * ch(ids[i], 2);
    }
    if (!pop) pop = 1;
    int r = (int)((sr * 17 + pop / 2) / pop), g = (int)((sg * 17 + pop / 2) / pop), b = (int)((sb * 17 + pop / 2) / pop);
    pr[k] = r; pg[k] = g; pb[k] = b;
    palRaw[k] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  }
  for (int bin = 0; bin < 4096; bin++) {
    int r = ((bin >> 8) & 15) * 17, g = ((bin >> 4) & 15) * 17, b = (bin & 15) * 17;
    int bestK = 0; int bestD = 1 << 30;
    for (int k = 0; k < nb; k++) {
      int dr = r - pr[k], dg = g - pg[k], db = b - pb[k];
      int dd = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
      if (dd < bestD) { bestD = dd; bestK = k; }
    }
    cvLut[bin] = bestK;
  }
  free(ids);
  return true;
}
bool writeFrame(File& f) {
  for (uint32_t i = 0; i < BG_FS; i++) bgIdx[i] = cvLut[bin444(cvBuf[i])];
  return f.write(bgIdx, BG_FS) == BG_FS;
}

void closeBg() {
  if (bgFile) bgFile.close();
  bgOn = false; bgCur = -2;
}

bool doConvert(const String& src, const String& dst, uint32_t srcSize, uint32_t srcTime, String& err) {
  cvCancel = false; cvPass = 1; cvPos = 0; cvSize = 1;
  convUI("OPENING", 0, true);
  if (!gif.open(src.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    err = (gif.getLastError() == GIF_TOO_WIDE) ? "GIF TOO WIDE" : "BAD GIF";
    return false;
  }
  int W = gif.getCanvasWidth(), H = gif.getCanvasHeight();
  if (W < 1 || H < 1) { gif.close(); err = "BAD GIF"; return false; }
  float sc = max((float)SCR_W / W, (float)SCR_H / H);        // cover: fills the whole screen
  float offX = (W * sc - SCR_W) / 2, offY = (H * sc - SCR_H) / 2;
  for (int x = 0; x < SCR_W; x++) sxMap[x] = constrain((int)((x + offX) / sc), 0, W - 1);
  for (int y = 0; y < SCR_H; y++) syMap[y] = constrain((int)((y + offY) / sc), 0, H - 1);

  // ---- pass 1: colors + frame count + duration
  uint32_t n = 0, D = 0; int fd = 0, r;
  do {
    r = gif.playFrame(false, &fd);
    if (r < 0) { if (n == 0) { gif.close(); err = "DECODE ERROR"; return false; } break; }
    n++; D += (fd <= 10 ? 100 : fd);
    if (!convPoll("1/2  ANALYZING COLORS", (int)((int64_t)cvPos * 45 / cvSize))) { gif.close(); err = "CANCELLED"; return false; }
  } while (r > 0);
  uint32_t avg = n ? D / n : 100;
  uint32_t outMs = avg < 83 ? 83 : (avg > 200 ? 200 : avg);
  uint32_t total = (D + outMs - 1) / outMs;
  if (total > BG_MAXF) {
    outMs = (D + BG_MAXF - 1) / BG_MAXF; if (outMs > 200) outMs = 200;
    total = (D + outMs - 1) / outMs; if (total > BG_MAXF) total = BG_MAXF;
  }
  if (total < 1) total = 1;
  convUI("BUILDING PALETTE", 48, true);
  if (!buildPalette()) { gif.close(); err = "LOW MEMORY"; return false; }

  // ---- pass 2: composite -> quantize -> write constant-rate frames
  gif.reset(); cvPass = 2; cvPos = 0;
  SD.remove(dst.c_str());
  File f = SD.open(dst.c_str(), FILE_WRITE);
  if (!f) { gif.close(); err = "SD WRITE ERROR"; return false; }
  BgHdr h; memset(&h, 0, sizeof(h)); memcpy(h.magic, "CBG0", 4);
  h.w = SCR_W; h.h = SCR_H; h.frames = 0; h.frameMs = outMs; h.srcSize = srcSize; h.srcTime = srcTime;
  f.write((uint8_t*)&h, sizeof(h)); f.write((uint8_t*)palRaw, BG_PAL);
  uint32_t t0 = 0, emitted = 0; bool fail = false;
  do {
    r = gif.playFrame(false, &fd);
    if (r < 0) break;
    if (fd <= 10) fd = 100;
    uint32_t t1 = t0 + fd;
    while (emitted < total && emitted * outMs < t1) {
      if (!writeFrame(f)) { fail = true; break; }
      emitted++;
    }
    t0 = t1;
    if (fail) break;
    if (!convPoll("2/2  WRITING FRAMES", 50 + (int)((int64_t)cvPos * 50 / cvSize))) { f.close(); gif.close(); SD.remove(dst.c_str()); err = "CANCELLED"; return false; }
  } while (r > 0 && emitted < total);
  while (!fail && emitted < total) { if (!writeFrame(f)) fail = true; else emitted++; }
  gif.close();
  if (fail || emitted == 0) { f.close(); SD.remove(dst.c_str()); err = "SD FULL?"; return false; }
  memcpy(h.magic, "CBG1", 4); h.frames = emitted;
  f.seek(0); f.write((uint8_t*)&h, sizeof(h));
  f.close();
  return true;
}

bool convertGif(const String& src, const String& dst, String& err) {
  err = "";
  if (syncState) {   // free Wi-Fi memory first
    WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF); syncState = 0; nextSyncAt = millis() + 60000;
  }
  closeBg();
  File sf = SD.open(src.c_str());
  if (!sf) { err = "FILE NOT FOUND"; return false; }
  uint32_t srcSize = sf.size(), srcTime = (uint32_t)sf.getLastWrite();
  sf.close();
  SD.mkdir("/clock"); SD.mkdir(CACHE_DIR);
  cvName = src;
  S.deleteSprite();   // frees 64 KB for the converter
  cvBuf = (uint16_t*)heap_caps_malloc(SCR_W * SCR_H * 2, MALLOC_CAP_8BIT);
  cvHist = (uint32_t*)heap_caps_calloc(4096, 4, MALLOC_CAP_8BIT);
  cvLut = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  bool ok = false;
  if (cvBuf && cvHist && cvLut) { memset(cvBuf, 0, SCR_W * SCR_H * 2); ok = doConvert(src, dst, srcSize, srcTime, err); }
  else err = "LOW MEMORY";
  if (cvBuf) free(cvBuf); if (cvHist) free(cvHist); if (cvLut) free(cvLut);
  cvBuf = nullptr; cvHist = nullptr; cvLut = nullptr; cvPass = 0;
  S.setColorDepth(16); S.createSprite(SCR_W, SCR_H);
  if (!ok) SD.remove(dst.c_str());
  return ok;
}

// ---- playback
bool bgOpenCache(const String& dst, const String& src, bool verify) {
  closeBg();
  if (!SD.exists(dst.c_str())) return false;
  File f = SD.open(dst.c_str(), FILE_READ);
  if (!f) return false;
  BgHdr h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || memcmp(h.magic, "CBG1", 4) || h.w != SCR_W || h.h != SCR_H || h.frames == 0 || h.frameMs < 20) { f.close(); return false; }
  if (verify) {
    File sf = SD.open(src.c_str());
    if (!sf) { f.close(); return false; }
    uint32_t sz = sf.size(), tm = (uint32_t)sf.getLastWrite(); sf.close();
    if (sz != h.srcSize || tm != h.srcTime) { f.close(); return false; }
  }
  if (f.size() < BG_HDR + BG_PAL + (uint32_t)h.frames * BG_FS) { f.close(); return false; }   // truncated
  if (f.read((uint8_t*)palRaw, BG_PAL) != BG_PAL) { f.close(); return false; }
  bgFile = f; bgFrames = h.frames; bgFrameMs = h.frameMs;
  bgCur = -2; bgFail = 0; bgT0 = esp_timer_get_time() / 1000ULL; bgOn = true;
  rebuildPal();
  return true;
}
void applyGif() {
  closeBg();
  if (gifPath.isEmpty() || !sdOK) return;
  String dst = cachePath(gifPath), err;
  if (!bgOpenCache(dst, gifPath, true)) {
    if (!(convertGif(gifPath, dst, err) && bgOpenCache(dst, gifPath, true))) {
      syncMsg = err.length() ? err : String("GIF ERROR");
      gifPath = ""; markCfg();
    }
  }
  dirty = true;
}
bool bgRead(int i) {
  if (i != bgCur + 1) { if (!bgFile.seek(BG_HDR + BG_PAL + (uint32_t)i * BG_FS)) { bgCur = -2; return false; } }
  if (bgFile.read(bgIdx, BG_FS) != BG_FS) { bgCur = -2; return false; }
  bgCur = i; return true;
}
void sdRemount() {
  SD.end(); SPI.end();
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOK = SD.begin(SD_CS, SPI, 25000000);
}
// Called every loop. Frame index comes from a 64-bit wall clock: slow SD = dropped frames, never lag.
void bgTick() {
  uint32_t now = millis();
  if (!sdOK) {
    if ((int32_t)(now - bgRetryAt) >= 0) { bgRetryAt = now + 30000; sdRemount(); }
    return;
  }
  if (!bgOn) {
    if (gifPath.length() && (int32_t)(now - bgRetryAt) >= 0) {
      bgRetryAt = now + 30000;
      if (bgOpenCache(cachePath(gifPath), gifPath, false)) dirty = true;
    }
    return;
  }
  uint64_t ms = esp_timer_get_time() / 1000ULL;
  int idx = (int)(((ms - bgT0) / bgFrameMs) % bgFrames);
  if (idx == bgCur) return;
  if (bgRead(idx)) { bgFail = 0; dirty = true; return; }
  if (++bgFail >= 3) {            // card hiccup: remount and reopen
    bgFail = 0; closeBg(); sdRemount(); bgRetryAt = now + (sdOK ? 500 : 30000);
  }
}

void scanDir(const char* dirPath) {
  File d = SD.open(dirPath);
  if (!d || !d.isDirectory()) { if (d) d.close(); return; }
  File e = d.openNextFile();
  while (e && gifList.size() < 64) {
    if (!e.isDirectory()) {
      String base = baseName(String(e.name()));
      String lo = base; lo.toLowerCase();
      if (lo.endsWith(".gif") && !lo.startsWith(".")) {
        String full = String(dirPath); if (!full.endsWith("/")) full += "/";
        gifList.push_back(full + base);
      }
    }
    e.close(); e = d.openNextFile();
  }
  if (e) e.close();
  d.close();
}
void scanGifs() {
  gifList.clear(); gifCached.clear();
  if (!sdOK) return;
  scanDir("/gifs"); scanDir("/");
  for (size_t i = 0; i < gifList.size(); i++) gifCached.push_back(SD.exists(cachePath(gifList[i]).c_str()) ? 1 : 0);
}

// Compose background (palette-mapped frame + darker glass panel) into the sprite
void composeBg() {
  if (!bgOn) { S.fillSprite(TFT_BLACK); return; }
  static uint16_t line[SCR_W];
  for (int y = 0; y < SCR_H; y++) {
    const uint8_t* s = bgIdx + y * SCR_W;
    if (glassLvl && y >= PY && y < PY + PH) {
      for (int x = 0; x < PX; x++) line[x] = palN[s[x]];
      for (int x = PX; x < PX + PW; x++) line[x] = palG[s[x]];
      for (int x = PX + PW; x < SCR_W; x++) line[x] = palN[s[x]];
    } else {
      for (int x = 0; x < SCR_W; x++) line[x] = palN[s[x]];
    }
    S.pushImage(0, y, SCR_W, 1, line);
  }
}

// ------------------------------------------------------------------ drawing primitives
struct P { int x, y; };
static const int SHEAR = 2;   // italic slant in 1/16
static inline void poly6(const P* p, uint16_t c, int yb) {
  int X[6], Y[6];
  for (int i = 0; i < 6; i++) { X[i] = p[i].x + ((yb - p[i].y) * SHEAR) / 16; Y[i] = p[i].y; }
  S.fillTriangle(X[0], Y[0], X[1], Y[1], X[5], Y[5], c);
  S.fillTriangle(X[1], Y[1], X[2], Y[2], X[4], Y[4], c);
  S.fillTriangle(X[1], Y[1], X[4], Y[4], X[5], Y[5], c);
  S.fillTriangle(X[2], Y[2], X[3], Y[3], X[4], Y[4], c);
}
// d: 0-9 digit, 10 = minus, <0 = blank (only ghost segments)
void drawDigit(int x, int y, int w, int h, int t, int d, uint16_t on, uint16_t off) {
  static const uint8_t M[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  uint8_t m = (d >= 0 && d <= 9) ? M[d] : (d == 10 ? 0x40 : 0);
  int hf = t / 2, g = t / 6 + 1, yb = y + h;
  int xl = x + hf, xr = x + w - hf, yt = y + hf, ym = y + h / 2, yB = y + h - hf;
  auto Hs = [&](int x0, int x1, int yc, uint16_t c) {
    P p[6] = {{x0, yc}, {x0 + hf, yc - hf}, {x1 - hf, yc - hf}, {x1, yc}, {x1 - hf, yc + hf}, {x0 + hf, yc + hf}};
    poly6(p, c, yb);
  };
  auto Vs = [&](int xc, int y0, int y1, uint16_t c) {
    P p[6] = {{xc, y0}, {xc + hf, y0 + hf}, {xc + hf, y1 - hf}, {xc, y1}, {xc - hf, y1 - hf}, {xc - hf, y0 + hf}};
    poly6(p, c, yb);
  };
  Hs(xl + g, xr - g, yt, (m & 1) ? on : off);
  Vs(xr, yt + g, ym - g, (m & 2) ? on : off);
  Vs(xr, ym + g, yB - g, (m & 4) ? on : off);
  Hs(xl + g, xr - g, yB, (m & 8) ? on : off);
  Vs(xl, ym + g, yB - g, (m & 16) ? on : off);
  Vs(xl, yt + g, ym - g, (m & 32) ? on : off);
  Hs(xl + g, xr - g, ym, (m & 64) ? on : off);
}
void colon(int x, int y, int h, int sz, uint16_t c) {
  int yb = y + h; int ys[2] = {y + h * 3 / 10, y + h * 7 / 10};
  for (int k = 0; k < 2; k++) {
    int xx = x + ((yb - ys[k]) * SHEAR) / 16;
    S.fillRect(xx - sz / 2, ys[k] - sz / 2, sz, sz, c);
  }
}
// char* overload is the hot path (no String allocations while the clock runs for days)
void txt(const char* s, int x, int y, uint16_t c, const lgfx::IFont* f, DT dt = D_TL, float sz = 1) {
  S.setFont(f); S.setTextSize(sz); S.setTextDatum(dt); S.setTextColor(c); S.drawString(s, x, y);
}
void txt(const String& s, int x, int y, uint16_t c, const lgfx::IFont* f, DT dt = D_TL, float sz = 1) {
  txt(s.c_str(), x, y, c, f, dt, sz);
}

// ------------------------------------------------------------------ Wi-Fi / NTP sync
void startSync() {
  if (ssid.isEmpty()) { syncMsg = "NO SSID"; return; }
  if (ESP.getMaxAllocHeap() < 60000) { syncMsg = "LOW MEM"; nextSyncAt = millis() + 600000UL; return; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), wpass.c_str());
  syncState = 1; syncT0 = millis(); syncMsg = "WIFI...";
}
void finishSync(bool ok, const char* msg) {
  syncMsg = msg; syncState = 0;
  WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF);
  if (ok) { synced = true; nextSyncAt = millis() + 3UL * 3600UL * 1000UL; saveCfg(); }
  else nextSyncAt = millis() + 5UL * 60UL * 1000UL;
}
void tickSync(uint32_t now) {
  if (syncState == 0) {
    if (ssid.length() && (int32_t)(now - nextSyncAt) >= 0) startSync();
    return;
  }
  if (syncState == 1) {
    if (WiFi.status() == WL_CONNECTED) {
      sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
      configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
      syncState = 2; syncT0 = now; syncMsg = "NTP...";
    } else if (now - syncT0 > 20000) finishSync(false, "WIFI FAIL");
  } else if (syncState == 2) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      if (autoTz) {
        HTTPClient http; http.setTimeout(3000);
        if (http.begin("http://ip-api.com/json/?fields=status,offset")) {
          if (http.GET() == 200) {
            String b = http.getString(); int i = b.indexOf("\"offset\":");
            if (i >= 0) tzMin = constrain((int)(b.substring(i + 9).toInt() / 60), -720, 840);
          }
          http.end();
        }
      }
      finishSync(true, "SYNC OK");
    } else if (now - syncT0 > 12000) finishSync(false, "NTP FAIL");
  }
}
bool setLocalTime(int y, int mo, int d, int hh, int mi) {
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > daysInMonth(y, mo - 1) || hh > 23 || mi > 59) return false;
  int64_t loc = daysFromCivil(y, mo, d) * 86400LL + hh * 3600 + mi * 60;
  timeval tv = {(time_t)(loc - (int64_t)tzMin * 60), 0};
  settimeofday(&tv, nullptr);
  synced = true; syncMsg = "MANUAL";
  return true;
}

// ------------------------------------------------------------------ ringing
void startRing(uint8_t kind, uint8_t idx) {
  ringKind = kind; ringIdx = idx; ringT0 = millis(); lastSlot = -1;
  M5Cardputer.Display.setBrightness(max(backlight, (uint8_t)60) * 255 / 100);
}
void stopRing(bool snooze) {
  if (ringKind == 2 && snooze) { snoozeAt = time(nullptr) + 300; snoozeIdx = ringIdx; }
  if (ringKind == 1) tmState = 0;
  ringKind = 0; applyBacklight();
}
void tickRing(uint32_t now) {
  if (!ringKind) return;
  int32_t slot = (now - ringT0) / 100;
  if (slot != lastSlot) {
    lastSlot = slot; int p = slot % 16;
    if (p == 0 || p == 2 || p == 4 || p == 6) M5Cardputer.Speaker.tone(ringKind == 2 ? 2800 : 2200, 70);
  }
  if (now - ringT0 > 90000) stopRing(false);
  dirty = true;
}
void tickAlarms() {
  time_t n = time(nullptr);
  uint32_t key = (uint32_t)((n + (time_t)tzMin * 60) / 60);
  if (key != lastAlarmKey) {
    lastAlarmKey = key;
    struct tm t; getLocal(t);
    for (int i = 0; i < 4; i++) {
      if (!al[i].on || al[i].h != t.tm_hour || al[i].m != t.tm_min) continue;
      bool wk = t.tm_wday >= 1 && t.tm_wday <= 5;
      bool ok = al[i].rep <= 1 || (al[i].rep == 2 && wk) || (al[i].rep == 3 && !wk);
      if (!ok) continue;
      if (al[i].rep == 0) { al[i].on = false; markCfg(); }
      if (!ringKind) startRing(2, i);
    }
  }
  if (snoozeAt && n >= snoozeAt && !ringKind) { snoozeAt = 0; startRing(2, snoozeIdx); }
}
void tickTimer(uint32_t now) {
  if (tmState == 1 && (int32_t)(now - tmEnd) >= 0) { tmState = 3; startRing(1, 0); }
}

// ------------------------------------------------------------------ screens
void drawClock() {
  // High-contrast editorial layout: dim animated image, opaque text zones.
  struct tm t; getLocal(t);
  char date[40], clockText[8], secText[4];
  snprintf(date, sizeof(date), "%s  %02d %s  %04d",
           WD[t.tm_wday], t.tm_mday, MON[t.tm_mon], t.tm_year + 1900);
  int hh = t.tm_hour;
  bool pm = hh >= 12;
  if (!h24) { hh %= 12; if (hh == 0) hh = 12; }
  snprintf(clockText, sizeof(clockText), "%02d:%02d", hh, t.tm_min);
  snprintf(secText, sizeof(secText), "%02d", t.tm_sec);

  // Top information strip.
  S.fillRoundRect(7, 6, 226, 20, 5, 0x0841);
  txt(date, 14, 10, 0xC638, &fonts::Font2);

  if (!sdOK) txt("SD OFF", 226, 10, 0xF800, &fonts::Font0, D_TR);
  else if (battLvl >= 0) {
    char bat[10]; snprintf(bat, sizeof(bat), "%d%%", battLvl);
    txt(bat, 226, 11, 0xC638, &fonts::Font0, D_TR);
  }

  // Main time uses a clean proportional font instead of segmented glyphs.
  S.fillRoundRect(7, 32, 226, 66, 7, 0x0000);
  S.drawRoundRect(7, 32, 226, 66, 7, 0x39E7);
  txt(clockText, 120, 38, cOn, &fonts::Font4, D_MC, 1.45f);

  // Seconds and format are deliberately secondary to the time.
  char detail[24];
  snprintf(detail, sizeof(detail), "%s  %s",
           secText, h24 ? "24 HOUR" : (pm ? "PM" : "AM"));
  txt(detail, 120, 82, cMid, &fonts::Font2, D_MC);

  // Status chips remain legible over any animation.
  S.fillRoundRect(7, 106, 226, 22, 5, 0x0841);
  bool anyAl = false; for (int i = 0; i < 4; ++i) anyAl |= al[i].on;
  txt("ALARM", 15, 112, anyAl ? cOn : 0x7BEF, &fonts::Font0);
  txt("TIMER", 75, 112, tmState ? cOn : 0x7BEF, &fonts::Font0);
  txt("STOPWATCH", 135, 112, swRun ? cOn : 0x7BEF, &fonts::Font0);
  const char* status = syncState ? "SYNC" : (synced ? "OK" : "--");
  txt(status, 225, 112, syncState ? cOn : (synced ? 0x07E0 : 0x7BEF),
      &fonts::Font0, D_TR);
}

void drawCalendar() {
  struct tm t; getLocal(t);
  char b[32]; snprintf(b, sizeof(b), "%s %d", MONF[viewM], viewY);
  txt(b, 120, 4, cOn, &fonts::Font2, D_TC);
  const char* H[7] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
  for (int i = 0; i < 7; i++) txt(H[i], 8 + i * 32 + 16, 22, i >= 5 ? cMid : cOn, &fonts::Font0, D_TC);
  int wd = (int)((daysFromCivil(viewY, viewM + 1, 1) + 4) % 7);   // 0=Sun
  int first = (wd + 6) % 7;
  int dim = daysInMonth(viewY, viewM);
  for (int d = 1; d <= dim; d++) {
    int idx = first + d - 1, r = idx / 7, c = idx % 7;
    int cx = 8 + c * 32, cy = 34 + r * 16;
    bool today = (viewY == t.tm_year + 1900 && viewM == t.tm_mon && d == t.tm_mday);
    if (today) S.fillRect(cx + 2, cy, 28, 15, cOn);
    snprintf(b, sizeof(b), "%d", d);
    txt(b, cx + 16, cy + 8, today ? (uint16_t)TFT_BLACK : (c >= 5 ? cMid : cOn), &fonts::Font2, D_MC);
  }
}

void drawHMS(int x, int y, int w, int h, int t, int gap, int hh, int mm, int ss, bool col) {
  int pairW = 2 * w + gap;
  int x0 = x, x1 = x0 + pairW + 16, x2 = x1 + pairW + 16;
  int xs[3] = {x0, x1, x2}; int v[3] = {hh, mm, ss};
  for (int k = 0; k < 3; k++) {
    drawDigit(xs[k], y, w, h, t, v[k] / 10, cOn, cGhost);
    drawDigit(xs[k] + w + gap, y, w, h, t, v[k] % 10, cOn, cGhost);
  }
  colon(x0 + pairW + 4, y, h, 7, col ? cOn : cGhost);
  colon(x1 + pairW + 4, y, h, 7, col ? cOn : cGhost);
}
void drawTimer() {
  const char* st[4] = {"SET", "RUN", "PAUSE", "TIME UP"};
  txt("TIMER", 10, 6, cOn, &fonts::Font2);
  txt(st[tmState], 232, 6, cMid, &fonts::Font2, D_TR);
  int hh, mm, ss; bool colOn = true;
  if (tmState == 0) { hh = tmDigits / 10000; mm = (tmDigits / 100) % 100; ss = tmDigits % 100; }
  else if (tmState == 3) { hh = mm = ss = 0; colOn = (millis() / 400) % 2; }
  else {
    int32_t rem = (tmState == 1) ? (int32_t)(tmEnd - millis()) : tmRem;
    if (rem < 0) rem = 0;
    int tot = (rem + 999) / 1000; hh = tot / 3600; mm = (tot / 60) % 60; ss = tot % 60;
    colOn = tmState == 2 ? (millis() / 400) % 2 : true;
  }
  drawHMS(8, 34, 28, 52, 7, 4, hh, mm, ss, colOn);
  const char* hint = tmState == 0 ? "TYPE DIGITS  ENTER=START  DEL=BACK" : (tmState == 1 ? "ENTER=PAUSE  DEL=RESET" : "ENTER=RESUME  DEL=RESET");
  txt(hint, 120, 104, cMid, &fonts::Font0, D_TC);
}

void drawAlarm() {
  txt("ALARM", 10, 4, cOn, &fonts::Font2);
  txt(alBuf.length() ? "HHMM + ENTER" : "ENTER=ON/OFF  ,/=REPEAT", 232, 8, cMid, &fonts::Font0, D_TR);
  for (int i = 0; i < 4; i++) {
    int y = 24 + i * 25;
    if (i == alCur) S.fillRect(6, y - 2, 228, 25, cSel);
    char nb[4]; snprintf(nb, sizeof(nb), "%d", i + 1);
    txt(nb, 12, y + 4, cMid, &fonts::Font2);
    int hh = al[i].h, mm = al[i].m;
    int dg[4] = {hh / 10, hh % 10, mm / 10, mm % 10};
    if (i == alCur && alBuf.length()) {
      for (int k = 0; k < 4; k++) dg[k] = (k < (int)alBuf.length()) ? alBuf[k] - '0' : -1;
    }
    uint16_t on = al[i].on ? cOn : cMid;
    drawDigit(34, y, 14, 21, 4, dg[0], on, cGhost);
    drawDigit(52, y, 14, 21, 4, dg[1], on, cGhost);
    colon(70, y, 21, 3, on);
    drawDigit(78, y, 14, 21, 4, dg[2], on, cGhost);
    drawDigit(96, y, 14, 21, 4, dg[3], on, cGhost);
    txt(REP[al[i].rep], 132, y + 4, cMid, &fonts::Font2);
    txt(al[i].on ? "ON" : "OFF", 224, y + 4, al[i].on ? cOn : cMid, &fonts::Font2, D_TR);
  }
}

uint32_t swElapsed() { return swAcc + (swRun ? millis() - swStart : 0); }
void fmtCs(uint32_t ms, char* b, size_t n) {
  uint32_t cs = (ms / 10) % 100, s = (ms / 1000) % 60, m = (ms / 60000) % 100;
  snprintf(b, n, "%02u:%02u.%02u", (unsigned)m, (unsigned)s, (unsigned)cs);
}
void drawStopw() {
  txt("STOPWATCH", 10, 6, cOn, &fonts::Font2);
  txt(swRun ? "RUN" : "STOP", 232, 6, cMid, &fonts::Font2, D_TR);
  uint32_t e = swElapsed();
  int m = (e / 60000) % 100, s = (e / 1000) % 60, cs = (e / 10) % 100;
  drawDigit(6, 30, 34, 50, 8, m / 10, cOn, cGhost);
  drawDigit(44, 30, 34, 50, 8, m % 10, cOn, cGhost);
  colon(83, 30, 50, 6, cOn);
  drawDigit(96, 30, 34, 50, 8, s / 10, cOn, cGhost);
  drawDigit(134, 30, 34, 50, 8, s % 10, cOn, cGhost);
  drawDigit(184, 54, 16, 26, 4, cs / 10, cOn, cGhost);
  drawDigit(203, 54, 16, 26, 4, cs % 10, cOn, cGhost);
  if (e >= 3600000UL) { char hb[12]; snprintf(hb, sizeof(hb), "%uH", (unsigned)(e / 3600000UL)); txt(hb, 184, 36, cOn, &fonts::Font2); }
  for (int i = 0; i < 3 && i < lapN; i++) {
    int li = lapN - 1 - i; char a[16]; fmtCs(laps[li], a, sizeof(a));
    uint32_t split = laps[li] - (li > 0 ? laps[li - 1] : 0); char c[16]; fmtCs(split, c, sizeof(c));
    char ln[48]; snprintf(ln, sizeof(ln), "LAP %02d  %s  +%s", li + 1, a, c);
    txt(ln, 10, 92 + i * 12, i == 0 ? cOn : cMid, &fonts::Font0);
  }
  if (!lapN) txt("ENTER=START/STOP  L=LAP  DEL=RESET", 120, 104, cMid, &fonts::Font0, D_TC);
}

String setLabel(int i) {
  static const char* L[NSET] = {"DIAL COLOR", "DIAL BRIGHT", "GIF FILE", "GIF COLOR", "GIF BRIGHT", "GLASS PANEL", "BACKLIGHT",
                                "TIME FORMAT", "WIFI SSID", "WIFI PASS", "AUTO TIMEZONE", "UTC OFFSET", "SYNC NOW", "SET TIME/DATE"};
  return L[i];
}
String setVal(int i) {
  char b[24];
  switch (i) {
    case 0: return PAL[dialColor].name;
    case 1: snprintf(b, sizeof(b), "%d%%", dialBr); return b;
    case 2: return gifPath.length() ? fit(baseName(gifPath), 16) : String("NONE");
    case 3: return gifTint ? String(PAL[gifTint - 1].name) : String("ORIGINAL");
    case 4: snprintf(b, sizeof(b), "%d%%", gifBr); return b;
    case 5: return glassLvl == 0 ? "OFF" : (glassLvl == 1 ? "LIGHT" : "DARK");
    case 6: snprintf(b, sizeof(b), "%d%%", backlight); return b;
    case 7: return h24 ? "24H" : "12H";
    case 8: return ssid.length() ? fit(ssid, 16) : String("-");
    case 9: return wpass.length() ? String("******") : String("-");
    case 10: return autoTz ? "ON" : "OFF";
    case 11: { int a = abs(tzMin); snprintf(b, sizeof(b), "%c%02d:%02d", tzMin < 0 ? '-' : '+', a / 60, a % 60); return b; }
    case 12: return fit(syncMsg, 14);
    case 13: return "ENTER";
  }
  return "";
}
void setAdjust(int i, int dir) {
  switch (i) {
    case 0: dialColor = (dialColor + dir + NPAL) % NPAL; updateColors(); break;
    case 1: dialBr = constrain((int)dialBr + 10 * dir, 10, 100); updateColors(); break;
    case 3: gifTint = (gifTint + dir + NPAL + 1) % (NPAL + 1); rebuildPal(); break;
    case 4: gifBr = constrain((int)gifBr + 10 * dir, 10, 100); rebuildPal(); break;
    case 5: glassLvl = (glassLvl + dir + 3) % 3; rebuildPal(); break;
    case 6: backlight = constrain((int)backlight + 10 * dir, 10, 100); applyBacklight(); break;
    case 7: h24 = !h24; break;
    case 10: autoTz = !autoTz; break;
    case 11: tzMin = constrain(tzMin + 30 * dir, -720, 840); autoTz = false; break;
    default: return;
  }
  markCfg();
}
void startText(uint8_t target) {
  inTarget = target; inBuf = target == 0 ? ssid : (target == 1 ? wpass : String(""));
  mode = M_TEXT;
}
void setEnter(int i) {
  if (i == 2) { scanGifs(); pickCur = 0; for (size_t k = 0; k < gifList.size(); k++) if (gifList[k] == gifPath) pickCur = k + 1; mode = M_PICK; }
  else if (i == 8) startText(0);
  else if (i == 9) startText(1);
  else if (i == 12) startSync();
  else if (i == 13) startText(2);
  else setAdjust(i, 1);
}
void drawSettings() {
  txt("SETTINGS", 10, 3, cOn, &fonts::Font2);
  int top = constrain(sCur - 2, 0, NSET - 6);
  for (int r = 0; r < 6; r++) {
    int i = top + r, y = 22 + r * 17;
    if (i == sCur) S.fillRect(6, y - 1, 228, 17, cSel);
    txt(setLabel(i), 12, y, i == sCur ? cOn : cMid, &fonts::Font2);
    txt(setVal(i), 230, y, cOn, &fonts::Font2, D_TR);
  }
  txt(";. SELECT   ,/ CHANGE   ENTER=ACTION", 120, 124, cMid, &fonts::Font0, D_TC);
}
void drawPicker() {
  char b[32]; snprintf(b, sizeof(b), "SELECT GIF (%d)", (int)gifList.size());
  txt(b, 10, 3, cOn, &fonts::Font2);
  int n = gifList.size() + 1;
  int top = constrain(pickCur - 2, 0, max(0, n - 6));
  for (int r = 0; r < 6 && top + r < n; r++) {
    int i = top + r, y = 22 + r * 17;
    if (i == pickCur) S.fillRect(6, y - 1, 228, 17, cSel);
    String nm = i == 0 ? String("NONE (no GIF)") : fit(baseName(gifList[i - 1]), 20);
    txt(nm, 12, y, i == pickCur ? cOn : cMid, &fonts::Font2);
    if (i > 0) txt(gifCached[i - 1] ? "READY" : "NEW", 230, y + 4, i == pickCur ? cOn : cMid, &fonts::Font0, D_TR);
  }
  if (!sdOK) txt("NO SD CARD", 120, 70, cOn, &fonts::Font2, D_MC);
  else if (gifList.empty()) txt("PUT .GIF FILES INTO /gifs", 120, 124, cMid, &fonts::Font0, D_TC);
  else txt(";. SELECT  ENTER=USE  R=REBUILD  DEL=BACK", 120, 124, cMid, &fonts::Font0, D_TC);
}
void drawTextInput() {
  const char* ti[3] = {"WIFI SSID", "WIFI PASSWORD", "DDMMYYYYHHMM"};
  txt(ti[inTarget], 120, 20, cOn, &fonts::Font2, D_TC);
  S.drawRect(10, 50, 220, 30, cMid);
  String shown = inBuf; if (shown.length() > 22) shown = shown.substring(shown.length() - 22);
  if ((millis() / 500) % 2) shown += "_";
  txt(shown, 16, 58, cOn, &fonts::Font2);
  if (inTarget == 2) txt("e.g. 221120261530 = 22 NOV 2026 15:30", 120, 90, cMid, &fonts::Font0, D_TC);
  txt("ENTER=OK   TAB=CANCEL   DEL=ERASE", 120, 118, cMid, &fonts::Font0, D_TC);
}
void drawRing() {
  bool fl = ((millis() - ringT0) / 400) % 2;
  S.fillSprite(fl ? cOn : (uint16_t)TFT_BLACK);
  uint16_t fg = fl ? (uint16_t)TFT_BLACK : cOn;
  if (ringKind == 2) {
    char b[24]; snprintf(b, sizeof(b), "%02d:%02d", al[ringIdx].h, al[ringIdx].m);
    txt("ALARM", 120, 38, fg, &fonts::Font4, D_MC, 2);
    txt(b, 120, 84, fg, &fonts::Font4, D_MC, 1.5);
    txt("ENTER=SNOOZE 5 MIN   ANY KEY=STOP", 120, 118, fg, &fonts::Font0, D_TC);
  } else {
    txt("TIME UP", 120, 50, fg, &fonts::Font4, D_MC, 2);
    txt("ANY KEY=STOP", 120, 105, fg, &fonts::Font2, D_TC);
  }
}

// ------------------------------------------------------------------ input
void goScreen(uint8_t n) {
  if (screen == SC_SET && cfgDirty) saveCfg();
  screen = n % SC_COUNT;
  if (screen == SC_CAL) { struct tm t; getLocal(t); viewY = t.tm_year + 1900; viewM = t.tm_mon; }
  alBuf = "";
}
void onKeys(Keyboard_Class::KeysState& st) {
  std::vector<char> ks(st.word.begin(), st.word.end());
  if (st.space && std::find(ks.begin(), ks.end(), ' ') == ks.end()) ks.push_back(' ');

  if (ringKind) { stopRing(ringKind == 2 && (st.enter || st.space)); return; }

  if (mode == M_TEXT) {
    if (st.tab) { mode = M_NORMAL; return; }
    if (st.enter) {
      if (inTarget == 0) { ssid = inBuf; markCfg(); nextSyncAt = millis(); }
      else if (inTarget == 1) { wpass = inBuf; markCfg(); }
      else if (inBuf.length() == 12) {
        int d = inBuf.substring(0, 2).toInt(), mo = inBuf.substring(2, 4).toInt(), y = inBuf.substring(4, 8).toInt();
        int hh = inBuf.substring(8, 10).toInt(), mi = inBuf.substring(10, 12).toInt();
        if (setLocalTime(y, mo, d, hh, mi)) saveCfg(); else syncMsg = "BAD DATE";
      }
      mode = M_NORMAL; return;
    }
    if (st.del) { if (inBuf.length()) inBuf.remove(inBuf.length() - 1); return; }
    for (char c : ks) {
      if (inTarget == 2) { if (c >= '0' && c <= '9' && inBuf.length() < 12) inBuf += c; }
      else if (c >= 32 && c < 127 && inBuf.length() < 63) inBuf += c;
    }
    return;
  }
  if (mode == M_PICK) {
    int n = gifList.size() + 1;
    if (st.tab || st.del) { mode = M_NORMAL; return; }
    if (st.enter) {
      mode = M_NORMAL;
      gifPath = pickCur == 0 ? String("") : gifList[pickCur - 1];
      applyGif(); markCfg(); return;
    }
    for (char c : ks) {
      if (c == ';') pickCur = (pickCur + n - 1) % n;
      else if (c == '.') pickCur = (pickCur + 1) % n;
      else if (c == '`') mode = M_NORMAL;
      else if ((c == 'r' || c == 'R') && pickCur > 0) {
        SD.remove(cachePath(gifList[pickCur - 1]).c_str()); gifCached[pickCur - 1] = 0;
      }
    }
    return;
  }

  if (st.tab) { goScreen(screen + 1); return; }
  for (char c : ks) if (c == '`') { goScreen(SC_CLOCK); return; }

  switch (screen) {
    case SC_CLOCK:
      for (char c : ks) {
        if (c == ',') { setAdjust(0, -1); } else if (c == '/') { setAdjust(0, 1); }
        else if (c == ';') { setAdjust(1, 1); } else if (c == '.') { setAdjust(1, -1); }
        else if (c == 'c') { setAdjust(3, 1); }
        else if (c == 'v') { setAdjust(4, -1); } else if (c == 'b') { setAdjust(4, 1); }
        else if (c == 's') { startSync(); }
        else if (c == 'g') {
          scanGifs();
          if (!gifList.empty()) {
            int cur = -1; for (size_t k = 0; k < gifList.size(); k++) if (gifList[k] == gifPath) cur = k;
            gifPath = gifList[(cur + 1) % gifList.size()]; applyGif(); markCfg();
          }
        }
      }
      break;
    case SC_CAL:
      for (char c : ks) {
        if (c == ',') { if (--viewM < 0) { viewM = 11; viewY--; } }
        else if (c == '/') { if (++viewM > 11) { viewM = 0; viewY++; } }
        else if (c == ';') viewY--; else if (c == '.') viewY++;
        else if (c == 't') { struct tm t; getLocal(t); viewY = t.tm_year + 1900; viewM = t.tm_mon; }
      }
      viewY = constrain(viewY, 1970, 2099);
      break;
    case SC_TIMER:
      for (char c : ks) if (c >= '0' && c <= '9' && tmState == 0) tmDigits = (tmDigits * 10 + (c - '0')) % 1000000UL;
      if (st.del) {
        if (tmState == 0) tmDigits /= 10; else tmState = 0;
      }
      if (st.enter) {
        if (tmState == 0) {
          uint32_t tot = (tmDigits / 10000) * 3600UL + ((tmDigits / 100) % 100) * 60UL + tmDigits % 100;
          if (tot) { tmEnd = millis() + tot * 1000UL; tmState = 1; }
        } else if (tmState == 1) { tmRem = (int32_t)(tmEnd - millis()); tmState = 2; }
        else if (tmState == 2) { tmEnd = millis() + tmRem; tmState = 1; }
      }
      break;
    case SC_ALARM: {
      for (char c : ks) {
        if (c == ';') { alCur = (alCur + 3) % 4; alBuf = ""; }
        else if (c == '.') { alCur = (alCur + 1) % 4; alBuf = ""; }
        else if (c == ',') { al[alCur].rep = (al[alCur].rep + 3) % 4; markCfg(); }
        else if (c == '/') { al[alCur].rep = (al[alCur].rep + 1) % 4; markCfg(); }
        else if (c >= '0' && c <= '9' && alBuf.length() < 4) alBuf += c;
      }
      if (st.del && alBuf.length()) alBuf.remove(alBuf.length() - 1);
      if (st.enter) {
        if (alBuf.length() == 4) {
          int hh = alBuf.substring(0, 2).toInt(), mm = alBuf.substring(2, 4).toInt();
          if (hh < 24 && mm < 60) { al[alCur].h = hh; al[alCur].m = mm; al[alCur].on = true; markCfg(); }
          alBuf = "";
        } else if (alBuf.isEmpty()) { al[alCur].on = !al[alCur].on; markCfg(); }
      }
      break;
    }
    case SC_STOPW:
      if (st.enter || st.space) {
        if (swRun) { swAcc += millis() - swStart; swRun = false; } else { swStart = millis(); swRun = true; }
      }
      for (char c : ks) if ((c == 'l' || c == 'L') && swRun && lapN < 20) laps[lapN++] = swElapsed();
      if (st.del && !swRun) { swAcc = 0; lapN = 0; }
      break;
    case SC_SET:
      for (char c : ks) {
        if (c == ';') sCur = (sCur + NSET - 1) % NSET;
        else if (c == '.') sCur = (sCur + 1) % NSET;
        else if (c == ',') setAdjust(sCur, -1);
        else if (c == '/') setAdjust(sCur, 1);
      }
      if (st.enter) setEnter(sCur);
      break;
  }
}

// ------------------------------------------------------------------ main
void render() {
  lastRender = millis(); dirty = false;
  if (ringKind) { drawRing(); S.pushSprite(0, 0); return; }
  composeBg();
  if (mode == M_PICK) drawPicker();
  else if (mode == M_TEXT) drawTextInput();
  else switch (screen) {
    case SC_CLOCK: drawClock(); break;
    case SC_CAL: drawCalendar(); break;
    case SC_TIMER: drawTimer(); break;
    case SC_ALARM: drawAlarm(); break;
    case SC_STOPW: drawStopw(); break;
    case SC_SET: drawSettings(); break;
  }
  S.pushSprite(0, 0);
}

void setup() {
  auto cfgM = M5.config();
  M5Cardputer.begin(cfgM, true);
  M5Cardputer.Speaker.setVolume(200);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOK = SD.begin(SD_CS, SPI, 25000000);
  S.setColorDepth(16);
  if (!S.createSprite(SCR_W, SCR_H)) { M5Cardputer.Display.println("NO MEMORY"); while (true) delay(1000); }
  loadCfg();
  applyBacklight(); updateColors();
  gif.begin(LITTLE_ENDIAN_PIXELS);
  if (gifPath.length()) applyGif();      // instant when the cache exists; converts once otherwise
  struct tm t; getLocal(t);
  viewY = t.tm_year + 1900; viewM = t.tm_mon;
  nextSyncAt = millis() + 2000;
  lastAlarmKey = (uint32_t)((time(nullptr) + (time_t)tzMin * 60) / 60);
  bgRetryAt = millis() + 30000;
}

void loop() {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();
    onKeys(st);
    dirty = true;
  }
  uint32_t now = millis();
  tickRing(now); tickTimer(now); tickAlarms(); tickSync(now);
  if (cfgDirty && now - cfgDirtyT > 3000) saveCfg();
  if (now - battT > 30000 || battT == 0) { battLvl = M5Cardputer.Power.getBatteryLevel(); battT = now; }
  bgTick();                                           // sets dirty only when the frame index changed
  uint32_t period = ringKind ? 80 : ((screen == SC_STOPW && swRun) ? 33 : 250);
  if (now - lastRender >= period) dirty = true;
  if (dirty) render();
}
