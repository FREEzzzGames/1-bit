#include <Arduino.h>
/*
 * DRUM MACHINE + LOOP STATION for M5Stack Cardputer ADV
 *
 * Board:     M5Stack -> M5Cardputer (ESP32-S3)
 * Libraries: M5Cardputer, M5Unified, M5GFX (SD / SPI come with the ESP32 core)
 *
 * 16 synthesized drum pads, 8 patterns (1/2/4 bars of 16th notes), live
 * recording with overdub and undo, count-in, metronome, swing, tap tempo,
 * song chain, tracks saved to SD (/DrumMachine/*.dmt) with catalog + favorites.
 *
 * KEYS (main screen)
 *   Pads, top row   Q W E R T Y U I  = 808 RIM SNR2 PDL TOM2 CLV SHK ZAP
 *   Pads, home row  A S D F G H J K  = KICK SNR CLAP CHH OHH TOM COW CRSH
 *   Shift+pad mute lane   Ctrl+pad solo lane   Alt+pad erase lane
 *   Z play/stop   X rec/overdub   C undo/redo   V clear pattern (press twice)
 *   B bars 1/2/4  N metronome  M count-in  SPACE tap tempo  DEL stop+rewind
 *   1-8 pattern (Opt+1-8 copies the pattern to that slot)   9 reset mutes
 *   , /  BPM -1/+1 (Shift: 10)    - =  swing    ; .  volume
 *   L add pattern to chain   O remove last   Shift+O clear chain   P song mode
 *   ENTER save track   TAB catalog   ` menu
 * CATALOG
 *   ; . pick   ENTER play   E edit   F favorite   R rename   DEL delete (2x)
 *   , / switch ALL/FAV   Z stop   TAB or ` back
 */
#include <M5Cardputer.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <math.h>
#include <ctype.h>
#include <string.h>

static const int SW = 240, SH = 135;
static const int SR = 16000;           // sample rate of the drum samples

M5Canvas cv(&M5Cardputer.Display);
Preferences prefs;

// ============================ SETTINGS =============================
static const uint8_t PAL[6][3] = {
  {60, 255, 90}, {255, 176, 0}, {235, 235, 235}, {0, 225, 255}, {255, 70, 70}, {255, 90, 255}
};
static const char* PALN[6] = {"GREEN", "AMBER", "WHITE", "CYAN", "RED", "PINK"};
uint16_t FG, DIM, DIM2, BG = 0;
int colorIdx = 0, vol = 8;
bool metroOn = true, countOn = true;
volatile bool songMode = false;

static void applyColor() {
  const uint8_t* c = PAL[colorIdx];
  auto& d = M5Cardputer.Display;
  FG = d.color565(c[0], c[1], c[2]);
  DIM = d.color565(c[0] * 0.40f, c[1] * 0.40f, c[2] * 0.40f);
  DIM2 = d.color565(c[0] * 0.15f, c[1] * 0.15f, c[2] * 0.15f);
}
static void applyVolume() { M5Cardputer.Speaker.setVolume(vol * 25); }
static void saveSettings() {
  prefs.putUChar("col", colorIdx);
  prefs.putUChar("vol", vol);
  prefs.putBool("met", metroOn);
  prefs.putBool("cnt", countOn);
}

// ============================== TOAST ==============================
static char toastBuf[32];
static uint32_t toastUntil = 0;
static void toast(const char* s) {
  strncpy(toastBuf, s, 31); toastBuf[31] = 0;
  toastUntil = millis() + 1400;
}

// ============================== INPUT ==============================
struct Ev { char c, lc; bool shift, ctrl, alt, opt, fn; };
static Ev evq[10];
static int nev = 0;
static bool evEnter = false, evDel = false, evTab = false;
static std::vector<char> prevWord;
static bool pEnter = false, pDel = false, pTab = false;

static void pollKeys() {
  nev = 0; evEnter = evDel = evTab = false;
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange()) return;
  auto st = M5Cardputer.Keyboard.keysState();
  for (char c : st.word) {
    bool was = false;
    for (char p : prevWord) if (p == c) was = true;
    if (!was && nev < 10) {
      Ev& e = evq[nev++];
      e.c = c;
      e.lc = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
      e.shift = st.shift || (c >= 'A' && c <= 'Z');
      e.ctrl = st.ctrl; e.alt = st.alt; e.opt = st.opt; e.fn = st.fn;
    }
  }
  if (st.enter && !pEnter) evEnter = true;
  if (st.del && !pDel) evDel = true;
  if (st.tab && !pTab) evTab = true;
  prevWord = st.word;
  pEnter = st.enter; pDel = st.del; pTab = st.tab;
}

// ============================== SAMPLES ============================
static const char PADKEY[16] = {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'};
static const char* PADNAME[16] = {"KICK", "SNR", "CLAP", "CHH", "OHH", "TOM", "COW", "CRSH",
                                  "808", "RIM", "SNR2", "PDL", "TOM2", "CLV", "SHK", "ZAP"};
// audio channel per pad (same channel = choke group)
static const uint8_t PADCH[16] = {0, 1, 2, 3, 3, 4, 5, 6, 0, 1, 1, 3, 4, 2, 2, 5};

struct Smp { int8_t* d; int n; };
static Smp smp[16], clickHi, clickLo;

static uint32_t rngS = 0x1234567;
static float frand() {
  rngS ^= rngS << 13; rngS ^= rngS >> 17; rngS ^= rngS << 5;
  return (rngS & 0xFFFF) / 32768.f - 1.f;
}
static int8_t q8(float v) {
  if (v > 127) v = 127;
  if (v < -127) v = -127;
  return (int8_t)v;
}
static bool mk(Smp& s, int n) {
  s.d = (int8_t*)malloc(n);
  s.n = s.d ? n : 0;
  return s.d != nullptr;
}
// tone (sine with pitch sweep) + noise (optionally high-passed)
static void synth(Smp& s, int n, float f0, float f1, float sw, float dec, float amp,
                  float nz, float nzdec, bool hp) {
  if (!mk(s, n)) return;
  float ph = 0, prevN = 0;
  for (int i = 0; i < n; i++) {
    float t = i / (float)SR;
    float f = f1 + (f0 - f1) * expf(-t * sw);
    ph += 6.2831853f * f / SR;
    float tone = (amp > 0) ? sinf(ph) * expf(-t * dec) * amp : 0;
    float nv = frand();
    float ns = hp ? (nv - prevN) * 0.6f : nv;
    prevN = nv;
    float noise = ns * expf(-t * nzdec) * nz;
    s.d[i] = q8(110.f * (tone + noise));
  }
}
static void synthAll() {
  synth(smp[0], 4500, 150, 45, 35, 8, 1.0f, 0.25f, 220, false);      // KICK
  synth(smp[1], 3200, 190, 170, 10, 16, 0.6f, 0.8f, 16, true);        // SNARE
  if (mk(smp[2], 3200)) {                                             // CLAP
    for (int i = 0; i < 3200; i++) {
      float t = i / (float)SR, env = 0;
      for (int b = 0; b < 3; b++) { float tb = t - b * 0.012f; if (tb >= 0) env += expf(-tb * 140.f) * 0.5f; }
      if (t > 0.036f) env += expf(-(t - 0.036f) * 20.f) * 0.5f;
      smp[2].d[i] = q8(110.f * frand() * env * 0.9f);
    }
  }
  synth(smp[3], 900, 0, 0, 0, 0, 0, 0.9f, 60, true);                  // CLOSED HAT
  synth(smp[4], 5000, 0, 0, 0, 0, 0, 0.9f, 9, true);                  // OPEN HAT
  synth(smp[5], 4200, 130, 85, 12, 7, 1.0f, 0.1f, 100, false);        // TOM LOW
  if (mk(smp[6], 3600)) {                                             // COWBELL
    float p1 = 0, p2 = 0;
    for (int i = 0; i < 3600; i++) {
      float t = i / (float)SR;
      p1 += 6.2831853f * 560.f / SR; p2 += 6.2831853f * 845.f / SR;
      float sq = ((sinf(p1) > 0) ? 1.f : -1.f) * 0.5f + ((sinf(p2) > 0) ? 1.f : -1.f) * 0.5f;
      smp[6].d[i] = q8(70.f * sq * expf(-t * 14.f));
    }
  }
  synth(smp[7], 8000, 0, 0, 0, 0, 0, 0.9f, 5.5f, true);               // CRASH
  synth(smp[8], 7000, 90, 48, 12, 5, 1.0f, 0, 0, false);              // 808
  synth(smp[9], 800, 1800, 1700, 0, 70, 0.7f, 0.6f, 120, true);       // RIM
  synth(smp[10], 2400, 230, 200, 8, 25, 0.5f, 0.9f, 30, true);        // SNARE 2
  synth(smp[11], 500, 0, 0, 0, 0, 0, 0.9f, 120, true);                // PEDAL HAT
  synth(smp[12], 3200, 260, 170, 15, 9, 1.0f, 0.08f, 100, false);     // TOM HIGH
  synth(smp[13], 960, 2500, 2400, 0, 45, 1.0f, 0, 0, false);          // CLAVE
  if (mk(smp[14], 1500)) {                                            // SHAKER
    float prevN = 0;
    for (int i = 0; i < 1500; i++) {
      float t = i / (float)SR;
      float env = (t < 0.012f) ? t / 0.012f : expf(-(t - 0.012f) * 35.f);
      float nv = frand();
      smp[14].d[i] = q8(110.f * (nv - prevN) * 0.6f * env);
      prevN = nv;
    }
  }
  synth(smp[15], 4000, 1800, 200, 12, 6, 1.0f, 0, 0, false);          // ZAP
  synth(clickHi, 320, 1600, 1600, 0, 120, 1.0f, 0, 0, false);
  synth(clickLo, 320, 1000, 1000, 0, 120, 1.0f, 0, 0, false);
}
static void playPad(int p) {
  if (p < 0 || p > 15 || smp[p].n == 0) return;
  M5Cardputer.Speaker.playRaw(smp[p].d, smp[p].n, SR, false, 1, PADCH[p], true);
}
static void playClick(bool hi) {
  Smp& s = hi ? clickHi : clickLo;
  if (s.n) M5Cardputer.Speaker.playRaw(s.d, s.n, SR, false, 1, 7, true);
}

// ============================== PROJECT ============================
struct Project {
  char magic[4];
  char name[20];
  uint8_t fav;
  uint8_t swing;           // 0..60
  uint16_t bpm;
  uint8_t bars[8];         // 1, 2 or 4 per pattern
  uint8_t chainLen;
  uint8_t chain[32];
  uint16_t muteMask;
  uint16_t pat[8][64];     // one bit per pad, per 16th step
  uint8_t spare[8];
};
static Project P, tmpP;

static void initProject(Project& p) {
  memset(&p, 0, sizeof(p));
  memcpy(p.magic, "DMT1", 4);
  p.bpm = 100;
  for (int i = 0; i < 8; i++) p.bars[i] = 1;
}
static void sanitize(Project& p) {
  p.name[19] = 0;
  if (p.bpm < 40 || p.bpm > 240) p.bpm = 100;
  if (p.swing > 60) p.swing = 0;
  for (int i = 0; i < 8; i++) if (p.bars[i] != 1 && p.bars[i] != 2 && p.bars[i] != 4) p.bars[i] = 1;
  if (p.chainLen > 32) p.chainLen = 32;
  for (int i = 0; i < 32; i++) if (p.chain[i] > 7) p.chain[i] = 0;
}

// ============================== SEQUENCER ==========================
enum { T_STOP = 0, T_COUNT = 1, T_PLAY = 2 };
static volatile uint8_t tState = T_STOP;
static volatile int gStep = 0, gPlayStep = -1, gCnt = 0, curPat = 0, nextPat = -1;
static volatile int chainPos = 0, gSkipStep = -1, solo = -1;
static volatile uint16_t gSkipMask = 0;
static volatile uint32_t stepStartUs = 0, stepDurUs = 125000;
static volatile bool restartReq = false;
static bool rec = false;
static uint32_t padFlash[16];

static void doStep(uint32_t startUs) {
  stepStartUs = startUs;
  if (tState == T_COUNT) {
    gPlayStep = -1;
    if (gCnt % 4 == 0) playClick(gCnt == 0);
    gCnt++;
    if (gCnt >= 16) { tState = T_PLAY; gStep = 0; }
    return;
  }
  int p = curPat;
  int len = P.bars[p] * 16;
  int s = gStep;
  if (s >= len) s = 0;
  gPlayStep = s;
  if (metroOn && (s % 4) == 0) playClick(s == 0);
  uint16_t m = P.pat[p][s] & ~P.muteMask;
  if (solo >= 0) m &= (uint16_t)(1 << solo);
  if (gSkipStep == s) { m &= ~gSkipMask; gSkipMask = 0; gSkipStep = -1; }
  for (int i = 0; i < 16; i++) if (m & (1 << i)) playPad(i);
  s++;
  if (s >= len) {
    s = 0;
    if (songMode && P.chainLen > 0) { chainPos = (chainPos + 1) % P.chainLen; curPat = P.chain[chainPos]; }
    else if (nextPat >= 0) { curPat = nextPat; nextPat = -1; }
  }
  gStep = s;
}

static void seqTask(void*) {
  uint32_t nextUs = 0;
  for (;;) {
    if (tState == T_STOP) { vTaskDelay(2); continue; }
    if (restartReq) { nextUs = micros(); restartReq = false; }
    uint32_t now = micros();
    if ((int32_t)(now - nextUs) >= 0) {
      uint32_t base = 15000000UL / (uint32_t)P.bpm;
      float sw = P.swing / 200.f;
      uint32_t dur = base;
      if (tState == T_PLAY) dur = (uint32_t)(base * ((gStep % 2 == 0) ? (1.f + sw) : (1.f - sw)));
      stepDurUs = dur;
      doStep(nextUs);
      nextUs += dur;
    }
    vTaskDelay(1);
  }
}

static void startPlay(bool withCount) {
  chainPos = 0;
  if (songMode && P.chainLen > 0) curPat = P.chain[0];
  nextPat = -1; gStep = 0; gCnt = 0; gPlayStep = -1; gSkipStep = -1; gSkipMask = 0;
  restartReq = true;
  tState = (withCount && countOn) ? T_COUNT : T_PLAY;
}
static void stopPlay() {
  tState = T_STOP; rec = false; gPlayStep = -1;
  M5Cardputer.Speaker.stop();
}

// ---- undo / edit helpers
static uint16_t undoBuf[64];
static int undoPatIdx = -1;
static void snapUndo() { memcpy(undoBuf, P.pat[curPat], 128); undoPatIdx = curPat; }
static void undoRedo() {
  if (undoPatIdx < 0) { toast("NOTHING TO UNDO"); return; }
  uint16_t tmp[64];
  memcpy(tmp, P.pat[undoPatIdx], 128);
  memcpy(P.pat[undoPatIdx], undoBuf, 128);
  memcpy(undoBuf, tmp, 128);
  toast("UNDO / REDO");
}
static uint32_t clearConfirmUntil = 0;
static void clearPattern() {
  if (millis() < clearConfirmUntil) {
    snapUndo(); memset(P.pat[curPat], 0, 128);
    clearConfirmUntil = 0; toast("PATTERN CLEARED");
  } else { clearConfirmUntil = millis() + 1500; toast("PRESS V AGAIN TO CLEAR"); }
}
static void eraseLane(int pad) {
  snapUndo();
  for (int s = 0; s < 64; s++) P.pat[curPat][s] &= ~(1 << pad);
  char b[32]; snprintf(b, sizeof(b), "ERASED %s", PADNAME[pad]); toast(b);
}
static void cycleBars() {
  int b = P.bars[curPat], nb = (b == 1) ? 2 : (b == 2 ? 4 : 1);
  if (nb > b) {
    bool empty = true;
    for (int s = b * 16; s < nb * 16; s++) if (P.pat[curPat][s]) empty = false;
    if (empty) for (int s = b * 16; s < nb * 16; s++) P.pat[curPat][s] = P.pat[curPat][s % (b * 16)];
  }
  P.bars[curPat] = nb;
  char t[24]; snprintf(t, sizeof(t), "PATTERN = %d BAR", nb); toast(t);
}
static void selectPattern(int idx, bool copy) {
  if (copy) {
    if (idx != curPat) { memcpy(P.pat[idx], P.pat[curPat], 128); P.bars[idx] = P.bars[curPat]; }
    char t[24]; snprintf(t, sizeof(t), "COPIED TO %d", idx + 1); toast(t);
    return;
  }
  songMode = false;
  if (tState == T_STOP) curPat = idx;
  else nextPat = (idx == curPat) ? -1 : idx;
}
static void tapTempo() {
  static uint32_t taps[4];
  static int nt = 0;
  uint32_t now = millis();
  if (nt > 0 && now - taps[nt - 1] > 2000) nt = 0;
  if (nt == 4) { for (int i = 0; i < 3; i++) taps[i] = taps[i + 1]; nt = 3; }
  taps[nt++] = now;
  if (nt >= 2) {
    uint32_t avg = (taps[nt - 1] - taps[0]) / (nt - 1);
    if (avg > 0) {
      int b = (int)(60000UL / avg);
      if (b < 40) b = 40;
      if (b > 240) b = 240;
      P.bpm = b;
    }
  }
}
static void recordHit(int pad) {
  if (!rec || tState != T_PLAY) return;
  int s = gPlayStep;
  if (s < 0) return;
  uint32_t now = micros();
  float fr = (float)(now - stepStartUs) / (float)stepDurUs;
  int len = P.bars[curPat] * 16;
  int t = (fr >= 0.5f) ? (s + 1) % len : s;
  P.pat[curPat][t] |= (uint16_t)(1 << pad);
  if (t != s) { gSkipMask |= (uint16_t)(1 << pad); gSkipStep = t; }
}
static void hitPad(int pad) {
  playPad(pad);
  padFlash[pad] = millis();
  recordHit(pad);
}
static void toggleRec() {
  if (rec) { rec = false; return; }
  snapUndo();
  rec = true;
  if (tState == T_STOP) startPlay(true);
}
static void togglePlay() {
  if (tState == T_STOP) startPlay(false);
  else stopPlay();
}

// ============================== SD / CATALOG =======================
static const int CSCK = 40, CMISO = 39, CMOSI = 14, CCS = 12;
static const char* DIRP = "/DrumMachine";
static bool sdOk = false;
static char curFile[32] = "";

struct CatEntry { char file[28]; char name[20]; uint16_t bpm; uint8_t fav, nPat, chainLen; };
static CatEntry cat[64];
static int catN = 0, catSel = 0, catTop = 0;
static bool favOnly = false;
static int viewIdx[64], viewN = 0;

static void pathOf(const char* file, char* out, size_t n) { snprintf(out, n, "%s/%s", DIRP, file); }

static void makeFile(const char* name, char* out) {   // out >= 28 bytes
  int n = 0;
  for (const char* p = name; *p && n < 16; p++) {
    char c = *p;
    out[n++] = (isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
  }
  if (n == 0) { strcpy(out, "TRACK"); n = 5; }
  strcpy(out + n, ".dmt");
}
static bool fileExists(const char* file) {
  char path[64]; pathOf(file, path, sizeof(path));
  return SD.exists(path);
}
static bool writeProject(const Project& pr, const char* file) {
  if (!sdOk) return false;
  SD.mkdir(DIRP);
  char path[64]; pathOf(file, path, sizeof(path));
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  size_t w = f.write((const uint8_t*)&pr, sizeof(Project));
  f.close();
  return w == sizeof(Project);
}
static bool readProject(const char* file, Project& out) {
  if (!sdOk) return false;
  char path[64]; pathOf(file, path, sizeof(path));
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t r = f.read((uint8_t*)&out, sizeof(Project));
  f.close();
  if (r != sizeof(Project) || memcmp(out.magic, "DMT1", 4) != 0) return false;
  sanitize(out);
  return true;
}
static void rebuildView(const char* keep) {
  viewN = 0;
  for (int i = 0; i < catN; i++) if (!favOnly || cat[i].fav) viewIdx[viewN++] = i;
  catSel = 0;
  if (keep) for (int i = 0; i < viewN; i++) if (strcmp(cat[viewIdx[i]].file, keep) == 0) catSel = i;
  if (catSel >= viewN) catSel = viewN > 0 ? viewN - 1 : 0;
  catTop = 0;
}
static void scanCatalog(const char* keep) {
  catN = 0;
  if (sdOk) {
    SD.mkdir(DIRP);
    File dir = SD.open(DIRP);
    if (dir) {
      while (catN < 64) {
        File f = dir.openNextFile();
        if (!f) break;
        const char* nm = f.name();
        const char* sl = strrchr(nm, '/');
        if (sl) nm = sl + 1;
        size_t L = strlen(nm);
        if (!f.isDirectory() && L > 4 && strcasecmp(nm + L - 4, ".dmt") == 0 && L < 28 &&
            f.size() >= sizeof(Project)) {
          size_t r = f.read((uint8_t*)&tmpP, sizeof(Project));
          if (r == sizeof(Project) && memcmp(tmpP.magic, "DMT1", 4) == 0) {
            sanitize(tmpP);
            CatEntry& e = cat[catN++];
            strncpy(e.file, nm, 27); e.file[27] = 0;
            strncpy(e.name, tmpP.name, 19); e.name[19] = 0;
            e.bpm = tmpP.bpm; e.fav = tmpP.fav ? 1 : 0; e.chainLen = tmpP.chainLen;
            e.nPat = 0;
            for (int p = 0; p < 8; p++) {
              bool any = false;
              for (int s = 0; s < 64 && !any; s++) if (tmpP.pat[p][s]) any = true;
              if (any) e.nPat++;
            }
          }
        }
        f.close();
      }
      dir.close();
    }
  }
  // sort: favorites first, then by name
  for (int i = 1; i < catN; i++) {
    CatEntry x = cat[i];
    int j = i - 1;
    while (j >= 0 && ((cat[j].fav < x.fav) || (cat[j].fav == x.fav && strcasecmp(cat[j].name, x.name) > 0))) {
      cat[j + 1] = cat[j]; j--;
    }
    cat[j + 1] = x;
  }
  rebuildView(keep);
}
static bool loadProject(const char* file) {
  if (!readProject(file, tmpP)) return false;
  stopPlay();
  memcpy(&P, &tmpP, sizeof(Project));
  strncpy(curFile, file, 31); curFile[31] = 0;
  curPat = 0; nextPat = -1; songMode = false; undoPatIdx = -1; solo = -1;
  return true;
}
static void defaultName(char* out) {
  char file[28];
  for (int n = 1; n < 1000; n++) {
    snprintf(out, 20, "TRACK%03d", n);
    makeFile(out, file);
    if (!fileExists(file)) return;
  }
}

// ============================== SCREENS ============================
enum Screen { SC_MAIN, SC_CAT, SC_MENU, SC_HELP, SC_NAME };
static Screen screen = SC_MAIN;
static int menuIdx = 0, helpPage = 0;
static bool confirmNew = false;
static uint32_t delConfirmUntil = 0;
enum { NAME_SAVE, NAME_RENAME };
static int nameMode = NAME_SAVE;
static char nameBuf[20];
static int nameLen = 0;
static char renameFile[28];

static void openName(int mode, const char* base) {
  nameMode = mode;
  strncpy(nameBuf, base, 16); nameBuf[16] = 0;
  nameLen = (int)strlen(nameBuf);
  screen = SC_NAME;
}
static void uniqueFile(char* file, const char* allowSame) {
  if (!fileExists(file) || (allowSame && strcmp(file, allowSame) == 0)) return;
  char base[28]; strncpy(base, file, 27); base[27] = 0;
  base[strlen(base) - 4] = 0;                 // strip ".dmt"
  if (strlen(base) > 12) base[12] = 0;
  for (int n = 2; n < 100; n++) {
    snprintf(file, 28, "%s_%d.dmt", base, n);
    if (!fileExists(file)) return;
  }
}
static void doSave() {
  if (!sdOk) { toast("NO SD CARD"); return; }
  if (nameLen == 0) defaultName(nameBuf);
  strncpy(P.name, nameBuf, 19); P.name[19] = 0;
  char file[28]; makeFile(P.name, file);
  uniqueFile(file, curFile);
  if (writeProject(P, file)) { strncpy(curFile, file, 31); curFile[31] = 0; toast("TRACK SAVED"); }
  else toast("SAVE FAILED");
}
static void doRename() {
  if (nameLen == 0) return;
  if (!readProject(renameFile, tmpP)) { toast("RENAME FAILED"); return; }
  strncpy(tmpP.name, nameBuf, 19); tmpP.name[19] = 0;
  char file[28]; makeFile(tmpP.name, file);
  uniqueFile(file, renameFile);
  if (writeProject(tmpP, file)) {
    if (strcmp(file, renameFile) != 0) { char old[64]; pathOf(renameFile, old, sizeof(old)); SD.remove(old); }
    if (strcmp(curFile, renameFile) == 0) { strncpy(curFile, file, 31); curFile[31] = 0; }
    scanCatalog(file);
    toast("RENAMED");
  } else toast("RENAME FAILED");
}
static void newProject() {
  stopPlay();
  initProject(P);
  curPat = 0; nextPat = -1; songMode = false; undoPatIdx = -1; solo = -1; curFile[0] = 0;
}

// ============================== DRAWING ============================
static void ctxt(const char* s, int cx, int y, int sz, uint16_t c) {
  cv.setTextSize(sz); cv.setTextColor(c);
  int w = cv.textWidth(s);
  cv.setCursor(cx - w / 2, y);
  cv.print(s);
}
static void ltxt(const char* s, int x, int y, int sz, uint16_t c) {
  cv.setTextSize(sz); cv.setTextColor(c);
  cv.setCursor(x, y);
  cv.print(s);
}
static int chip(int x, int y, const char* s, bool on) {
  int w = (int)strlen(s) * 6 + 4;
  if (on) { cv.fillRect(x, y - 1, w, 10, FG); ltxt(s, x + 2, y, 1, BG); }
  else ltxt(s, x + 2, y, 1, DIM);
  return w + 3;
}

static void drawMain() {
  cv.fillScreen(BG);
  uint32_t now = millis();
  int cp = curPat;
  // pattern selector
  for (int i = 0; i < 8; i++) {
    int x = 4 + i * 11;
    bool has = false;
    for (int s = 0; s < 64 && !has; s++) if (P.pat[i][s]) has = true;
    char d[2] = {(char)('1' + i), 0};
    if (i == cp) { cv.fillRect(x, 1, 10, 10, FG); ltxt(d, x + 2, 2, 1, BG); }
    else {
      bool q = (nextPat == i) && ((now / 250) & 1);
      cv.drawRect(x, 1, 10, 10, q ? FG : (has ? DIM : DIM2));
      ltxt(d, x + 2, 2, 1, has ? DIM : DIM2);
    }
  }
  char b[24];
  snprintf(b, sizeof(b), "%d BPM", (int)P.bpm);
  ltxt(b, 96, 3, 1, FG);
  snprintf(b, sizeof(b), "SW%d", (int)P.swing);
  ltxt(b, 142, 3, 1, DIM);
  snprintf(b, sizeof(b), "x%d", (int)P.bars[cp]);
  ltxt(b, 174, 3, 1, DIM);
  // transport state
  if (tState == T_STOP) ltxt("STOP", 204, 3, 1, DIM);
  else if (tState == T_COUNT) { cv.fillRect(200, 1, 38, 10, FG); ltxt("COUNT", 205, 3, 1, BG); }
  else if (rec) { if ((now / 300) & 1) cv.fillRect(200, 1, 38, 10, FG); ltxt("REC", 212, 3, 1, ((now / 300) & 1) ? BG : FG); }
  else ltxt("PLAY", 204, 3, 1, FG);

  // pads (rows mimic the keyboard rows)
  for (int i = 0; i < 16; i++) {
    int row = (i >= 8) ? 0 : 1, col = i % 8;
    int x = 4 + col * 29, y = 14 + row * 20;
    bool flash = (now - padFlash[i]) < 100;
    bool muted = (P.muteMask >> i) & 1;
    if (flash) cv.fillRect(x, y, 27, 18, FG);
    else cv.drawRect(x, y, 27, 18, muted ? DIM2 : DIM);
    ctxt(PADNAME[i], x + 13, y + 2, 1, flash ? BG : (muted ? DIM : FG));
    char kb[2] = {(char)toupper(PADKEY[i]), 0};
    ctxt(kb, x + 13, y + 10, 1, flash ? BG : DIM);
    if (solo == i) cv.fillRect(x + 1, y + 1, 3, 3, flash ? BG : FG);
    if (muted) cv.drawLine(x + 2, y + 15, x + 24, y + 3, DIM);
  }

  // step grid
  int len = P.bars[cp] * 16;
  int ps = (tState == T_PLAY && gPlayStep >= 0) ? gPlayStep : 0;
  int page = ps / 16;
  for (int c = 0; c < 16; c++) if (c % 4 == 0) cv.fillRect(16 + c * 13, 58, 13, 49, DIM2);
  for (int lane = 0; lane < 16; lane++) {
    int y = 58 + lane * 3;
    bool muted = (P.muteMask >> lane) & 1;
    for (int c = 0; c < 16; c++) {
      int s = page * 16 + c;
      if (s < len && ((P.pat[cp][s] >> lane) & 1)) cv.fillRect(17 + c * 13, y, 11, 2, muted ? DIM : FG);
    }
  }
  if (tState == T_PLAY && gPlayStep >= 0 && gPlayStep / 16 == page)
    cv.drawRect(16 + (gPlayStep % 16) * 13, 57, 13, 51, FG);
  for (int bi = 0; bi < P.bars[cp]; bi++) cv.fillRect(230, 60 + bi * 8, 5, 5, bi == page ? FG : DIM2);

  // footer
  int x = 2;
  x += chip(x, 112, "METRO", metroOn);
  x += chip(x, 112, "CNT", countOn);
  x += chip(x, 112, "SONG", songMode);
  snprintf(b, sizeof(b), "VOL %d", vol);
  ltxt(b, x + 4, 112, 1, DIM);
  if (!sdOk) ltxt("NO SD", SW - 34, 112, 1, DIM);
  if (now < toastUntil) ctxt(toastBuf, SW / 2, 123, 1, FG);
  else if (P.chainLen > 0) {
    ltxt("CH", 2, 123, 1, DIM);
    for (int i = 0; i < P.chainLen; i++) {
      char d[2] = {(char)('1' + P.chain[i]), 0};
      int cx = 16 + i * 7;
      if (songMode && tState != T_STOP && i == chainPos) { cv.fillRect(cx - 1, 122, 7, 10, FG); ltxt(d, cx, 123, 1, BG); }
      else ltxt(d, cx, 123, 1, FG);
    }
  } else ctxt("TAB catalog  ENTER save  ` menu", SW / 2, 123, 1, DIM);
}

static void drawCatalog() {
  cv.fillScreen(BG);
  ltxt("CATALOG", 4, 3, 1, FG);
  int x = 70;
  x += chip(x, 3, "ALL", !favOnly);
  x += chip(x, 3, "FAV", favOnly);
  char b[40];
  snprintf(b, sizeof(b), "%d/%d", viewN ? catSel + 1 : 0, viewN);
  cv.setTextSize(1);
  ltxt(b, SW - 4 - cv.textWidth(b), 3, 1, DIM);
  cv.drawFastHLine(0, 13, SW, DIM2);
  if (!sdOk) {
    ctxt("NO SD CARD", SW / 2, 50, 2, FG);
    ctxt("insert a card and restart", SW / 2, 74, 1, DIM);
  } else if (viewN == 0) {
    ctxt(favOnly ? "NO FAVORITES YET" : "NO TRACKS YET", SW / 2, 46, 1, FG);
    ctxt("record a loop, press ENTER to save", SW / 2, 62, 1, DIM);
  } else {
    if (catSel < catTop) catTop = catSel;
    if (catSel >= catTop + 8) catTop = catSel - 7;
    for (int r = 0; r < 8; r++) {
      int vi = catTop + r;
      if (vi >= viewN) break;
      CatEntry& e = cat[viewIdx[vi]];
      int y = 16 + r * 11;
      bool sel = (vi == catSel);
      if (sel) cv.fillRect(0, y - 1, SW, 11, FG);
      uint16_t tc = sel ? BG : FG, dc = sel ? BG : DIM;
      ltxt(e.fav ? "*" : " ", 4, y, 1, tc);
      ltxt(e.name, 14, y, 1, tc);
      snprintf(b, sizeof(b), "%d", (int)e.bpm);
      ltxt(b, 142, y, 1, dc);
      snprintf(b, sizeof(b), "P%d", (int)e.nPat);
      ltxt(b, 176, y, 1, dc);
      if (e.chainLen) { snprintf(b, sizeof(b), "C%d", (int)e.chainLen); ltxt(b, 204, y, 1, dc); }
    }
  }
  if (tState != T_STOP) {
    cv.fillRect(0, 104, SW, 10, FG);
    snprintf(b, sizeof(b), "> PLAYING  %s", P.name[0] ? P.name : "(unsaved)");
    ltxt(b, 4, 105, 1, BG);
  }
  ctxt("ENT play  E edit  F fav  R rename", SW / 2, 116, 1, DIM);
  ctxt("DEL del  Z stop  , / filter  TAB back", SW / 2, 125, 1, DIM);
  if (millis() < toastUntil) { cv.fillRect(40, 56, 160, 16, BG); cv.drawRect(40, 56, 160, 16, FG); ctxt(toastBuf, SW / 2, 60, 1, FG); }
}

static void drawNameDialog() {
  drawMain();
  cv.fillRect(20, 34, 200, 62, BG);
  cv.drawRect(20, 34, 200, 62, FG);
  ctxt(nameMode == NAME_SAVE ? "SAVE TRACK AS" : "RENAME TRACK", SW / 2, 40, 1, FG);
  cv.drawRect(30, 56, 180, 16, DIM);
  char b[24]; snprintf(b, sizeof(b), "%s%s", nameBuf, ((millis() / 400) & 1) ? "_" : " ");
  ltxt(b, 36, 60, 1, FG);
  ctxt("ENTER ok   ` cancel   DEL erase", SW / 2, 80, 1, DIM);
}

static void drawMenuOverlay() {
  drawMain();
  cv.fillRect(40, 8, 160, 120, BG);
  cv.drawRect(40, 8, 160, 120, FG);
  ctxt("MENU", SW / 2, 13, 2, FG);
  char s2[28], s3[28], s4[28], s5[28];
  snprintf(s2, sizeof(s2), "VOLUME  %d/10", vol);
  snprintf(s3, sizeof(s3), "COLOR  %s", PALN[colorIdx]);
  snprintf(s4, sizeof(s4), "COUNT-IN  %s", countOn ? "ON" : "OFF");
  snprintf(s5, sizeof(s5), "METRONOME  %s", metroOn ? "ON" : "OFF");
  const char* it[7] = {"RESUME", "HELP / KEYS", s2, s3, s4, s5, confirmNew ? "SURE? ENTER" : "NEW PROJECT"};
  for (int i = 0; i < 7; i++) {
    int y = 36 + i * 12;
    if (i == menuIdx) { cv.fillRect(48, y - 2, 144, 11, FG); ctxt(it[i], SW / 2, y, 1, BG); }
    else ctxt(it[i], SW / 2, y, 1, FG);
  }
}

static void drawHelp() {
  cv.fillScreen(BG);
  char t[24]; snprintf(t, sizeof(t), "KEYS  %d/2", helpPage + 1);
  ltxt(t, 4, 3, 1, FG);
  cv.drawFastHLine(0, 13, SW, DIM2);
  static const char* p1[9] = {
    "PADS  A-K (low row)  Q-I (top row)",
    "Z play/stop       X rec/overdub",
    "C undo/redo       V clear (2x)",
    "B bars 1/2/4  N metro  M count-in",
    "SPACE tap tempo       DEL stop",
    ", /  BPM -1/+1  (Shift: 10)",
    "- =  swing           ; .  volume",
    "1-8 pattern   Opt+1-8 copy to slot",
    "9 reset mutes and solo"
  };
  static const char* p2[9] = {
    "Shift+pad  mute lane",
    "Ctrl+pad   solo lane",
    "Alt+pad    erase lane",
    "L add pattern to chain   O remove",
    "Shift+O clear chain   P song mode",
    "ENTER save   TAB catalog   ` menu",
    "CATALOG: ; . pick  ENTER play",
    "E edit  F fav  R rename  DEL delete",
    "Z stop     , / ALL-FAV filter"
  };
  const char** pg = helpPage == 0 ? p1 : p2;
  for (int i = 0; i < 9; i++) ltxt(pg[i], 4, 18 + i * 11, 1, FG);
  ctxt(", / page    ENTER or ` close", SW / 2, 123, 1, DIM);
}

// ============================== EVENTS =============================
static void bpmAdd(int d) {
  int b = (int)P.bpm + d;
  if (b < 40) b = 40;
  if (b > 240) b = 240;
  P.bpm = b;
}

static void mainKey(const Ev& e) {
  char c = e.lc;
  for (int i = 0; i < 16; i++) {
    if (c == PADKEY[i]) {
      if (e.alt) eraseLane(i);
      else if (e.shift) {
        P.muteMask ^= (uint16_t)(1 << i);
        char b[28]; snprintf(b, sizeof(b), "%s %s", PADNAME[i], ((P.muteMask >> i) & 1) ? "MUTED" : "ON"); toast(b);
      } else if (e.ctrl) {
        solo = (solo == i) ? -1 : i;
        char b[28]; if (solo >= 0) snprintf(b, sizeof(b), "SOLO %s", PADNAME[i]); else snprintf(b, sizeof(b), "SOLO OFF");
        toast(b);
      } else hitPad(i);
      return;
    }
  }
  if (c >= '1' && c <= '8') { selectPattern(c - '1', e.opt); return; }
  switch (c) {
    case 'z': togglePlay(); break;
    case 'x': toggleRec(); break;
    case 'c': undoRedo(); break;
    case 'v': clearPattern(); break;
    case 'b': cycleBars(); break;
    case 'n': metroOn = !metroOn; saveSettings(); toast(metroOn ? "METRONOME ON" : "METRONOME OFF"); break;
    case 'm': countOn = !countOn; saveSettings(); toast(countOn ? "COUNT-IN ON" : "COUNT-IN OFF"); break;
    case ' ': tapTempo(); break;
    case ',': bpmAdd(-1); break;
    case '<': bpmAdd(-10); break;
    case '/': bpmAdd(1); break;
    case '?': bpmAdd(10); break;
    case ';': case ':': if (vol < 10) vol++; applyVolume(); saveSettings(); break;
    case '.': case '>': if (vol > 0) vol--; applyVolume(); saveSettings(); break;
    case '-': case '_': if (P.swing >= 5) P.swing -= 5; else P.swing = 0; break;
    case '=': case '+': if (P.swing <= 55) P.swing += 5; break;
    case '9': P.muteMask = 0; solo = -1; toast("MUTES RESET"); break;
    case 'l':
      if (P.chainLen < 32) { P.chain[P.chainLen++] = curPat; char t[24]; snprintf(t, sizeof(t), "CHAIN +%d", curPat + 1); toast(t); }
      else toast("CHAIN FULL");
      break;
    case 'o':
      if (e.shift) { P.chainLen = 0; songMode = false; toast("CHAIN CLEARED"); }
      else if (P.chainLen > 0) { P.chainLen--; if (P.chainLen == 0) songMode = false; toast("CHAIN -1"); }
      break;
    case 'p':
      if (P.chainLen == 0) toast("CHAIN EMPTY - USE L");
      else {
        songMode = !songMode;
        if (songMode) { chainPos = 0; if (tState == T_STOP) curPat = P.chain[0]; }
        toast(songMode ? "SONG MODE ON" : "SONG MODE OFF");
      }
      break;
    case '`': screen = SC_MENU; menuIdx = 0; confirmNew = false; break;
  }
}

static void openCatalog() {
  scanCatalog(curFile[0] ? curFile : nullptr);
  screen = SC_CAT;
}

static void catPlay() {
  if (viewN == 0) return;
  char f[28]; strncpy(f, cat[viewIdx[catSel]].file, 27); f[27] = 0;
  if (!loadProject(f)) { toast("LOAD FAILED"); return; }
  songMode = P.chainLen > 0;
  startPlay(false);
  rec = false;
}
static void catKey(const Ev& e) {
  char c = e.lc;
  if (c == ';' || c == ':') { if (catSel > 0) catSel--; }
  else if (c == '.' || c == '>') { if (catSel < viewN - 1) catSel++; }
  else if (c == ',' || c == '<' || c == '/' || c == '?') { favOnly = !favOnly; rebuildView(nullptr); }
  else if (c == 'z') { stopPlay(); }
  else if (c == '`') { screen = SC_MAIN; }
  else if (viewN > 0) {
    CatEntry& en = cat[viewIdx[catSel]];
    if (c == 'e') {
      if (loadProject(en.file)) { screen = SC_MAIN; toast("LOADED FOR EDIT"); }
      else toast("LOAD FAILED");
    } else if (c == 'f') {
      char f[28]; strncpy(f, en.file, 27); f[27] = 0;
      if (readProject(f, tmpP)) {
        tmpP.fav = tmpP.fav ? 0 : 1;
        if (writeProject(tmpP, f)) { toast(tmpP.fav ? "ADDED TO FAVORITES" : "REMOVED FROM FAV"); scanCatalog(f); }
      }
    } else if (c == 'r') {
      strncpy(renameFile, en.file, 27); renameFile[27] = 0;
      openName(NAME_RENAME, en.name);
    }
  }
}

static void nameKey(const Ev& e) {
  char c = e.c;
  if (c == '`') { screen = (nameMode == NAME_SAVE) ? SC_MAIN : SC_CAT; return; }
  if (c == ' ') c = '_';
  if ((isalnum((unsigned char)c) || c == '-' || c == '_') && nameLen < 16) {
    nameBuf[nameLen++] = c; nameBuf[nameLen] = 0;
  }
}

static void processEvents() {
  switch (screen) {
    case SC_MAIN:
      for (int i = 0; i < nev; i++) mainKey(evq[i]);
      if (evTab) openCatalog();
      if (evEnter) {
        if (P.name[0]) strncpy(nameBuf, P.name, 16); else defaultName(nameBuf);
        nameBuf[16] = 0;
        openName(NAME_SAVE, nameBuf);
      }
      if (evDel) { stopPlay(); gStep = 0; }
      break;

    case SC_CAT:
      for (int i = 0; i < nev; i++) catKey(evq[i]);
      if (evTab) screen = SC_MAIN;
      if (evEnter) catPlay();
      if (evDel && viewN > 0) {
        if (millis() < delConfirmUntil) {
          char path[64]; pathOf(cat[viewIdx[catSel]].file, path, sizeof(path));
          if (strcmp(curFile, cat[viewIdx[catSel]].file) == 0) curFile[0] = 0;
          SD.remove(path);
          delConfirmUntil = 0;
          scanCatalog(nullptr);
          toast("TRACK DELETED");
        } else { delConfirmUntil = millis() + 2000; toast("DEL AGAIN TO DELETE"); }
      }
      break;

    case SC_NAME:
      for (int i = 0; i < nev; i++) nameKey(evq[i]);
      if (screen != SC_NAME) break;
      if (evDel && nameLen > 0) nameBuf[--nameLen] = 0;
      if (evTab) screen = (nameMode == NAME_SAVE) ? SC_MAIN : SC_CAT;
      else if (evEnter) {
        if (nameMode == NAME_SAVE) { doSave(); screen = SC_MAIN; }
        else { doRename(); screen = SC_CAT; }
      }
      break;

    case SC_MENU:
      for (int i = 0; i < nev; i++) {
        char c = evq[i].lc;
        if (c == '`') { screen = SC_MAIN; break; }
        if (c == ';') { menuIdx = (menuIdx + 6) % 7; confirmNew = false; }
        else if (c == '.') { menuIdx = (menuIdx + 1) % 7; confirmNew = false; }
        else if (c == ',' || c == '/') {
          int d = (c == ',') ? -1 : 1;
          if (menuIdx == 2) { vol = vol + d; if (vol < 0) vol = 0; if (vol > 10) vol = 10; applyVolume(); saveSettings(); }
          else if (menuIdx == 3) { colorIdx = (colorIdx + d + 6) % 6; applyColor(); saveSettings(); }
          else if (menuIdx == 4) { countOn = !countOn; saveSettings(); }
          else if (menuIdx == 5) { metroOn = !metroOn; saveSettings(); }
        }
      }
      if (evTab) screen = SC_MAIN;
      if (evEnter && screen == SC_MENU) {
        if (menuIdx == 0) screen = SC_MAIN;
        else if (menuIdx == 1) { helpPage = 0; screen = SC_HELP; }
        else if (menuIdx == 4) { countOn = !countOn; saveSettings(); }
        else if (menuIdx == 5) { metroOn = !metroOn; saveSettings(); }
        else if (menuIdx == 6) {
          if (!confirmNew) confirmNew = true;
          else { newProject(); confirmNew = false; screen = SC_MAIN; toast("NEW PROJECT"); }
        }
      }
      break;

    case SC_HELP:
      for (int i = 0; i < nev; i++) {
        char c = evq[i].lc;
        if (c == '`') screen = SC_MAIN;
        else if (c == ',' || c == '/') helpPage = 1 - helpPage;
      }
      if (evEnter || evTab) screen = SC_MAIN;
      break;
  }
}

// ============================== MAIN ===============================
static uint32_t lastDraw = 0;

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  cv.setColorDepth(16);
  cv.createSprite(SW, SH);
  prefs.begin("drum", false);
  colorIdx = prefs.getUChar("col", 0) % 6;
  vol = prefs.getUChar("vol", 8); if (vol > 10) vol = 8;
  metroOn = prefs.getBool("met", true);
  countOn = prefs.getBool("cnt", true);
  applyColor();
  applyVolume();
  for (int ch = 0; ch < 8; ch++) M5Cardputer.Speaker.setChannelVolume(ch, ch == 7 ? 130 : 255);

  cv.fillScreen(BG);
  ctxt("DRUM MACHINE", SW / 2, 50, 2, FG);
  ctxt("loading sounds...", SW / 2, 78, 1, DIM);
  cv.pushSprite(0, 0);

  synthAll();
  initProject(P);
  SPI.begin(CSCK, CMISO, CMOSI, CCS);
  sdOk = SD.begin(CCS, SPI, 25000000);
  if (sdOk) SD.mkdir(DIRP);
  xTaskCreatePinnedToCore(seqTask, "seq", 4096, NULL, 3, NULL, 0);
}

void loop() {
  pollKeys();
  processEvents();
  uint32_t now = millis();
  if (now - lastDraw >= 33) {
    lastDraw = now;
    switch (screen) {
      case SC_MAIN: drawMain(); break;
      case SC_CAT:  drawCatalog(); break;
      case SC_MENU: drawMenuOverlay(); break;
      case SC_HELP: drawHelp(); break;
      case SC_NAME: drawNameDialog(); break;
    }
    cv.pushSprite(0, 0);
  }
  vTaskDelay(1);
}