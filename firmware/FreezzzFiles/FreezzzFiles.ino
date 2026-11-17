/*
 * FREEzzz FILES - file tools firmware for M5Stack Cardputer ADV
 *
 * Board:     M5Stack -> M5Cardputer (ESP32-S3)
 * Libraries: M5Cardputer, M5Unified, M5GFX (SD / SPI come with the ESP32 core)
 *
 *  01 FILE MANAGER    browse, new file/folder, rename, delete, copy, move, info
 *                     (smart info for ESP32 .bin images, partition tables,
 *                      ELF/PNG/ZIP/WAV..., ESP32 project folders)
 *  02 TEXT EDITOR     edit text files (up to ~40 KB), find, save
 *  03 HEX EDITOR      view / patch bytes, goto, find, save patches
 *  04 FILE VIEWER     TXT / LOG / JSON (pretty) / CSV (table), find, big files by parts
 *  05 ARCHIVE INDEX   catalog of zip/tar/gz/7z/rar/bin on the card, zip+tar contents, export
 *  06 SEARCH          by file name or by text inside files
 *  07 SD DIAGNOSTICS  card info, free space, speed + verify test, tree/read check
 *  08 SERIAL TERMINAL USB or Grove UART (G1=RX, G2=TX), hex view, logs saved to SD
 *  09 BACKUP          backup a folder to /FREEzzz/backup, restore, delete
 *  10 SYSTEM INFO     chip, memory, battery, partition table
 *
 * KEYS
 *   ; . = up / down    , / = left / right    [ ] = page up / down
 *   Enter = open/ok    ` = back/exit    Tab = cancel in dialogs
 *   Menu: 1-9,0 jump to item   C color   M sound
 *   Text editor (typing screen): hold Fn / Opt / Alt / Ctrl + ; . , / = cursor,
 *     +A home +E end +T top +B bottom +S save +Q quit +F find +N find next
 */
#include <M5Cardputer.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include "FreezzzTypes.h"

static const int SW = 240, SH = 135;
static const int RXG = 1, TXG = 2;      // Grove UART pins (G1 = RX, G2 = TX)

M5Canvas cv(&M5Cardputer.Display);
Preferences prefs;

// ============================ SETTINGS =============================
static const uint8_t PAL[6][3] = {
  {60, 255, 90}, {255, 176, 0}, {235, 235, 235}, {0, 225, 255}, {255, 70, 70}, {255, 90, 255}
};
static const char* PALN[6] = {"GREEN", "AMBER", "WHITE", "CYAN", "RED", "PINK"};
static uint16_t FG, DIM, DIM2, BG = 0;
static int colorIdx = 0;
static bool sndOn = true, sdOk = false;
static uint8_t ioBuf[4096];

static void applyColor() {
  const uint8_t* c = PAL[colorIdx];
  auto& d = M5Cardputer.Display;
  FG = d.color565(c[0], c[1], c[2]);
  DIM = d.color565(c[0] * 0.45f, c[1] * 0.45f, c[2] * 0.45f);
  DIM2 = d.color565(c[0] * 0.18f, c[1] * 0.18f, c[2] * 0.18f);
}
static void applyVolume() { M5Cardputer.Speaker.setVolume(sndOn ? 120 : 0); }
static void beep() { if (sndOn) M5Cardputer.Speaker.tone(2000, 8); }
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }

// ============================== INPUT ==============================
struct Ev { char c, lc; bool shift, ctrl, alt, opt, fn; };
static Ev evq[16];
static int nev = 0;
static std::vector<char> prevK;
static char repChar = 0;
static uint32_t repSince = 0, repLast = 0;

static void pushEv(char c, bool sh, bool ct, bool al, bool op, bool fn) {
  if (nev >= 16) return;
  Ev& e = evq[nev++];
  e.c = c;
  e.lc = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
  e.shift = sh || (c >= 'A' && c <= 'Z');
  e.ctrl = ct; e.alt = al; e.opt = op; e.fn = fn;
}
static void pollKeys() {
  nev = 0;
  M5Cardputer.update();
  uint32_t now = millis();
  std::vector<char> cur;
  bool sh = false, ct = false, al = false, op = false, fn = false;
  if (M5Cardputer.Keyboard.isPressed()) {
    auto st = M5Cardputer.Keyboard.keysState();
    cur = st.word;
    if (st.enter) cur.push_back('\n');
    if (st.del) cur.push_back('\b');
    if (st.tab) cur.push_back('\t');
    sh = st.shift; ct = st.ctrl; al = st.alt; op = st.opt; fn = st.fn;
  }
  for (char c : cur) {
    bool was = false;
    for (char p : prevK) if (p == c) was = true;
    if (!was) { pushEv(c, sh, ct, al, op, fn); repChar = c; repSince = now; repLast = now; }
  }
  if (repChar) {
    bool still = false;
    for (char c : cur) if (c == repChar) still = true;
    if (!still || repChar == '\n' || repChar == '\t') { if (!still) repChar = 0; }
    else if (now - repSince > 420 && now - repLast >= 60) { pushEv(repChar, sh, ct, al, op, fn); repLast = now; }
  }
  prevK = cur;
}
static int takeEvents(Ev* dst) {
  int n = nev;
  memcpy(dst, evq, sizeof(Ev) * n);
  nev = 0;
  return n;
}
static int navOf(const Ev& e) {
  switch (e.c) {
    case ';': case ':': return 1;
    case '.': case '>': return 2;
    case ',': case '<': return 3;
    case '/': case '?': return 4;
    case '[': case '{': return 5;
    case ']': case '}': return 6;
  }
  return 0;
}
static bool isMod(const Ev& e) { return e.fn || e.ctrl || e.alt || e.opt; }

// ============================== DRAW HELPERS =======================
static char toastMsg[40];
static uint32_t toastUntil = 0;
static void toast(const char* s) { strncpy(toastMsg, s, 39); toastMsg[39] = 0; toastUntil = millis() + 1500; }

static void ctxt(const char* s, int cx, int y, uint16_t c) {
  cv.setTextSize(1); cv.setTextColor(c);
  cv.setCursor(cx - cv.textWidth(s) / 2, y); cv.print(s);
}
static void ltxt(const char* s, int x, int y, uint16_t c) {
  cv.setTextSize(1); cv.setTextColor(c);
  cv.setCursor(x, y); cv.print(s);
}
static void rtxt(const char* s, int xr, int y, uint16_t c) {
  cv.setTextSize(1); cv.setTextColor(c);
  cv.setCursor(xr - cv.textWidth(s), y); cv.print(s);
}
static void clipTo(char* out, const char* s, int maxc, bool tail = false) {
  int n = (int)strlen(s);
  if (n <= maxc) { strcpy(out, s); return; }
  if (tail) { out[0] = '~'; strcpy(out + 1, s + n - (maxc - 1)); }
  else { strncpy(out, s, maxc); out[maxc] = 0; }
}
static void header(const char* t, const char* right = nullptr) {
  cv.fillRect(0, 0, SW, 12, FG);
  ltxt(t, 4, 2, BG);
  if (right) rtxt(right, SW - 4, 2, BG);
}
static void footer(const char* t) {
  cv.fillRect(0, 124, SW, 11, BG);
  cv.drawFastHLine(0, 123, SW, DIM2);
  ltxt(t, 3, 126, DIM);
}
static void present() {
  if (millis() < toastUntil) {
    cv.fillRect(20, 56, 200, 16, BG);
    cv.drawRect(20, 56, 200, 16, FG);
    ctxt(toastMsg, 120, 60, FG);
  }
  cv.pushSprite(0, 0);
}
static void fmtSize(char* o, size_t n, uint64_t b) {
  if (b < 1024) snprintf(o, n, "%uB", (unsigned)b);
  else if (b < 1048576ULL) snprintf(o, n, "%.1fK", b / 1024.0);
  else if (b < 1073741824ULL) snprintf(o, n, "%.1fM", b / 1048576.0);
  else snprintf(o, n, "%.2fG", b / 1073741824.0);
}

// ============================== PATH / FILE HELPERS ================
static const char* baseName(const char* p) { const char* s = strrchr(p, '/'); return s ? s + 1 : p; }
static void joinPath(char* o, size_t n, const char* d, const char* nm) {
  if (strcmp(d, "/") == 0) snprintf(o, n, "/%s", nm); else snprintf(o, n, "%s/%s", d, nm);
}
static void parentDir(char* p) { char* s = strrchr(p, '/'); if (!s || s == p) { p[0] = '/'; p[1] = 0; } else *s = 0; }
static bool hasExt(const char* n, const char* ext) {
  size_t a = strlen(n), b = strlen(ext);
  return a >= b && strcasecmp(n + a - b, ext) == 0;
}
static bool isTextName(const char* n) {
  static const char* e[] = {".txt", ".log", ".md", ".ini", ".cfg", ".json", ".csv", ".ino", ".c", ".cpp", ".h", ".py",
                            ".js", ".html", ".xml", ".yml", ".yaml", ".conf", ".tmx", ".lua", ".sh", ".dmt0"};
  for (auto x : e) if (hasExt(n, x)) return true;
  return false;
}
static uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static int ciFind(const char* b, int n, const char* q) {
  int qn = (int)strlen(q);
  if (qn == 0) return -1;
  for (int i = 0; i + qn <= n; i++) {
    int k = 0;
    while (k < qn && tolower((unsigned char)b[i + k]) == tolower((unsigned char)q[k])) k++;
    if (k == qn) return i;
  }
  return -1;
}
static uint32_t crcTab[256];
static bool crcReady = false;
static void crcSetup() {
  for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); crcTab[i] = c; }
  crcReady = true;
}

// ============================== DIALOGS ============================
static bool inputText(const char* title, char* buf, int maxLen) {
  int len = (int)strlen(buf);
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    for (int i = 0; i < n; i++) {
      char c = ev[i].c;
      if (c == '\n') return len > 0;
      if (c == '\t') return false;
      if (c == '\b') { if (len) buf[--len] = 0; }
      else if (c >= 32 && c < 127 && len < maxLen) { buf[len++] = c; buf[len] = 0; }
    }
    cv.fillRect(10, 40, 220, 52, BG);
    cv.drawRect(10, 40, 220, 52, FG);
    ctxt(title, 120, 45, FG);
    cv.drawRect(16, 58, 208, 14, DIM);
    char t[40]; clipTo(t, buf, 33, true);
    char d[48]; snprintf(d, sizeof(d), "%s%s", t, ((millis() / 400) & 1) ? "_" : " ");
    ltxt(d, 20, 61, FG);
    ctxt("ENTER ok    TAB cancel", 120, 78, DIM);
    present(); delay(2);
  }
}
static bool confirmBox(const char* msg) {
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    for (int i = 0; i < n; i++) {
      if (ev[i].c == '\n' || ev[i].lc == 'y') return true;
      if (ev[i].c == '\t' || ev[i].c == '`' || ev[i].lc == 'n') return false;
    }
    char t[40]; clipTo(t, msg, 34);
    cv.fillRect(15, 42, 210, 46, BG);
    cv.drawRect(15, 42, 210, 46, FG);
    ctxt(t, 120, 50, FG);
    ctxt("ENTER = yes      TAB = no", 120, 72, DIM);
    present(); delay(2);
  }
}
static int ask3(const char* msg) {       // 1 yes, 0 no, -1 cancel
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    for (int i = 0; i < n; i++) {
      if (ev[i].c == '\n') return 1;
      if (ev[i].c == '\b') return 0;
      if (ev[i].c == '\t') return -1;
    }
    cv.fillRect(15, 42, 210, 46, BG);
    cv.drawRect(15, 42, 210, 46, FG);
    ctxt(msg, 120, 50, FG);
    ctxt("ENTER save  DEL discard  TAB back", 120, 72, DIM);
    present(); delay(2);
  }
}
static void msgBox(const char* l1, const char* l2 = "") {
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    if (n > 0) return;
    char a[40], b[40]; clipTo(a, l1, 34); clipTo(b, l2, 34);
    cv.fillRect(15, 40, 210, 52, BG);
    cv.drawRect(15, 40, 210, 52, FG);
    ctxt(a, 120, 48, FG); ctxt(b, 120, 60, FG);
    ctxt("press any key", 120, 78, DIM);
    present(); delay(2);
  }
}
static int popup(const char* title, const char* const* items, int n, int sel = 0) {
  while (true) {
    pollKeys();
    Ev ev[16]; int m = takeEvents(ev);
    for (int i = 0; i < m; i++) {
      int k = navOf(ev[i]);
      if (k == 1 && sel > 0) sel--;
      else if (k == 2 && sel < n - 1) sel++;
      else if (ev[i].c == '\n') return sel;
      else if (ev[i].c == '\t' || ev[i].c == '`') return -1;
      else if (ev[i].c >= '1' && ev[i].c <= '9' && ev[i].c - '1' < n) return ev[i].c - '1';
    }
    int h = 20 + n * 11, y = (SH - h) / 2;
    cv.fillRect(30, y, 180, h, BG);
    cv.drawRect(30, y, 180, h, FG);
    ctxt(title, 120, y + 4, FG);
    for (int i = 0; i < n; i++) {
      int yy = y + 16 + i * 11;
      char t[34]; clipTo(t, items[i], 28);
      if (i == sel) { cv.fillRect(34, yy - 1, 172, 10, FG); ctxt(t, 120, yy, BG); }
      else ctxt(t, 120, yy, FG);
    }
    present(); delay(2);
  }
}
static void progressBox(const char* title, const char* l1, const char* l2) {
  char a[40], b[40]; clipTo(a, l1, 34, true); clipTo(b, l2, 34, true);
  cv.fillRect(15, 36, 210, 60, BG);
  cv.drawRect(15, 36, 210, 60, FG);
  ctxt(title, 120, 41, FG);
  ctxt(a, 120, 56, FG); ctxt(b, 120, 68, DIM);
  ctxt("` cancel", 120, 83, DIM);
  present();
}
static bool cancelKey() {
  pollKeys();
  for (int i = 0; i < nev; i++) if (evq[i].c == '`' || evq[i].c == '\t') return true;
  return false;
}

// ---- generic text pager (info screens)
static char pg[40][46];
static int pgN = 0;
static bool (*pgHook)(char) = nullptr;
static void pgClear() { pgN = 0; }
static void pgAdd(const char* fmt, ...) {
  if (pgN >= 40) return;
  va_list a; va_start(a, fmt);
  vsnprintf(pg[pgN++], 46, fmt, a);
  va_end(a);
}
static void pager(const char* title, const char* hint) {
  int top = 0;
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    for (int i = 0; i < n; i++) {
      int k = navOf(ev[i]);
      if (k == 1 && top > 0) top--;
      else if (k == 2 && top < pgN - 12) top++;
      else if (k == 5) top = imax(0, top - 12);
      else if (k == 6) top = imax(0, imin(pgN - 12, top + 12));
      else if (ev[i].c == '`' || ev[i].c == '\t') { pgHook = nullptr; return; }
      else if (pgHook) pgHook(ev[i].lc);
    }
    cv.fillScreen(BG);
    header(title);
    for (int r = 0; r < 12 && top + r < pgN; r++) ltxt(pg[top + r], 4, 15 + r * 9, FG);
    footer(hint);
    present(); delay(2);
  }
}

// ============================== FILE KINDS / BIN INFO ==============
static const char* chipName(uint16_t id) {
  switch (id) {
    case 0: return "ESP32"; case 2: return "ESP32-S2"; case 5: return "ESP32-C3"; case 9: return "ESP32-S3";
    case 12: return "ESP32-C2"; case 13: return "ESP32-C6"; case 16: return "ESP32-H2";
  }
  return "ESP?";
}
// h = first bytes of file (up to 160), returns short description
static void describeMagic(const uint8_t* h, int r, uint32_t size, const char* name, char* out, size_t n) {
  out[0] = 0;
  if (r >= 16 && h[0] == 0xE9 && h[1] > 0 && h[1] < 17) {
    const char* cn = chipName(le16(h + 12));
    if (r >= 112 && le32(h + 32) == 0xABCD5432u) {
      char proj[33], ver[33];
      memcpy(proj, h + 80, 32); proj[32] = 0;
      memcpy(ver, h + 48, 32); ver[32] = 0;
      snprintf(out, n, "%s APP %s %s", cn, proj, ver);
    } else snprintf(out, n, "%s IMAGE (%d seg)", cn, h[1]);
    return;
  }
  if (r >= 2 && h[0] == 0xAA && h[1] == 0x50) { snprintf(out, n, "ESP PARTITION TABLE"); return; }
  if (r >= 4 && !memcmp(h, "\x7f" "ELF", 4)) { snprintf(out, n, "ELF EXECUTABLE"); return; }
  if (r >= 4 && !memcmp(h, "\x89PNG", 4)) { snprintf(out, n, "PNG IMAGE"); return; }
  if (r >= 2 && !memcmp(h, "BM", 2) && hasExt(name, ".bmp")) { snprintf(out, n, "BMP IMAGE"); return; }
  if (r >= 4 && !memcmp(h, "GIF8", 4)) { snprintf(out, n, "GIF IMAGE"); return; }
  if (r >= 3 && h[0] == 0xFF && h[1] == 0xD8 && h[2] == 0xFF) { snprintf(out, n, "JPEG IMAGE"); return; }
  if (r >= 4 && !memcmp(h, "PK\x03\x04", 4)) { snprintf(out, n, "ZIP ARCHIVE"); return; }
  if (r >= 2 && h[0] == 0x1F && h[1] == 0x8B) { snprintf(out, n, "GZIP"); return; }
  if (r >= 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4)) { snprintf(out, n, "WAV AUDIO"); return; }
  if (r >= 4 && !memcmp(h, "UF2\n", 4)) { snprintf(out, n, "UF2 FIRMWARE"); return; }
  if (r >= 4 && !memcmp(h, "7z\xBC\xAF", 4)) { snprintf(out, n, "7-ZIP"); return; }
  if (r >= 4 && !memcmp(h, "Rar!", 4)) { snprintf(out, n, "RAR ARCHIVE"); return; }
  if (r >= 4 && !memcmp(h, "%PDF", 4)) { snprintf(out, n, "PDF"); return; }
  if (r >= 3 && !memcmp(h, "ID3", 3)) { snprintf(out, n, "MP3 AUDIO"); return; }
  if (r >= 4 && !memcmp(h, "OggS", 4)) { snprintf(out, n, "OGG AUDIO"); return; }
  if (r >= 15 && !memcmp(h, "SQLite format 3", 15)) { snprintf(out, n, "SQLITE DATABASE"); return; }
  if (hasExt(name, ".json")) { snprintf(out, n, "JSON"); return; }
  if (hasExt(name, ".csv")) { snprintf(out, n, "CSV TABLE"); return; }
  if (hasExt(name, ".ino") || hasExt(name, ".cpp") || hasExt(name, ".c") || hasExt(name, ".h") || hasExt(name, ".py")) { snprintf(out, n, "SOURCE CODE"); return; }
  int bad = 0;
  for (int i = 0; i < r; i++) if (h[i] < 9 || (h[i] > 13 && h[i] < 32)) bad++;
  if (r > 0 && bad * 10 <= r) snprintf(out, n, "TEXT");
  else if (r > 0) snprintf(out, n, hasExt(name, ".bin") ? "BINARY (.bin, unknown)" : "BINARY DATA");
  else snprintf(out, n, "EMPTY");
}
static void projectTag(const char* path, char* out, size_t n) {
  out[0] = 0;
  char p[160];
  joinPath(p, sizeof(p), path, "platformio.ini");
  if (SD.exists(p)) { snprintf(out, n, "ESP32 PROJECT: PlatformIO"); return; }
  joinPath(p, sizeof(p), path, "sdkconfig");
  if (SD.exists(p)) { snprintf(out, n, "ESP32 PROJECT: ESP-IDF"); return; }
  joinPath(p, sizeof(p), path, "CMakeLists.txt");
  if (SD.exists(p)) { snprintf(out, n, "ESP32 PROJECT: CMake"); return; }
  File d = SD.open(path);
  if (!d) return;
  File e; int cnt = 0, bins = 0, assets = 0;
  while (cnt < 40 && (e = d.openNextFile())) {
    const char* nm = baseName(e.name());
    if (hasExt(nm, ".ino")) { snprintf(out, n, "ARDUINO SKETCH: %s", nm); e.close(); d.close(); return; }
    if (hasExt(nm, ".bin")) bins++;
    if (hasExt(nm, ".bmp") || hasExt(nm, ".png") || hasExt(nm, ".wav") || hasExt(nm, ".raw") || hasExt(nm, ".tmx")) assets++;
    e.close(); cnt++;
  }
  d.close();
  if (bins > 0) snprintf(out, n, "FIRMWARE FOLDER (%d .bin)", bins);
  else if (assets > 2) snprintf(out, n, "GAME RESOURCES (%d files)", assets);
}
static void detectKind(const char* path, bool isDir, char* out, size_t n) {
  out[0] = 0;
  if (isDir) { projectTag(path, out, n); return; }
  File f = SD.open(path, FILE_READ);
  if (!f) return;
  uint8_t h[160]; memset(h, 0, sizeof(h));
  int r = f.read(h, sizeof(h));
  uint32_t size = f.size();
  f.close();
  describeMagic(h, r < 0 ? 0 : r, size, baseName(path), out, n);
}

static bool crcHook(char c);
static char infoPath[160];
static void fileInfo(const char* path) {
  strncpy(infoPath, path, 159); infoPath[159] = 0;
  File f = SD.open(path, FILE_READ);
  if (!f) { toast("OPEN FAILED"); return; }
  uint32_t size = f.size();
  uint8_t h[512]; memset(h, 0, sizeof(h));
  int r = f.read(h, sizeof(h));
  if (r < 0) r = 0;
  pgClear();
  char sz[16]; fmtSize(sz, sizeof(sz), size);
  pgAdd("%s", baseName(path));
  pgAdd("SIZE: %u bytes (%s)", (unsigned)size, sz);
  char kind[48]; describeMagic(h, r, size, baseName(path), kind, sizeof(kind));
  pgAdd("TYPE: %s", kind);
  if (r >= 16 && h[0] == 0xE9 && h[1] > 0 && h[1] < 17) {
    uint16_t cid = le16(h + 12);
    pgAdd("CHIP: %s   SEGMENTS: %d", chipName(cid), h[1]);
    pgAdd("ENTRY: 0x%08X", (unsigned)le32(h + 4));
    static const char* fmodes[] = {"QIO", "QOUT", "DIO", "DOUT"};
    static const char* fsizes[] = {"1MB", "2MB", "4MB", "8MB", "16MB", "32MB", "64MB", "128MB"};
    pgAdd("FLASH MODE: %s  SIZE: %s", fmodes[h[2] & 3], fsizes[(h[3] >> 4) & 7]);
    if (le32(h + 32) == 0xABCD5432u) {
      // esp_app_desc_t starts at offset 32: version@+16 project@+48 time@+80 date@+96 idf@+112
      char t[33], d[17], tm[17];
      memcpy(t, h + 80, 32); t[32] = 0; pgAdd("PROJECT: %s", t);
      memcpy(t, h + 48, 32); t[32] = 0; pgAdd("VERSION: %s", t);
      memcpy(tm, h + 112, 16); tm[16] = 0;
      memcpy(d, h + 128, 16); d[16] = 0;
      pgAdd("BUILT: %s %s", d, tm);
      memcpy(t, h + 144, 32); t[32] = 0;
      pgAdd("IDF: %s", t);
      pgAdd("LAUNCHER-READY APP IMAGE");
    } else pgAdd("NO APP DESCRIPTOR (bootloader/part)");
  }
  if (r >= 32 && h[0] == 0xAA && h[1] == 0x50) {
    pgAdd("PARTITION ENTRIES:");
    for (int i = 0; i + 32 <= r && i < 12 * 32; i += 32) {
      if (h[i] != 0xAA || h[i + 1] != 0x50) break;
      char nm[17]; memcpy(nm, h + i + 12, 16); nm[16] = 0;
      pgAdd(" %-10s @%06X %uK", nm, (unsigned)le32(h + i + 4), (unsigned)(le32(h + i + 8) / 1024));
    }
  }
  if (size > 0x9000 && h[0] == 0xE9) {              // maybe a full flash dump
    uint8_t t[2] = {0, 0};
    f.seek(0x8000); f.read(t, 2);
    if (t[0] == 0xAA && t[1] == 0x50) pgAdd("FULL FLASH IMAGE (table @0x8000)");
  }
  if (!strncmp(kind, "ZIP", 3) || !strncmp(kind, "GZIP", 4)) pgAdd("see ARCHIVE INDEX for contents");
  f.close();
  pgAdd("CRC32: press C to calculate");
  pgHook = crcHook;
  pager("FILE INFO", "; . scroll   C crc32   ` back");
}
static bool crcHook(char c) {
  if (c != 'c') return false;
  File f = SD.open(infoPath, FILE_READ);
  if (!f) return false;
  if (!crcReady) crcSetup();
  uint32_t crc = 0xFFFFFFFFu, size = f.size(), done = 0;
  bool ok = true;
  while (f.available()) {
    int n = f.read(ioBuf, sizeof(ioBuf));
    if (n <= 0) break;
    for (int i = 0; i < n; i++) crc = crcTab[(crc ^ ioBuf[i]) & 0xFF] ^ (crc >> 8);
    done += n;
    if (((done >> 15) != ((done - n) >> 15))) {
      char b[40]; snprintf(b, sizeof(b), "%u / %u KB", (unsigned)(done / 1024), (unsigned)(size / 1024));
      progressBox("CRC32", baseName(infoPath), b);
      if (cancelKey()) { ok = false; break; }
    }
  }
  f.close();
  if (ok) { crc ^= 0xFFFFFFFFu; snprintf(pg[pgN - 1], 46, "CRC32: %08X", (unsigned)crc); }
  return true;
}

// ============================== TREE OPS ===========================
static bool copyFile(const char* src, const char* dst, bool prog) {
  File a = SD.open(src, FILE_READ);
  if (!a) return false;
  File b = SD.open(dst, FILE_WRITE);
  if (!b) { a.close(); return false; }
  uint32_t total = a.size(), done = 0;
  bool ok = true;
  while (a.available()) {
    int n = a.read(ioBuf, sizeof(ioBuf));
    if (n <= 0) break;
    if (b.write(ioBuf, n) != (size_t)n) { ok = false; break; }
    done += n;
    if (prog && ((done >> 16) != ((done - n) >> 16))) {
      char s[40]; snprintf(s, sizeof(s), "%u / %u KB", (unsigned)(done / 1024), (unsigned)(total / 1024));
      progressBox("COPYING", baseName(src), s);
      if (cancelKey()) { ok = false; break; }
    }
  }
  a.close(); b.close();
  if (!ok) SD.remove(dst);
  return ok;
}
struct Child { String n; bool d; };
static void listChildren(const char* path, std::vector<Child>& v) {
  File d = SD.open(path);
  if (!d) return;
  File e;
  while ((e = d.openNextFile())) { Child c; c.n = baseName(e.name()); c.d = e.isDirectory(); v.push_back(c); e.close(); }
  d.close();
}
static bool removeTree(const char* path) {
  File d = SD.open(path);
  if (!d) return false;
  bool isd = d.isDirectory();
  d.close();
  if (!isd) return SD.remove(path);
  std::vector<Child> v; listChildren(path, v);
  for (auto& c : v) { char p[160]; joinPath(p, sizeof(p), path, c.n.c_str()); removeTree(p); }
  return SD.rmdir(path);
}
static int bkFiles = 0, bkFail = 0;
static uint32_t bkBytes = 0;
static bool bkCancel = false;
static void copyTree(const char* src, const char* dst) {
  if (bkCancel) return;
  SD.mkdir(dst);
  std::vector<Child> v; listChildren(src, v);
  for (auto& c : v) {
    if (bkCancel) return;
    if (strcmp(src, "/") == 0 && (c.n == "FREEzzz" || c.n == "System Volume Information")) continue;
    char s[160], d[160];
    joinPath(s, sizeof(s), src, c.n.c_str()); joinPath(d, sizeof(d), dst, c.n.c_str());
    if (c.d) copyTree(s, d);
    else {
      char l[40]; snprintf(l, sizeof(l), "%d files, %u KB", bkFiles, (unsigned)(bkBytes / 1024));
      progressBox("COPYING", c.n.c_str(), l);
      if (cancelKey()) { bkCancel = true; return; }
      File t = SD.open(s, FILE_READ);
      uint32_t sz = t ? t.size() : 0;
      if (t) t.close();
      if (copyFile(s, d, false)) { bkFiles++; bkBytes += sz; } else bkFail++;
    }
  }
}
static bool copyAny(const char* src, const char* dst) {
  File t = SD.open(src);
  if (!t) return false;
  bool isd = t.isDirectory();
  t.close();
  if (isd) { bkFiles = 0; bkFail = 0; bkBytes = 0; bkCancel = false; copyTree(src, dst); return !bkCancel && bkFail == 0; }
  return copyFile(src, dst, true);
}
static uint32_t stFiles, stDirs, stBytes, stErr, stCount;
static bool stDeep = false, stCancel = false;
static char stBig[64]; static uint32_t stBigSz;
static void treeStats(const char* path, int depth) {
  if (stCancel || depth > 12) return;
  File d = SD.open(path);
  if (!d) return;
  File e;
  while (!stCancel && (e = d.openNextFile())) {
    char nm[64]; strncpy(nm, baseName(e.name()), 63); nm[63] = 0;
    bool isd = e.isDirectory(); uint32_t sz = e.size();
    e.close();
    char p[160]; joinPath(p, sizeof(p), path, nm);
    stCount++;
    if (stCount % 25 == 0) {
      char l[40]; snprintf(l, sizeof(l), "%u files  %u dirs", (unsigned)stFiles, (unsigned)stDirs);
      progressBox("SCANNING", p, l);
      if (cancelKey()) stCancel = true;
    }
    if (isd) { stDirs++; if (strcmp(nm, "System Volume Information")) treeStats(p, depth + 1); }
    else {
      stFiles++; stBytes += sz;
      if (sz > stBigSz) { stBigSz = sz; strncpy(stBig, p, 63); stBig[63] = 0; }
      if (stDeep) {
        File t = SD.open(p, FILE_READ);
        if (!t) stErr++;
        else { int want = sz < 512 ? (int)sz : 512; if (want > 0 && t.read(ioBuf, want) != want) stErr++; t.close(); }
      }
    }
  }
  d.close();
}

// ============================== TEXT VIEWER ========================
static const int VPART = 32000, MAXL = 4000;
static char* vbuf = nullptr; static char* pbuf = nullptr; static const char* vcur = nullptr;
static uint16_t* vl = nullptr;
static int vraw = 0, vlen = 0, vnl = 0, vmode = 0;
static uint8_t csvW[16];

static int prettyJson(const char* src, int n, char* dst, int cap) {
  int o = 0, ind = 0; bool inStr = false, esc = false;
  for (int i = 0; i < n; i++) {
    char c = src[i];
    auto put = [&](char ch) { if (o < cap - 1) dst[o++] = ch; };
    auto nl = [&]() { put('\n'); for (int k = 0; k < ind * 2; k++) put(' '); };
    if (inStr) { put(c); if (esc) esc = false; else if (c == '\\') esc = true; else if (c == '"') inStr = false; continue; }
    switch (c) {
      case '"': inStr = true; put(c); break;
      case '{': case '[': put(c); ind++; nl(); break;
      case '}': case ']': ind = ind > 0 ? ind - 1 : 0; nl(); put(c); break;
      case ',': put(c); nl(); break;
      case ':': put(':'); put(' '); break;
      case ' ': case '\n': case '\r': case '\t': break;
      default: put(c);
    }
  }
  dst[o] = 0;
  return o;
}
static void buildLines(const char* b, int n, bool wrap, int cols) {
  vnl = 0; vl[vnl++] = 0;
  int col = 0;
  for (int i = 0; i < n && vnl < MAXL - 1; i++) {
    char c = b[i];
    if (c == '\n') { vl[vnl++] = i + 1; col = 0; }
    else if (c != '\r') { col++; if (wrap && col >= cols) { vl[vnl++] = i + 1; col = 0; } }
  }
  vl[vnl] = n;
}
static void csvFmt(const char* b, int s, int e, char* out, int on, bool measure) {
  int o = 0, col = 0, fl = 0; bool q = false; char f[32];
  auto flush = [&]() {
    int ci = col < 16 ? col : 15;
    if (measure) { if (fl > csvW[ci]) csvW[ci] = (uint8_t)imin(fl, 14); }
    else { int w = csvW[ci]; for (int i = 0; i < w && o < on - 2; i++) out[o++] = (i < fl) ? f[i] : ' '; if (o < on - 2) out[o++] = '|'; }
    col++; fl = 0;
  };
  for (int i = s; i < e; i++) {
    char c = b[i];
    if (c == '\r' || c == '\n') continue;
    if (c == '"') { q = !q; continue; }
    if (c == ',' && !q) { flush(); continue; }
    if (fl < 31) f[fl++] = c;
  }
  flush();
  out[o] = 0;
}
static void vRebuild() {
  if (vmode == 1) {
    if (!pbuf) pbuf = (char*)malloc(48000);
    if (pbuf) { vlen = prettyJson(vbuf, vraw, pbuf, 47990); vcur = pbuf; }
    else { vcur = vbuf; vlen = vraw; }
  } else { vcur = vbuf; vlen = vraw; }
  buildLines(vcur, vlen, vmode != 2, 39);
  if (vmode == 2) {
    for (int i = 0; i < 16; i++) csvW[i] = 2;
    char tmp[8];
    for (int i = 0; i < vnl && i < 60; i++) csvFmt(vcur, vl[i], vl[i + 1], tmp, 8, true);
  }
}
static void viewFile(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) { toast("OPEN FAILED"); return; }
  uint32_t fsz = f.size();
  uint8_t h[256]; int r = f.read(h, sizeof(h)); int bad = 0;
  for (int i = 0; i < r; i++) if (h[i] < 9 || (h[i] > 13 && h[i] < 32)) bad++;
  if (r > 0 && bad * 10 > r) {
    f.close();
    extern void hexFile(const char*, bool);
    if (confirmBox("BINARY FILE - OPEN IN HEX?")) hexFile(path, false);
    return;
  }
  vbuf = (char*)malloc(VPART + 4);
  vl = (uint16_t*)malloc(sizeof(uint16_t) * (MAXL + 2));
  if (!vbuf || !vl) { free(vbuf); free(vl); vbuf = nullptr; vl = nullptr; f.close(); toast("NO MEMORY"); return; }
  int defMode = hasExt(path, ".json") ? 1 : (hasExt(path, ".csv") ? 2 : 0);
  vmode = defMode;
  int part = 0, top = 0, hs = 0, parts = (int)((fsz + VPART - 1) / VPART); if (parts < 1) parts = 1;
  bool reload = true;
  char fq[32] = "";
  while (true) {
    if (reload) {
      f.seek((uint32_t)part * VPART);
      int n = f.read((uint8_t*)vbuf, VPART);
      vraw = n < 0 ? 0 : n; vbuf[vraw] = 0;
      vRebuild(); top = 0; hs = 0; reload = false;
    }
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    bool quit = false;
    for (int i = 0; i < n && !quit; i++) {
      int k = navOf(ev[i]); char c = ev[i].lc;
      if (k == 1 && top > 0) top--;
      else if (k == 2 && top < vnl - 1) top++;
      else if (k == 5) top = imax(0, top - 11);
      else if (k == 6) top = imin(vnl - 1, top + 11);
      else if (k == 3 && vmode == 2) hs = imax(0, hs - 6);
      else if (k == 4 && vmode == 2) hs += 6;
      else if (c == 'g') top = 0;
      else if (c == 'e') top = imax(0, vnl - 11);
      else if (c == 't') { if (defMode == 0) toast("NO ALT MODE"); else { vmode = (vmode == 0) ? defMode : 0; vRebuild(); top = 0; } }
      else if (c == '-' && part > 0) { part--; reload = true; }
      else if (c == '=' && part < parts - 1) { part++; reload = true; }
      else if (c == 'f') {
        if (inputText("FIND TEXT", fq, 28)) {
          int found = -1;
          for (int l = top + 1; l < vnl && found < 0; l++) if (ciFind(vcur + vl[l], vl[l + 1] - vl[l], fq) >= 0) found = l;
          if (found < 0) for (int l = 0; l <= top && found < 0; l++) if (ciFind(vcur + vl[l], vl[l + 1] - vl[l], fq) >= 0) found = l;
          if (found >= 0) top = found; else toast("NOT FOUND");
        }
        quit = false; break;
      }
      else if (ev[i].c == '`' || ev[i].c == '\t') quit = true;
    }
    if (quit) break;
    cv.fillScreen(BG);
    char t[40]; snprintf(t, sizeof(t), "%s", baseName(path));
    char tc[24]; clipTo(tc, t, 22);
    char rt[32]; snprintf(rt, sizeof(rt), "%d/%d P%d/%d", top + 1, vnl, part + 1, parts);
    header(tc, rt);
    for (int rrow = 0; rrow < 11 && top + rrow < vnl; rrow++) {
      int s = vl[top + rrow], e = vl[top + rrow + 1];
      char line[320];
      if (vmode == 2) {
        char tmp[320]; csvFmt(vcur, s, e, tmp, sizeof(tmp), false);
        int len = (int)strlen(tmp);
        if (hs < len) { strncpy(line, tmp + hs, 39); line[39] = 0; } else line[0] = 0;
      } else {
        int o = 0;
        for (int i = s; i < e && o < 39; i++) if (vcur[i] != '\n' && vcur[i] != '\r') line[o++] = (vcur[i] >= 32 && vcur[i] < 127) ? vcur[i] : '.';
        line[o] = 0;
      }
      ltxt(line, 2, 14 + rrow * 9, (vmode == 2 && top + rrow == 0) ? FG : FG);
    }
    const char* md = vmode == 1 ? "JSON" : (vmode == 2 ? "CSV" : "TEXT");
    char fh[48]; snprintf(fh, sizeof(fh), "; . [ ] scroll F find T %s - = part", md);
    footer(fh);
    present(); delay(2);
  }
  f.close();
  free(vbuf); free(vl); free(pbuf);
  vbuf = nullptr; vl = nullptr; pbuf = nullptr;
}

// ============================== TEXT EDITOR ========================
static char* eb = nullptr;
static int elen = 0, ecur = 0;
static const int ECAP = 40000;
static bool edirty = false;
static int lineStartAt(int p) { while (p > 0 && eb[p - 1] != '\n') p--; return p; }
static int lineEndAt(int p) { while (p < elen && eb[p] != '\n') p++; return p; }
static void eInsert(char ch) {
  if (elen >= ECAP - 2) return;
  memmove(eb + ecur + 1, eb + ecur, elen - ecur + 1);
  eb[ecur++] = ch; elen++; edirty = true;
}
static void eBackspace() {
  if (ecur <= 0) return;
  memmove(eb + ecur - 1, eb + ecur, elen - ecur + 1);
  ecur--; elen--; edirty = true;
}
static bool eSave(const char* path) {
  if (!SD.exists(path)) { toast("FILE NOT FOUND"); return false; }
  // Write to a temporary file and verify it before replacing the original.
  // Arduino SD FILE_WRITE opens in append mode, so never write directly here.
  char tmp[176];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  SD.remove(tmp);
  File w = SD.open(tmp, FILE_WRITE);
  if (!w) { toast("SAVE FAILED"); return false; }
  size_t written = w.write((const uint8_t*)eb, (size_t)elen);
  w.flush();
  w.close();
  File check = SD.open(tmp, FILE_READ);
  bool valid = check && written == (size_t)elen && check.size() == (uint32_t)elen;
  if (check) check.close();
  if (!valid) { SD.remove(tmp); toast("WRITE INCOMPLETE"); return false; }
  char bak[176];
  snprintf(bak, sizeof(bak), "%s.bak", path);
  SD.remove(bak);
  if (!SD.rename(path, bak)) { toast("BACKUP RENAME FAILED"); return false; }
  if (!SD.rename(tmp, path)) {
    SD.rename(bak, path); // best-effort rollback; verified temp remains available
    toast("REPLACE FAILED");
    return false;
  }
  SD.remove(bak);
  edirty = false; toast("SAVED");
  return true;
}
static void eFind(const char* q) {
  if (!q[0]) return;
  int start = ecur + 1; if (start > elen) start = 0;
  int i = ciFind(eb + start, elen - start, q);
  if (i >= 0) { ecur = start + i; return; }
  i = ciFind(eb, elen, q);
  if (i >= 0) ecur = i; else toast("NOT FOUND");
}
static void editFile(const char* path) {
  eb = (char*)malloc(ECAP + 2);
  if (!eb) { toast("NO MEMORY"); return; }
  elen = 0;
  File f = SD.open(path, FILE_READ);
  if (f) {
    if (f.size() > ECAP - 1000) { f.close(); free(eb); eb = nullptr; msgBox("FILE TOO BIG FOR EDITOR", "use VIEWER or HEX"); return; }
    elen = f.read((uint8_t*)eb, ECAP - 1); if (elen < 0) elen = 0;
    f.close();
  }
  eb[elen] = 0; ecur = 0; edirty = false;
  int topLine = 0, leftCol = 0;
  char fq[32] = "";
  const int ROWS = 11, COLS = 39;
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    bool quit = false;
    for (int i = 0; i < n && !quit; i++) {
      const Ev& e = ev[i];
      if (isMod(e)) {
        switch (e.lc) {
          case ';': { int ls = lineStartAt(ecur), col = ecur - ls; if (ls > 0) { int pe = ls - 1, ps = lineStartAt(pe); ecur = imin(ps + col, pe); } break; }
          case '.': { int le = lineEndAt(ecur), col = ecur - lineStartAt(ecur); if (le < elen) { int ns = le + 1, ne = lineEndAt(ns); ecur = imin(ns + col, ne); } break; }
          case ',': if (ecur > 0) ecur--; break;
          case '/': if (ecur < elen) ecur++; break;
          case 'a': ecur = lineStartAt(ecur); break;
          case 'e': ecur = lineEndAt(ecur); break;
          case 't': ecur = 0; break;
          case 'b': ecur = elen; break;
          case 's': eSave(path); break;
          case 'f': if (inputText("FIND TEXT", fq, 28)) eFind(fq); break;
          case 'n': eFind(fq); break;
          case 'q': {
            if (edirty) { int a = ask3("SAVE CHANGES?"); if (a == -1) break; if (a == 1 && !eSave(path)) break; }
            quit = true; break;
          }
        }
      } else if (e.c == '\n') eInsert('\n');
      else if (e.c == '\b') eBackspace();
      else if (e.c == '\t') { eInsert(' '); eInsert(' '); }
      else if (e.c >= 32 && e.c < 127) eInsert(e.c);
    }
    if (quit) break;
    // view metrics
    int curLine = 0;
    for (int i = 0; i < ecur; i++) if (eb[i] == '\n') curLine++;
    int curCol = ecur - lineStartAt(ecur);
    if (curLine < topLine) topLine = curLine;
    if (curLine >= topLine + ROWS) topLine = curLine - ROWS + 1;
    if (curCol < leftCol) leftCol = curCol;
    if (curCol >= leftCol + COLS) leftCol = curCol - COLS + 1;
    cv.fillScreen(BG);
    char tn[24]; clipTo(tn, baseName(path), 20);
    char hd[40]; snprintf(hd, sizeof(hd), "%s%s", tn, edirty ? " *" : "");
    char rt[24]; snprintf(rt, sizeof(rt), "Ln %d Col %d", curLine + 1, curCol + 1);
    header(hd, rt);
    int p = 0, l = 0;
    while (l < topLine && p < elen) { if (eb[p] == '\n') l++; p++; }
    for (int r = 0; r < ROWS; r++) {
      char line[COLS + 1]; int o = 0;
      int le = lineEndAt(p);
      for (int i = p + leftCol; i < le && o < COLS; i++) line[o++] = (eb[i] >= 32 && eb[i] < 127) ? eb[i] : '.';
      line[o] = 0;
      int y = 14 + r * 9;
      ltxt(line, 2, y, FG);
      if (topLine + r == curLine && ((millis() / 450) & 1)) {
        int cx = 2 + (curCol - leftCol) * 6;
        char ch[2] = {(curCol - leftCol < o) ? line[curCol - leftCol] : ' ', 0};
        cv.fillRect(cx, y - 1, 6, 9, FG); ltxt(ch, cx, y, BG);
      }
      if (le >= elen) { if (r + 1 < ROWS) { /* end of text */ } p = le; if (p >= elen) { for (int rr = r + 1; rr < ROWS; rr++) {} break; } }
      p = le + 1;
    }
    footer("Fn/Opt/Alt+ ; . , / move  S save Q quit");
    present(); delay(2);
  }
  free(eb); eb = nullptr;
}

// ============================== HEX EDITOR =========================
struct Patch { uint32_t off; uint8_t val; };
static Patch patches[128];
static int np = 0;
static uint8_t pageBuf[96];
static int pageN = 0;
static int64_t pageBase = -1;
static int patchIdx(uint32_t off) { for (int i = 0; i < np; i++) if (patches[i].off == off) return i; return -1; }
void hexFile(const char* path, bool editable) {
  File f = SD.open(path, FILE_READ);
  if (!f) { toast("OPEN FAILED"); return; }
  uint32_t fsz = f.size();
  if (fsz == 0) { f.close(); msgBox("EMPTY FILE"); return; }
  uint32_t cur = 0;
  bool edit = false; int pend = -1;
  np = 0; pageBase = -1;
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    bool quit = false;
    for (int i = 0; i < n && !quit; i++) {
      int k = navOf(ev[i]); char c = ev[i].lc;
      if (k == 1) { cur = cur >= 8 ? cur - 8 : cur; pend = -1; }
      else if (k == 2) { cur = (cur + 8 < fsz) ? cur + 8 : cur; pend = -1; }
      else if (k == 3) { if (cur > 0) cur--; pend = -1; }
      else if (k == 4) { if (cur + 1 < fsz) cur++; pend = -1; }
      else if (k == 5) { cur = cur >= 96 ? cur - 96 : 0; pend = -1; }
      else if (k == 6) { cur = (cur + 96 < fsz) ? cur + 96 : fsz - 1; pend = -1; }
      else if (ev[i].c == '\n') { if (editable) { edit = !edit; pend = -1; toast(edit ? "EDIT MODE ON" : "VIEW MODE"); } else toast("READ-ONLY"); }
      else if (edit && isxdigit((unsigned char)c)) {
        int v = (c <= '9') ? c - '0' : c - 'a' + 10;
        if (pend < 0) pend = v;
        else {
          uint8_t b = (uint8_t)((pend << 4) | v); pend = -1;
          int pi = patchIdx(cur);
          if (pi < 0 && np < 128) { patches[np].off = cur; patches[np].val = b; np++; }
          else if (pi >= 0) patches[pi].val = b;
          else toast("PATCH LIST FULL");
          if (cur + 1 < fsz) cur++;
        }
      }
      else if (c == 'g') {
        char b[12] = "";
        if (inputText("GOTO OFFSET (HEX)", b, 8)) { uint32_t o = strtoul(b, nullptr, 16); cur = (o < fsz) ? o : fsz - 1; }
        break;
      }
      else if (c == 'x') {
        static char q[40] = "";
        if (inputText("FIND: text or 0xHEX", q, 34)) {
          uint8_t pat[16]; int pl = 0;
          if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) {
            for (int a = 2; q[a] && q[a + 1] && pl < 16; a += 2) { char t[3] = {q[a], q[a + 1], 0}; pat[pl++] = (uint8_t)strtoul(t, nullptr, 16); }
          } else { pl = imin((int)strlen(q), 16); memcpy(pat, q, pl); }
          bool found = false;
          uint32_t pos = cur + 1;
          while (pl > 0 && pos < fsz && !found) {
            f.seek(pos);
            int rd = f.read(ioBuf, 1024);
            if (rd < pl) break;
            for (int a = 0; a + pl <= rd; a++) if (!memcmp(ioBuf + a, pat, pl)) { cur = pos + a; found = true; break; }
            pos += rd - pl + 1;
          }
          if (!found) toast("NOT FOUND");
        }
        break;
      }
      else if (c == 'u') { if (np > 0) { np--; toast("UNDO PATCH"); } }
      else if (c == 's') {
        if (np == 0) { toast("NO CHANGES"); }
        else {
          f.close();
          File w = SD.open(path, "r+");
          if (!w) { toast("WRITE FAILED"); }
          else {
            bool writeOk = true;
            for (int a = 0; a < np; a++) {
              if (!w.seek(patches[a].off) || w.write(&patches[a].val, 1) != 1) {
                writeOk = false;
                break;
              }
            }
            w.flush();
            w.close();
            if (writeOk) {
              np = 0;
              toast("PATCHES SAVED");
            } else {
              toast("PATCH WRITE FAILED");
            }
          }
          f = SD.open(path, FILE_READ); pageBase = -1;
        }
      }
      else if (ev[i].c == '`' || ev[i].c == '\t') {
        if (np > 0 && !confirmBox("DISCARD UNSAVED PATCHES?")) break;
        quit = true;
      }
    }
    if (quit) break;
    int64_t base = ((int64_t)cur / 96) * 96;
    if (pageBase != base) { f.seek((uint32_t)base); pageN = f.read(pageBuf, 96); if (pageN < 0) pageN = 0; pageBase = base; }
    cv.fillScreen(BG);
    char tn[22]; clipTo(tn, baseName(path), 18);
    char rt[28]; snprintf(rt, sizeof(rt), "%s %d", edit ? "EDIT" : "VIEW", np);
    header(tn, rt);
    for (int r = 0; r < 12; r++) {
      uint32_t ro = (uint32_t)base + r * 8;
      if (ro >= fsz) break;
      int y = 14 + r * 9;
      char t[12]; snprintf(t, sizeof(t), "%06X", (unsigned)ro);
      ltxt(t, 2, y, DIM);
      for (int b = 0; b < 8; b++) {
        uint32_t off = ro + b;
        if (off >= fsz) break;
        int idx = (int)(off - base);
        int pi = patchIdx(off);
        uint8_t v = pi >= 0 ? patches[pi].val : (idx < pageN ? pageBuf[idx] : 0);
        char hx[4]; snprintf(hx, sizeof(hx), "%02X", v);
        int x = 42 + b * 18;
        bool isCur = (off == cur);
        if (isCur) cv.fillRect(x - 1, y - 1, 14, 9, FG);
        ltxt(hx, x, y, isCur ? BG : FG);
        if (pi >= 0 && !isCur) cv.drawFastHLine(x, y + 8, 12, FG);
        char ac[2] = {(v >= 32 && v < 127) ? (char)v : '.', 0};
        int ax = 190 + b * 6;
        if (isCur) cv.fillRect(ax, y - 1, 6, 9, FG);
        ltxt(ac, ax, y, isCur ? BG : DIM);
      }
    }
    char st[40]; snprintf(st, sizeof(st), "%08X / %08X", (unsigned)cur, (unsigned)(fsz - 1));
    ltxt(st, 2, 113, DIM);
    if (pend >= 0) { char pc[8]; snprintf(pc, sizeof(pc), "%X_", pend); rtxt(pc, SW - 4, 113, FG); }
    footer(edit ? "0-F type  U undo  S save  ENTER stop edit" : "ENTER edit G goto X find S save ` exit");
    present(); delay(2);
  }
  f.close();
}

// ============================== FILE MANAGER =======================
struct FEnt { char name[64]; uint8_t dir; uint32_t size; };
static FEnt ents[200];
static int nEnt = 0;
static char cwd[128] = "/";
static char clipPath[160] = "";
static bool clipCut = false, sortBySize = false;

static bool entLess(const FEnt& a, const FEnt& b) {
  if (a.dir != b.dir) return a.dir > b.dir;
  if (sortBySize && !a.dir) return a.size > b.size;
  return strcasecmp(a.name, b.name) < 0;
}
static bool loadDir(const char* path) {
  nEnt = 0;
  File d = SD.open(path);
  if (!d) return false;
  if (!d.isDirectory()) { d.close(); return false; }
  File e;
  while (nEnt < 200 && (e = d.openNextFile())) {
    FEnt& x = ents[nEnt];
    strncpy(x.name, baseName(e.name()), 63); x.name[63] = 0;
    x.dir = e.isDirectory(); x.size = x.dir ? 0 : e.size();
    e.close(); nEnt++;
  }
  d.close();
  for (int i = 1; i < nEnt; i++) {
    FEnt t = ents[i]; int j = i - 1;
    while (j >= 0 && entLess(t, ents[j])) { ents[j + 1] = ents[j]; j--; }
    ents[j + 1] = t;
  }
  return true;
}
static void openFilePopup(const char* p) {
  static const char* it[] = {"VIEW", "EDIT TEXT", "HEX", "INFO"};
  int r = popup(baseName(p), it, 4);
  if (r == 0) viewFile(p);
  else if (r == 1) editFile(p);
  else if (r == 2) hexFile(p, true);
  else if (r == 3) fileInfo(p);
}
static void dirSummary(const char* p) {
  stFiles = stDirs = stBytes = stErr = stCount = 0; stDeep = false; stCancel = false; stBigSz = 0; stBig[0] = 0;
  treeStats(p, 0);
  char sz[16]; fmtSize(sz, sizeof(sz), stBytes);
  char a[40], b[40];
  snprintf(a, sizeof(a), "%u files  %u folders", (unsigned)stFiles, (unsigned)stDirs);
  snprintf(b, sizeof(b), "total %s%s", sz, stCancel ? " (partial)" : "");
  msgBox(a, b);
}
// mode: 0 manage, 1 pick a file (returns path), 2 pick a folder (Tab = use current)
static bool browse(int mode, char* out, size_t outN) {
  if (!loadDir(cwd)) { strcpy(cwd, "/"); loadDir(cwd); }
  int sel = 0, top = 0, kindFor = -2;
  char kind[48] = "";
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    bool stop = false;
    for (int i = 0; i < n && !stop; i++) {
      const Ev& e = ev[i]; int k = navOf(e); char c = e.lc;
      char p[160] = "";
      if (nEnt > 0) joinPath(p, sizeof(p), cwd, ents[sel].name);
      if (k == 1 && sel > 0) sel--;
      else if (k == 2 && sel < nEnt - 1) sel++;
      else if (k == 5) sel = imax(0, sel - 8);
      else if (k == 6) sel = imax(0, imin(nEnt - 1, sel + 8));
      else if (k == 3) {
        if (strcmp(cwd, "/") != 0) {
          char old[64]; strncpy(old, baseName(cwd), 63); old[63] = 0;
          parentDir(cwd); loadDir(cwd); sel = 0;
          for (int a = 0; a < nEnt; a++) if (!strcmp(ents[a].name, old)) sel = a;
          kindFor = -2;
        }
      }
      else if (k == 4 || e.c == '\n') {
        if (nEnt == 0) continue;
        if (ents[sel].dir) { strncpy(cwd, p, 127); cwd[127] = 0; loadDir(cwd); sel = 0; top = 0; kindFor = -2; }
        else if (e.c == '\n') {
          if (mode == 1) { strncpy(out, p, outN - 1); out[outN - 1] = 0; return true; }
          if (mode == 0) { openFilePopup(p); loadDir(cwd); kindFor = -2; stop = true; }
        }
      }
      else if (e.c == '`') return false;
      else if (e.c == '\t' && mode == 2) { strncpy(out, cwd, outN - 1); out[outN - 1] = 0; return true; }
      else if (mode == 0) {
        if (c == 'n') {
          char nm[48] = "";
          if (inputText("NEW FILE NAME", nm, 40)) {
            char q[160]; joinPath(q, sizeof(q), cwd, nm);
            if (SD.exists(q)) toast("ALREADY EXISTS");
            else { File t = SD.open(q, FILE_WRITE); if (t) { t.close(); toast("FILE CREATED"); } else toast("FAILED"); }
            loadDir(cwd);
          }
          stop = true;
        } else if (c == 'k') {
          char nm[48] = "";
          if (inputText("NEW FOLDER NAME", nm, 40)) {
            char q[160]; joinPath(q, sizeof(q), cwd, nm);
            toast(SD.mkdir(q) ? "FOLDER CREATED" : "FAILED");
            loadDir(cwd);
          }
          stop = true;
        } else if (c == 'r' && nEnt > 0) {
          char nm[48]; strncpy(nm, ents[sel].name, 47); nm[47] = 0;
          if (inputText("RENAME TO", nm, 44)) {
            char q[160]; joinPath(q, sizeof(q), cwd, nm);
            if (SD.exists(q)) toast("NAME EXISTS");
            else toast(SD.rename(p, q) ? "RENAMED" : "FAILED");
            loadDir(cwd);
          }
          stop = true;
        } else if (c == 'x' && nEnt > 0) {
          char m[60]; snprintf(m, sizeof(m), "DELETE %s?", ents[sel].name);
          if (confirmBox(m)) { toast(removeTree(p) ? "DELETED" : "DELETE FAILED"); loadDir(cwd); if (sel >= nEnt) sel = imax(0, nEnt - 1); }
          kindFor = -2; stop = true;
        } else if ((c == 'c' || c == 'm') && nEnt > 0) {
          strncpy(clipPath, p, 159); clipPath[159] = 0; clipCut = (c == 'm');
          toast(clipCut ? "MARKED FOR MOVE" : "MARKED FOR COPY");
        } else if (c == 'v' && clipPath[0]) {
          char q[160]; joinPath(q, sizeof(q), cwd, baseName(clipPath));
          if (SD.exists(q)) toast("NAME EXISTS HERE");
          else {
            bool ok = false;
            if (clipCut) { ok = SD.rename(clipPath, q); if (!ok) { ok = copyAny(clipPath, q); if (ok) removeTree(clipPath); } }
            else ok = copyAny(clipPath, q);
            toast(ok ? (clipCut ? "MOVED" : "COPIED") : "FAILED");
            if (ok && clipCut) clipPath[0] = 0;
            loadDir(cwd);
          }
          kindFor = -2; stop = true;
        } else if (c == 'i' && nEnt > 0) {
          if (ents[sel].dir) dirSummary(p); else fileInfo(p);
          stop = true;
        } else if (c == 'e' && nEnt > 0 && !ents[sel].dir) { editFile(p); loadDir(cwd); stop = true; }
        else if (c == 'h' && nEnt > 0 && !ents[sel].dir) { hexFile(p, true); stop = true; }
        else if (c == 's') { sortBySize = !sortBySize; loadDir(cwd); toast(sortBySize ? "SORT: SIZE" : "SORT: NAME"); }
      }
    }
    if (sel >= nEnt) sel = imax(0, nEnt - 1);
    if (sel < top) top = sel;
    if (sel >= top + 8) top = sel - 7;
    if (kindFor != sel) {
      kindFor = sel; kind[0] = 0;
      if (nEnt > 0) { char p[160]; joinPath(p, sizeof(p), cwd, ents[sel].name); detectKind(p, ents[sel].dir, kind, sizeof(kind)); }
    }
    cv.fillScreen(BG);
    char hd[40]; clipTo(hd, cwd, 30, true);
    char cnt[16]; snprintf(cnt, sizeof(cnt), "%d", nEnt);
    header(mode == 1 ? "PICK FILE" : (mode == 2 ? "PICK FOLDER" : hd), mode == 0 ? cnt : hd);
    if (nEnt == 0) ctxt("(empty)", 120, 50, DIM);
    for (int r = 0; r < 8 && top + r < nEnt; r++) {
      FEnt& x = ents[top + r];
      int y = 15 + r * 10;
      bool s = (top + r == sel);
      if (s) cv.fillRect(0, y - 1, SW, 10, FG);
      char nm[40];
      if (x.dir) { char t[36]; clipTo(t, x.name, 29); snprintf(nm, sizeof(nm), "[%s]", t); } else clipTo(nm, x.name, 30);
      ltxt(nm, 3, y, s ? BG : FG);
      if (!x.dir) { char sz[16]; fmtSize(sz, sizeof(sz), x.size); rtxt(sz, SW - 3, y, s ? BG : DIM); }
    }
    cv.drawFastHLine(0, 96, SW, DIM2);
    char kl[48]; clipTo(kl, kind[0] ? kind : (nEnt > 0 && ents[sel].dir ? "FOLDER" : ""), 39);
    ltxt(kl, 3, 99, FG);
    if (clipPath[0]) { char cl[48]; snprintf(cl, sizeof(cl), "%s: %s", clipCut ? "MOVE" : "COPY", baseName(clipPath)); char t[44]; clipTo(t, cl, 38); ltxt(t, 3, 108, DIM); }
    if (mode == 0) {
      ltxt("N file K dir R ren X del C cp M mv V paste", 3, 114, DIM);
      footer("I info E edit H hex S sort , up  / open");
    } else if (mode == 1) footer("ENTER select    , / navigate    ` cancel");
    else footer("TAB use this folder  / enter  , up  ` cancel");
    present(); delay(2);
  }
}

// ============================== ARCHIVE INDEX ======================
struct Arc { char path[96]; uint32_t size; uint8_t kind; };
static Arc arcs[150];
static int nArc = 0, arcScanned = 0;
static bool arcCancel = false;
static char zn[200][56]; static uint32_t zs[200];
static const char* ARCK[7] = {"ZIP", "TAR", "GZ", "7Z", "RAR", "BIN", "ARC"};
static int arcKind(const char* n) {
  if (hasExt(n, ".zip") || hasExt(n, ".jar") || hasExt(n, ".apk")) return 0;
  if (hasExt(n, ".tar")) return 1;
  if (hasExt(n, ".gz") || hasExt(n, ".tgz")) return 2;
  if (hasExt(n, ".7z")) return 3;
  if (hasExt(n, ".rar")) return 4;
  if (hasExt(n, ".bin") || hasExt(n, ".uf2") || hasExt(n, ".elf")) return 5;
  return -1;
}
static void arcWalk(const char* dir, int depth) {
  if (arcCancel || nArc >= 150 || depth > 6) return;
  File d = SD.open(dir);
  if (!d) return;
  File e;
  while (!arcCancel && nArc < 150 && (e = d.openNextFile())) {
    char nm[64]; strncpy(nm, baseName(e.name()), 63); nm[63] = 0;
    bool isd = e.isDirectory(); uint32_t sz = e.size();
    e.close();
    char p[128]; joinPath(p, sizeof(p), dir, nm);
    arcScanned++;
    if (arcScanned % 25 == 0) {
      char l[40]; snprintf(l, sizeof(l), "%d files, %d found", arcScanned, nArc);
      progressBox("INDEXING ARCHIVES", p, l);
      if (cancelKey()) arcCancel = true;
    }
    if (isd) { if (strcmp(nm, "System Volume Information")) arcWalk(p, depth + 1); continue; }
    int k = arcKind(nm);
    if (k >= 0) { Arc& a = arcs[nArc++]; strncpy(a.path, p, 95); a.path[95] = 0; a.size = sz; a.kind = (uint8_t)k; }
  }
  d.close();
}
static int zipList(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return -1;
  uint32_t sz = f.size();
  if (sz < 22) { f.close(); return -1; }
  // EOCD may be followed by a ZIP comment of up to 65535 bytes.
  // Search backwards in bounded chunks instead of assuming the last 4 KB.
  int32_t eocdPos = -1;
  uint32_t end = sz;
  while (end > 0 && eocdPos < 0) {
    uint32_t start = end > sizeof(ioBuf) ? end - sizeof(ioBuf) : 0;
    uint32_t count = end - start;
    if (!f.seek(start)) break;
    int got = f.read(ioBuf, count);
    if (got != (int)count) break;
    for (int i = got - 4; i >= 0; --i) {
      if (ioBuf[i] != 0x50 || ioBuf[i + 1] != 0x4B || ioBuf[i + 2] != 5 || ioBuf[i + 3] != 6) continue;
      if (i + 22 > got) continue;
      uint16_t commentLen = le16(ioBuf + i + 20);
      uint32_t absolute = start + (uint32_t)i;
      if (absolute + 22u + commentLen == sz) { eocdPos = (int32_t)absolute; break; }
    }
    if (start == 0) break;
    end = start + 3; // overlap signature bytes across chunk boundary
  }
  if (eocdPos < 0 || !f.seek((uint32_t)eocdPos) || f.read(ioBuf, 22) != 22) { f.close(); return -1; }
  int entries = le16(ioBuf + 10);
  uint32_t pos = le32(ioBuf + 16);
  int cnt = 0;
  for (int k = 0; k < entries && cnt < 200; k++) {
    uint8_t h[46];
    f.seek(pos);
    if (f.read(h, 46) != 46 || le32(h) != 0x02014B50u) break;
    int nl = le16(h + 28), xl = le16(h + 30), cl = le16(h + 32);
    int rn = nl < 55 ? nl : 55;
    int got = f.read((uint8_t*)zn[cnt], rn); if (got < 0) got = 0;
    zn[cnt][got] = 0; zs[cnt] = le32(h + 24); cnt++;
    pos += 46 + nl + xl + cl;
  }
  f.close();
  return cnt;
}
static int tarList(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return -1;
  uint32_t fsz = f.size(), pos = 0; int cnt = 0;
  while (pos + 512 <= fsz && cnt < 200) {
    f.seek(pos);
    if (f.read(ioBuf, 512) != 512) break;
    bool zero = true;
    for (int i = 0; i < 512; i++) if (ioBuf[i]) { zero = false; break; }
    if (zero) break;
    char nm[101]; memcpy(nm, ioBuf, 100); nm[100] = 0;
    uint32_t size = strtoul((char*)ioBuf + 124, nullptr, 8);
    char tf = (char)ioBuf[156];
    if (tf != 'x' && tf != 'g' && nm[0]) { strncpy(zn[cnt], nm, 55); zn[cnt][55] = 0; zs[cnt] = size; cnt++; }
    pos += 512 + ((size + 511) / 512) * 512;
  }
  f.close();
  return cnt;
}
static void showEntries(const char* title, int n) {
  pgClear();
  int sel = 0, top = 0;
  while (true) {
    pollKeys();
    Ev ev[16]; int m = takeEvents(ev);
    for (int i = 0; i < m; i++) {
      int k = navOf(ev[i]);
      if (k == 1 && sel > 0) sel--;
      else if (k == 2 && sel < n - 1) sel++;
      else if (k == 5) sel = imax(0, sel - 10);
      else if (k == 6) sel = imax(0, imin(n - 1, sel + 10));
      else if (ev[i].c == '`' || ev[i].c == '\t') return;
    }
    if (sel < top) top = sel;
    if (sel >= top + 10) top = sel - 9;
    cv.fillScreen(BG);
    char t[24]; clipTo(t, title, 20);
    char rt[16]; snprintf(rt, sizeof(rt), "%d items", n);
    header(t, rt);
    for (int r = 0; r < 10 && top + r < n; r++) {
      int y = 15 + r * 10; bool s = (top + r == sel);
      if (s) cv.fillRect(0, y - 1, SW, 10, FG);
      char nm[40]; clipTo(nm, zn[top + r], 30);
      ltxt(nm, 3, y, s ? BG : FG);
      char sz[16]; fmtSize(sz, sizeof(sz), zs[top + r]);
      rtxt(sz, SW - 3, y, s ? BG : DIM);
    }
    footer("; . [ ] scroll          ` back");
    present(); delay(2);
  }
}
static void exportIndex() {
  SD.mkdir("/FREEzzz");
  File f = SD.open("/FREEzzz/archive_index.txt", FILE_WRITE);
  if (!f) { toast("EXPORT FAILED"); return; }
  char line[200];
  for (int i = 0; i < nArc; i++) {
    snprintf(line, sizeof(line), "%s\t%u\t%s\n", ARCK[arcs[i].kind], (unsigned)arcs[i].size, arcs[i].path);
    f.print(line);
    if (arcs[i].kind == 0 || arcs[i].kind == 1) {
      int c = arcs[i].kind == 0 ? zipList(arcs[i].path) : tarList(arcs[i].path);
      for (int k = 0; k < c && k < 100; k++) { snprintf(line, sizeof(line), "  - %s\t%u\n", zn[k], (unsigned)zs[k]); f.print(line); }
    }
  }
  f.close();
  toast("SAVED /FREEzzz/archive_index.txt");
}
static void archiveIndex() {
  nArc = 0; arcScanned = 0; arcCancel = false;
  arcWalk("/", 0);
  int sel = 0, top = 0;
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    bool stop = false;
    for (int i = 0; i < n && !stop; i++) {
      int k = navOf(ev[i]); char c = ev[i].lc;
      if (k == 1 && sel > 0) sel--;
      else if (k == 2 && sel < nArc - 1) sel++;
      else if (k == 5) sel = imax(0, sel - 8);
      else if (k == 6) sel = imax(0, imin(nArc - 1, sel + 8));
      else if (ev[i].c == '`' || ev[i].c == '\t') return;
      else if (c == 'e') { exportIndex(); stop = true; }
      else if (c == 'r') { nArc = 0; arcScanned = 0; arcCancel = false; arcWalk("/", 0); sel = 0; stop = true; }
      else if (ev[i].c == '\n' && nArc > 0) {
        Arc& a = arcs[sel];
        if (a.kind == 0 || a.kind == 1) {
          int cnt = a.kind == 0 ? zipList(a.path) : tarList(a.path);
          if (cnt < 0) toast("CANNOT READ ARCHIVE");
          else if (cnt == 0) toast("ARCHIVE IS EMPTY");
          else showEntries(baseName(a.path), cnt);
        } else if (a.kind == 2) {
          File f = SD.open(a.path, FILE_READ);
          if (f && f.size() > 4) {
            uint8_t t[4]; f.seek(f.size() - 4); f.read(t, 4);
            char l[40]; snprintf(l, sizeof(l), "unpacked size ~%u bytes", (unsigned)le32(t));
            f.close(); msgBox("GZIP ARCHIVE", l);
          } else { if (f) f.close(); toast("CANNOT READ"); }
        } else if (a.kind == 5) fileInfo(a.path);
        else msgBox("LISTING NOT SUPPORTED", "use a PC for this format");
        stop = true;
      }
    }
    if (sel < top) top = sel;
    if (sel >= top + 9) top = sel - 8;
    cv.fillScreen(BG);
    char rt[24]; snprintf(rt, sizeof(rt), "%d found%s", nArc, arcCancel ? "*" : "");
    header("ARCHIVE INDEX", rt);
    if (nArc == 0) ctxt("no archives or .bin files", 120, 50, DIM);
    for (int r = 0; r < 9 && top + r < nArc; r++) {
      Arc& a = arcs[top + r];
      int y = 15 + r * 10; bool s = (top + r == sel);
      if (s) cv.fillRect(0, y - 1, SW, 10, FG);
      char nm[48]; snprintf(nm, sizeof(nm), "%s %s", ARCK[a.kind], baseName(a.path));
      char t[40]; clipTo(t, nm, 31);
      ltxt(t, 3, y, s ? BG : FG);
      char sz[16]; fmtSize(sz, sizeof(sz), a.size);
      rtxt(sz, SW - 3, y, s ? BG : DIM);
    }
    if (nArc > 0) { char t[40]; clipTo(t, arcs[sel].path, 38, true); ltxt(t, 3, 108, DIM); }
    footer("ENTER open  E export  R rescan  ` exit");
    present(); delay(2);
  }
}

// ============================== SEARCH =============================
struct SRes { char path[100]; char snip[36]; };
static SRes sres[80];
static int nres = 0, scanned = 0, scMode = 0;
static bool scCancel = false;
static char sq[32] = "";
static void addRes(const char* p, const char* snip) {
  if (nres >= 80) return;
  strncpy(sres[nres].path, p, 99); sres[nres].path[99] = 0;
  strncpy(sres[nres].snip, snip, 35); sres[nres].snip[35] = 0;
  nres++;
}
static void contentSearch(const char* p) {
  File f = SD.open(p, FILE_READ);
  if (!f) return;
  int qn = (int)strlen(sq);
  uint32_t pos = 0, sz = f.size();
  while (pos < sz) {
    f.seek(pos);
    int rd = f.read(ioBuf, 1024);
    if (rd <= 0) break;
    int idx = ciFind((const char*)ioBuf, rd, sq);
    if (idx >= 0) {
      char sn[36]; int o = 0;
      for (int i = idx; i < rd && o < 35; i++) { char c = (char)ioBuf[i]; if (c == '\n' || c == '\r') break; sn[o++] = (c >= 32 && c < 127) ? c : '.'; }
      sn[o] = 0;
      addRes(p, sn); break;
    }
    if (rd <= qn) break;
    pos += rd - qn + 1;
  }
  f.close();
}
static void scWalk(const char* dir, int depth) {
  if (scCancel || nres >= 80 || depth > 7) return;
  File d = SD.open(dir);
  if (!d) return;
  File e;
  while (!scCancel && nres < 80 && (e = d.openNextFile())) {
    char nm[64]; strncpy(nm, baseName(e.name()), 63); nm[63] = 0;
    bool isd = e.isDirectory(); uint32_t sz = e.size();
    e.close();
    char p[128]; joinPath(p, sizeof(p), dir, nm);
    scanned++;
    if (scanned % 20 == 0) {
      char l[40]; snprintf(l, sizeof(l), "%d scanned, %d found", scanned, nres);
      progressBox("SEARCHING", p, l);
      if (cancelKey()) scCancel = true;
    }
    if (isd) { if (strcmp(nm, "System Volume Information")) scWalk(p, depth + 1); continue; }
    if (scMode == 0) { if (ciFind(nm, (int)strlen(nm), sq) >= 0) addRes(p, ""); }
    else if (isTextName(nm) && sz <= 300000) contentSearch(p);
  }
  d.close();
}
static void searchModule() {
  static const char* it[] = {"BY FILE NAME", "BY TEXT INSIDE FILES"};
  int m = popup("SEARCH", it, 2);
  if (m < 0) return;
  scMode = m;
  if (!inputText(m == 0 ? "NAME CONTAINS" : "TEXT TO FIND", sq, 28)) return;
  nres = 0; scanned = 0; scCancel = false;
  scWalk("/", 0);
  int sel = 0, top = 0;
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    bool stop = false;
    for (int i = 0; i < n && !stop; i++) {
      int k = navOf(ev[i]);
      if (k == 1 && sel > 0) sel--;
      else if (k == 2 && sel < nres - 1) sel++;
      else if (k == 5) sel = imax(0, sel - 7);
      else if (k == 6) sel = imax(0, imin(nres - 1, sel + 7));
      else if (ev[i].c == '`' || ev[i].c == '\t') return;
      else if (ev[i].c == '\n' && nres > 0) { openFilePopup(sres[sel].path); stop = true; }
      else if (ev[i].lc == 'f') { if (inputText(m == 0 ? "NAME CONTAINS" : "TEXT TO FIND", sq, 28)) { nres = 0; scanned = 0; scCancel = false; scWalk("/", 0); sel = 0; } stop = true; }
    }
    if (sel < top) top = sel;
    if (sel >= top + 7) top = sel - 6;
    cv.fillScreen(BG);
    char t[24]; snprintf(t, sizeof(t), "FOUND %d%s", nres, nres >= 80 ? "+" : "");
    header(t, sq);
    if (nres == 0) ctxt("nothing found", 120, 50, DIM);
    for (int r = 0; r < 7 && top + r < nres; r++) {
      int y = 15 + r * 14; bool s = (top + r == sel);
      if (s) cv.fillRect(0, y - 1, SW, 14, FG);
      char a[44]; clipTo(a, sres[top + r].path, 38, true);
      ltxt(a, 3, y, s ? BG : FG);
      if (sres[top + r].snip[0]) { char b[40]; clipTo(b, sres[top + r].snip, 38); ltxt(b, 3, y + 7, s ? BG : DIM); }
    }
    footer("ENTER open   F new search   ` exit");
    present(); delay(2);
  }
}

// ============================== SD DIAGNOSTICS =====================
static void diagInfo() {
  progressBox("SD CARD", "reading card info...", "");
  pgClear();
  uint8_t t = SD.cardType();
  pgAdd("TYPE: %s", t == CARD_MMC ? "MMC" : t == CARD_SD ? "SD" : t == CARD_SDHC ? "SDHC/SDXC" : "UNKNOWN");
  char a[16], b[16], c[16];
  uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  fmtSize(a, sizeof(a), SD.cardSize());
  pgAdd("CARD SIZE: %s", a);
  fmtSize(a, sizeof(a), total); fmtSize(b, sizeof(b), used); fmtSize(c, sizeof(c), total > used ? total - used : 0);
  pgAdd("FILESYSTEM: %s", a);
  pgAdd("USED: %s", b);
  pgAdd("FREE: %s", c);
  pgAdd("FREE %%: %d", total ? (int)((total - used) * 100 / total) : 0);
  pager("CARD INFO", "; . scroll         ` back");
}
static void diagSpeed() {
  const char* p = "/FREEzzz/speedtest.tmp";
  progressBox("SPEED TEST", "writing 512 KB...", "");
  File f = SD.open(p, FILE_WRITE);
  if (!f) { msgBox("CANNOT WRITE TEST FILE"); return; }
  uint32_t expect = 0;
  for (int i = 0; i < 4096; i++) { ioBuf[i] = (uint8_t)(i * 7 + 13); expect += ioBuf[i]; }
  uint32_t t0 = millis(); size_t w = 0;
  for (int k = 0; k < 128; k++) w += f.write(ioBuf, 4096);
  f.close();
  uint32_t tw = millis() - t0; if (tw == 0) tw = 1;
  progressBox("SPEED TEST", "reading + verifying...", "");
  f = SD.open(p, FILE_READ);
  uint32_t sum = 0, rd = 0;
  t0 = millis();
  while (f && f.available()) { int n = f.read(ioBuf, 4096); if (n <= 0) break; for (int i = 0; i < n; i++) sum += ioBuf[i]; rd += n; }
  if (f) f.close();
  uint32_t tr = millis() - t0; if (tr == 0) tr = 1;
  SD.remove(p);
  bool ok = (w == 524288 && rd == 524288 && sum == expect * 128);
  pgClear();
  pgAdd("WRITE: 512 KB in %u ms", (unsigned)tw);
  pgAdd("       = %u KB/s", (unsigned)(524288UL / 1024 * 1000 / tw));
  pgAdd("READ:  512 KB in %u ms", (unsigned)tr);
  pgAdd("       = %u KB/s", (unsigned)(524288UL / 1024 * 1000 / tr));
  pgAdd("VERIFY: %s", ok ? "OK - data matches" : "FAILED!");
  pager("SPEED TEST", "; . scroll         ` back");
}
static void diagTree(bool deep) {
  stFiles = stDirs = stBytes = stErr = stCount = 0; stDeep = deep; stCancel = false; stBigSz = 0; stBig[0] = 0;
  treeStats("/", 0);
  pgClear();
  char sz[16]; fmtSize(sz, sizeof(sz), stBytes);
  pgAdd("%s", stCancel ? "CANCELLED - PARTIAL RESULT" : "SCAN COMPLETE");
  pgAdd("FILES: %u   FOLDERS: %u", (unsigned)stFiles, (unsigned)stDirs);
  pgAdd("TOTAL DATA: %s", sz);
  if (stBig[0]) { fmtSize(sz, sizeof(sz), stBigSz); pgAdd("LARGEST: %s", sz); char t[44]; clipTo(t, stBig, 42, true); pgAdd("%s", t); }
  if (deep) pgAdd("READ ERRORS: %u %s", (unsigned)stErr, stErr ? "<-- CHECK CARD" : "(all files readable)");
  pager(deep ? "TREE + READ CHECK" : "TREE SCAN", "; . scroll         ` back");
}
static void sdDiag() {
  static const char* it[] = {"CARD INFO / FREE SPACE", "SPEED + VERIFY TEST", "TREE SCAN (count files)", "READ CHECK (open every file)"};
  while (true) {
    int r = popup("SD DIAGNOSTICS", it, 4);
    if (r < 0) return;
    if (r == 0) diagInfo();
    else if (r == 1) diagSpeed();
    else if (r == 2) diagTree(false);
    else diagTree(true);
  }
}

// ============================== SERIAL TERMINAL ====================
static char tl[32][42];
static int tHead = 0, tCount = 1, tCol = 0;
static void tNewLine() { tHead = (tHead + 1) % 32; tl[tHead][0] = 0; tCol = 0; if (tCount < 32) tCount++; }
static void tAddChar(char c) {
  if (c == '\r') return;
  if (c == '\n') { tNewLine(); return; }
  if (tCol >= 39) tNewLine();
  if (c == '\t') c = ' ';
  if (c >= 32 && c < 127) { tl[tHead][tCol++] = c; tl[tHead][tCol] = 0; }
}
static void tAddStr(const char* s) { while (*s) tAddChar(*s++); }
static void terminal() {
  static const long BAUDS[8] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
  static const char* LINE_END_NAMES[4] = {"NONE", "CR", "LF", "CR+LF"};
  static int baudIdx = 4, lend = 3;
  static bool grove = false, hexv = false, logOn = false;
  File logF;
  for (int i = 0; i < 32; i++) tl[i][0] = 0;
  tHead = 0; tCount = 1; tCol = 0;
  char inp[64] = "";
  auto startPort = [&]() {
    if (grove) { Serial1.end(); Serial1.begin(BAUDS[baudIdx], SERIAL_8N1, RXG, TXG); }
    else { Serial.end(); Serial.begin(BAUDS[baudIdx]); }
  };
  startPort();
  char info[40]; snprintf(info, sizeof(info), "-- %s %ld 8N1 --", grove ? "GROVE" : "USB", BAUDS[baudIdx]);
  tAddStr(info); tNewLine();
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    bool exitT = false;
    for (int i = 0; i < n && !exitT; i++) {
      char c = ev[i].c;
      if (c == '\n') {
        Stream& S = grove ? (Stream&)Serial1 : (Stream&)Serial;
        S.print(inp);
        if (lend == 1 || lend == 3) S.write('\r');
        if (lend == 2 || lend == 3) S.write('\n');
        char echo[70]; snprintf(echo, sizeof(echo), "> %s", inp);
        if (tCol > 0) tNewLine();
        tAddStr(echo); tNewLine();
        if (logOn && logF) { logF.print("> "); logF.println(inp); }
        inp[0] = 0;
      } else if (c == '\b') { int l = (int)strlen(inp); if (l) inp[l - 1] = 0; }
      else if (c == '\t') {
        char m[7][34];
        snprintf(m[0], 34, "PORT: %s", grove ? "GROVE UART" : "USB");
        snprintf(m[1], 34, "BAUD: %ld", BAUDS[baudIdx]);
        snprintf(m[2], 34, "LINE END: %s", LINE_END_NAMES[lend]);
        snprintf(m[3], 34, "HEX VIEW: %s", hexv ? "ON" : "OFF");
        snprintf(m[4], 34, "LOG TO SD: %s", logOn ? "ON" : "OFF");
        snprintf(m[5], 34, "CLEAR SCREEN");
        snprintf(m[6], 34, "EXIT TERMINAL");
        const char* ptr[7] = {m[0], m[1], m[2], m[3], m[4], m[5], m[6]};
        int r = popup("TERMINAL MENU", ptr, 7);
        if (r == 0) { grove = !grove; startPort(); }
        else if (r == 1) { baudIdx = (baudIdx + 1) % 8; startPort(); }
        else if (r == 2) lend = (lend + 1) % 4;
        else if (r == 3) hexv = !hexv;
        else if (r == 4) {
          logOn = !logOn;
          if (logOn) {
            SD.mkdir("/FREEzzz/logs");
            char fn[48];
            for (int k = 1; k < 1000; k++) { snprintf(fn, sizeof(fn), "/FREEzzz/logs/term_%03d.log", k); if (!SD.exists(fn)) break; }
            logF = SD.open(fn, FILE_WRITE);
            if (logF) { char t[48]; snprintf(t, sizeof(t), "LOG: %s", baseName(fn)); toast(t); } else { logOn = false; toast("LOG OPEN FAILED"); }
          } else if (logF) { logF.close(); toast("LOG SAVED"); }
        }
        else if (r == 5) { for (int k = 0; k < 32; k++) tl[k][0] = 0; tHead = 0; tCount = 1; tCol = 0; }
        else if (r == 6) exitT = true;
        break;
      }
      else if (c >= 32 && c < 127 && strlen(inp) < 60) { int l = (int)strlen(inp); inp[l] = c; inp[l + 1] = 0; }
    }
    if (exitT) break;
    // receive
    Stream& S = grove ? (Stream&)Serial1 : (Stream&)Serial;
    int got = 0;
    while (S.available() && got < 256) {
      int b = S.read(); got++;
      if (b < 0) break;
      if (hexv) { char h[4]; snprintf(h, sizeof(h), "%02X ", b); tAddStr(h); }
      else tAddChar((char)b);
      if (logOn && logF) logF.write((uint8_t)b);
    }
    if (got && logOn && logF) logF.flush();
    cv.fillScreen(BG);
    char hd[32]; snprintf(hd, sizeof(hd), "TERM %s %ld", grove ? "GROVE" : "USB", BAUDS[baudIdx]);
    char rt[16]; snprintf(rt, sizeof(rt), "%s%s", hexv ? "HEX " : "", logOn ? "LOG" : "");
    header(hd, rt);
    for (int r = 0; r < 11; r++) {
      int li = (tHead - (10 - r) + 64) % 32;
      if (10 - r >= tCount) continue;
      ltxt(tl[li], 2, 14 + r * 9, FG);
    }
    cv.drawFastHLine(0, 113, SW, DIM2);
    char il[48]; snprintf(il, sizeof(il), "> %s%s", strlen(inp) > 36 ? inp + strlen(inp) - 36 : inp, ((millis() / 400) & 1) ? "_" : " ");
    ltxt(il, 2, 115, FG);
    footer("ENTER send   TAB menu (port/baud/log)");
    present(); delay(2);
  }
  if (logF) logF.close();
}

// ============================== BACKUP =============================
static const char* BKROOT = "/FREEzzz/backup";
static char bkNames[30][24];
static int bkN = 0;
static void listBackups() {
  bkN = 0;
  File d = SD.open(BKROOT);
  if (!d) return;
  File e;
  while (bkN < 30 && (e = d.openNextFile())) {
    if (e.isDirectory()) { strncpy(bkNames[bkN], baseName(e.name()), 23); bkNames[bkN][23] = 0; bkN++; }
    e.close();
  }
  d.close();
  for (int i = 1; i < bkN; i++) {
    char t[24]; strcpy(t, bkNames[i]); int j = i - 1;
    while (j >= 0 && strcmp(bkNames[j], t) > 0) { strcpy(bkNames[j + 1], bkNames[j]); j--; }
    strcpy(bkNames[j + 1], t);
  }
}
static int pickBackup(const char* title) {
  listBackups();
  if (bkN == 0) { msgBox("NO BACKUPS YET"); return -1; }
  const char* ptr[30];
  for (int i = 0; i < bkN; i++) ptr[i] = bkNames[i];
  return popup(title, ptr, imin(bkN, 8), 0);
}
static void showBkResult(const char* what) {
  pgClear();
  char sz[16]; fmtSize(sz, sizeof(sz), bkBytes);
  pgAdd("%s", bkCancel ? "CANCELLED (partial)" : what);
  pgAdd("FILES COPIED: %d", bkFiles);
  pgAdd("DATA: %s", sz);
  pgAdd("FAILED: %d", bkFail);
  pager("BACKUP", "; . scroll         ` back");
}
static void backupModule() {
  static const char* it[] = {"BACKUP A FOLDER", "RESTORE A BACKUP", "DELETE A BACKUP", "LIST BACKUPS"};
  while (true) {
    int r = popup("BACKUP", it, 4);
    if (r < 0) return;
    SD.mkdir("/FREEzzz"); SD.mkdir(BKROOT);
    if (r == 0) {
      char src[128] = "";
      if (!browse(2, src, sizeof(src))) continue;
      if (!strncmp(src, "/FREEzzz/backup", 15)) { msgBox("CANNOT BACK UP A BACKUP"); continue; }
      int num = 1;
      listBackups();
      for (int i = 0; i < bkN; i++) { int v = atoi(bkNames[i] + 3); if (v >= num) num = v + 1; }
      char dst[64]; snprintf(dst, sizeof(dst), "%s/BK_%03d", BKROOT, num);
      char m[60]; snprintf(m, sizeof(m), "BACKUP %s ?", src);
      if (!confirmBox(m)) continue;
      SD.mkdir(dst);
      char mf[80]; snprintf(mf, sizeof(mf), "%s/_source.txt", dst);
      File f = SD.open(mf, FILE_WRITE);
      if (f) { f.print(src); f.close(); }
      char data[80]; snprintf(data, sizeof(data), "%s/data", dst);
      bkFiles = 0; bkFail = 0; bkBytes = 0; bkCancel = false;
      copyTree(src, data);
      showBkResult("BACKUP COMPLETE");
    } else if (r == 1) {
      int s = pickBackup("RESTORE WHICH?"); if (s < 0) continue;
      char mf[80]; snprintf(mf, sizeof(mf), "%s/%s/_source.txt", BKROOT, bkNames[s]);
      char target[128] = "";
      File f = SD.open(mf, FILE_READ);
      if (f) { int n = f.read((uint8_t*)target, 127); target[n < 0 ? 0 : n] = 0; f.close(); }
      if (!target[0]) { msgBox("BACKUP HAS NO SOURCE INFO"); continue; }
      char m[60]; snprintf(m, sizeof(m), "RESTORE TO %s ?", target);
      if (!confirmBox(m)) continue;
      char data[80]; snprintf(data, sizeof(data), "%s/%s/data", BKROOT, bkNames[s]);
      bkFiles = 0; bkFail = 0; bkBytes = 0; bkCancel = false;
      copyTree(data, target);
      showBkResult("RESTORE COMPLETE");
    } else if (r == 2) {
      int s = pickBackup("DELETE WHICH?"); if (s < 0) continue;
      char m[60]; snprintf(m, sizeof(m), "DELETE %s ?", bkNames[s]);
      if (confirmBox(m)) { char p[64]; snprintf(p, sizeof(p), "%s/%s", BKROOT, bkNames[s]); toast(removeTree(p) ? "BACKUP DELETED" : "FAILED"); }
    } else {
      listBackups();
      pgClear();
      if (bkN == 0) pgAdd("no backups yet");
      for (int i = 0; i < bkN && i < 38; i++) {
        char mf[80]; snprintf(mf, sizeof(mf), "%s/%s/_source.txt", BKROOT, bkNames[i]);
        char src[40] = ""; File f = SD.open(mf, FILE_READ);
        if (f) { int n = f.read((uint8_t*)src, 30); src[n < 0 ? 0 : n] = 0; f.close(); }
        pgAdd("%s  %s", bkNames[i], src);
      }
      pager("BACKUPS", "; . scroll         ` back");
    }
  }
}

// ============================== SYSTEM INFO ========================
static void sysInfo() {
  int page = 0;
  while (true) {
    pgClear();
    if (page == 0) {
      char a[16];
      pgAdd("CHIP: %s rev %d, %d cores", ESP.getChipModel(), (int)ESP.getChipRevision(), (int)ESP.getChipCores());
      pgAdd("CPU: %d MHz", (int)ESP.getCpuFreqMHz());
      fmtSize(a, sizeof(a), ESP.getFlashChipSize());
      pgAdd("FLASH: %s @ %d MHz", a, (int)(ESP.getFlashChipSpeed() / 1000000));
      pgAdd("HEAP: %u free / %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getHeapSize() / 1024));
      pgAdd("HEAP MIN FREE: %u KB", (unsigned)(ESP.getMinFreeHeap() / 1024));
      pgAdd("MAX BLOCK: %u KB", (unsigned)(ESP.getMaxAllocHeap() / 1024));
      pgAdd("PSRAM: %u KB", (unsigned)(ESP.getPsramSize() / 1024));
      fmtSize(a, sizeof(a), ESP.getSketchSize());
      pgAdd("THIS APP: %s", a);
      fmtSize(a, sizeof(a), ESP.getFreeSketchSpace());
      pgAdd("FREE OTA SPACE: %s", a);
      pgAdd("BATTERY: %d%%  %d mV", (int)M5Cardputer.Power.getBatteryLevel(), (int)M5Cardputer.Power.getBatteryVoltage());
      uint32_t s = millis() / 1000;
      pgAdd("UPTIME: %uh %um %us", (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
      uint64_t mac = ESP.getEfuseMac();
      pgAdd("MAC: %02X:%02X:%02X:%02X:%02X:%02X", (unsigned)((mac >> 0) & 255), (unsigned)((mac >> 8) & 255), (unsigned)((mac >> 16) & 255),
            (unsigned)((mac >> 24) & 255), (unsigned)((mac >> 32) & 255), (unsigned)((mac >> 40) & 255));
      pgAdd("SDK: %s", ESP.getSdkVersion());
      pgAdd("SD CARD: %s", sdOk ? "mounted" : "not found");
    } else {
      const esp_partition_t* run = esp_ota_get_running_partition();
      pgAdd("PARTITIONS (* = running)");
      esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
      while (it && pgN < 38) {
        const esp_partition_t* p = esp_partition_get(it);
        pgAdd("%c%-8s %02X/%02X @%06X %uK", (run && p->address == run->address) ? '*' : ' ', p->label,
              (unsigned)p->type, (unsigned)p->subtype, (unsigned)p->address, (unsigned)(p->size / 1024));
        it = esp_partition_next(it);
      }
    }
    // single-page pager with page switching
    int top = 0; bool leave = false, flip = false;
    while (!leave && !flip) {
      pollKeys();
      Ev ev[16]; int n = takeEvents(ev);
      for (int i = 0; i < n; i++) {
        int k = navOf(ev[i]);
        if (k == 1 && top > 0) top--;
        else if (k == 2 && top < pgN - 12) top++;
        else if (k == 3 || k == 4) flip = true;
        else if (ev[i].c == '`' || ev[i].c == '\t') leave = true;
      }
      cv.fillScreen(BG);
      header(page == 0 ? "SYSTEM INFO 1/2" : "SYSTEM INFO 2/2");
      for (int r = 0; r < 12 && top + r < pgN; r++) ltxt(pg[top + r], 4, 15 + r * 9, FG);
      footer(", / page      ; . scroll      ` back");
      present(); delay(2);
    }
    if (leave) return;
    page = 1 - page;
  }
}

// ============================== MAIN MENU ==========================
static const char* MENU[10] = {"FILE MANAGER", "TEXT EDITOR", "HEX EDITOR", "FILE VIEWER", "ARCHIVE INDEX",
                               "SEARCH", "SD DIAGNOSTICS", "SERIAL TERMINAL", "BACKUP", "SYSTEM INFO"};
static void needSd() { msgBox("NO SD CARD FOUND", "insert a FAT32 card, restart"); }

static void runModule(int i) {
  char path[160];
  bool needs = (i != 7 && i != 9);
  if (needs && !sdOk) { needSd(); return; }
  switch (i) {
    case 0: browse(0, nullptr, 0); break;
    case 1: {
      static const char* it[] = {"OPEN A FILE", "NEW FILE (current folder)"};
      int r = popup("TEXT EDITOR", it, 2);
      if (r == 0) { if (browse(1, path, sizeof(path))) editFile(path); }
      else if (r == 1) {
        char nm[48] = "note.txt";
        if (inputText("NEW FILE NAME", nm, 40)) {
          joinPath(path, sizeof(path), cwd, nm);
          if (!SD.exists(path)) { File t = SD.open(path, FILE_WRITE); if (t) t.close(); }
          editFile(path);
        }
      }
      break;
    }
    case 2: if (browse(1, path, sizeof(path))) hexFile(path, true); break;
    case 3: if (browse(1, path, sizeof(path))) viewFile(path); break;
    case 4: archiveIndex(); break;
    case 5: searchModule(); break;
    case 6: sdDiag(); break;
    case 7: terminal(); break;
    case 8: backupModule(); break;
    case 9: sysInfo(); break;
  }
}

static void mainMenu() {
  int sel = 0;
  char sdLine[24];
  if (sdOk) { char z[16]; fmtSize(z, sizeof(z), SD.cardSize()); snprintf(sdLine, sizeof(sdLine), "SD %s", z); }
  else snprintf(sdLine, sizeof(sdLine), "NO SD");
  while (true) {
    pollKeys();
    Ev ev[16]; int n = takeEvents(ev);
    for (int i = 0; i < n; i++) {
      int k = navOf(ev[i]); char c = ev[i].c;
      if (k == 1) { sel = (sel + 9) % 10; beep(); }
      else if (k == 2) { sel = (sel + 1) % 10; beep(); }
      else if (c == '\n') { runModule(sel); break; }
      else if (c >= '1' && c <= '9') { sel = c - '1'; runModule(sel); break; }
      else if (c == '0') { sel = 9; runModule(9); break; }
      else if (ev[i].lc == 'c') { colorIdx = (colorIdx + 1) % 6; applyColor(); prefs.putUChar("col", colorIdx); }
      else if (ev[i].lc == 'm') { sndOn = !sndOn; applyVolume(); prefs.putBool("snd", sndOn); toast(sndOn ? "SOUND ON" : "SOUND OFF"); }
    }
    cv.fillScreen(BG);
    header("FREEzzz FILES", sdLine);
    for (int i = 0; i < 10; i++) {
      int y = 15 + i * 10;
      char t[32]; snprintf(t, sizeof(t), "%02d  %s", i + 1, MENU[i]);
      if (i == sel) { cv.fillRect(0, y - 1, SW, 10, FG); ltxt(t, 10, y, BG); }
      else ltxt(t, 10, y, FG);
    }
    char ft[44]; snprintf(ft, sizeof(ft), "; . ENTER  1-0 jump  C %s  M snd", PALN[colorIdx]);
    footer(ft);
    present(); delay(2);
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(0);
  cv.setColorDepth(16);
  cv.createSprite(SW, SH);
  prefs.begin("freezzz", false);
  colorIdx = prefs.getUChar("col", 0) % 6;
  sndOn = prefs.getBool("snd", true);
  applyColor();
  applyVolume();
  cv.fillScreen(BG);
  ctxt("FREEzzz FILES", 120, 52, FG);
  ctxt("mounting SD card...", 120, 70, DIM);
  cv.pushSprite(0, 0);
  SPI.begin(40, 39, 14, 12);
  sdOk = SD.begin(12, SPI, 25000000);
  if (sdOk) { SD.mkdir("/FREEzzz"); SD.mkdir("/FREEzzz/logs"); SD.mkdir("/FREEzzz/backup"); }
}

void loop() {
  mainMenu();
}