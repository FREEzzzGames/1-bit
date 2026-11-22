from pathlib import Path

p = Path("build-src/CardputerClock/CardputerClock.ino")
s = p.read_text()

def once(old, new, label):
    global s
    if old not in s:
        raise SystemExit("PATCH ANCHOR NOT FOUND: " + label)
    s = s.replace(old, new, 1)

once('String gifPath = "", ssid = "", wpass = "";',
     'String gifPath = "", gifDir = "/gifs", ssid = "", wpass = "";\nbool setupDone = false, setupWizard = false;\nstd::vector<String> wifiList, dirList;\nString browseDir = "/"; int browseCur = 0;',
     'config globals')
once('f.print("gif="); f.println(gifPath);',
     'f.print("gif="); f.println(gifPath); f.print("gifDir="); f.println(gifDir); f.print("setupDone="); f.println(setupDone ? 1 : 0);',
     'save config')
once('else if (k == "gif") gifPath = v;',
     'else if (k == "gif") gifPath = v;\n    else if (k == "gifDir") gifDir = v;\n    else if (k == "setupDone") setupDone = v.toInt() != 0;',
     'load config')
once('void scanGifs() {\n  gifList.clear(); gifCached.clear();\n  if (!sdOK) return;\n  scanDir("/gifs"); scanDir("/");',
     'void scanGifs() {\n  gifList.clear(); gifCached.clear();\n  if (!sdOK) return;\n  // Search the selected folder plus both standard locations.\n  scanDir(gifDir.c_str());\n  if (gifDir != "/gifs") scanDir("/gifs");\n  if (gifDir != "/") scanDir("/");\n  // Avoid duplicate entries when a standard folder is selected.\n  for (size_t i = 0; i < gifList.size(); ++i) {\n    for (size_t j = i + 1; j < gifList.size();) {\n      if (gifList[i] == gifList[j]) gifList.erase(gifList.begin() + j);\n      else ++j;\n    }\n  }',
     'scan selected and standard folders')
once('enum { M_NORMAL, M_TEXT, M_PICK };',
     'enum { M_NORMAL, M_TEXT, M_PICK, M_SETUP, M_WIFI, M_DIR };',
     'modes')
# Add menu drawing and directory scanning helpers before drawRing
anchor = 'void drawRing() {'
insert = r'''
void scanSubdirs() {
  dirList.clear();
  File d = SD.open(browseDir.c_str());
  if (!d || !d.isDirectory()) { if (d) d.close(); return; }
  if (browseDir != "/") dirList.push_back("..");
  File e = d.openNextFile();
  while (e && dirList.size() < 48) {
    if (e.isDirectory()) {
      String nm = baseName(String(e.name()));
      if (nm.length() && nm[0] != '.') dirList.push_back(nm);
    }
    e.close(); e = d.openNextFile();
  }
  if (e) e.close();
  d.close();
  std::sort(dirList.begin(), dirList.end());
  browseCur = 0;
}
String joinDir(const String& a, const String& b) {
  if (b == "..") { int k = a.lastIndexOf('/'); return k <= 0 ? String("/") : a.substring(0, k); }
  return a == "/" ? String("/") + b : a + "/" + b;
}
void drawSetupMenu() {
  txt("FIRST START", 120, 5, cOn, &fonts::Font2, D_TC);
  const char* labels[5] = {"1  WIFI NETWORK", "2  GIF FOLDER", "3  DATE / TIME", "4  MANUAL SETTINGS", "5  FINISH"};
  for (int i = 0; i < 5; i++) {
    int y = 27 + i * 18;
    if (i == pickCur) S.fillRect(6, y - 1, 228, 17, cSel);
    txt(labels[i], 14, y, i == pickCur ? cOn : cMid, &fonts::Font2);
  }
  txt(";. SELECT   ENTER=OPEN", 120, 124, cMid, &fonts::Font0, D_TC);
}
void drawWifiList() {
  txt("SELECT WIFI", 120, 4, cOn, &fonts::Font2, D_TC);
  int top = constrain(browseCur - 3, 0, max(0, (int)wifiList.size() - 6));
  for (int r = 0; r < 6 && top + r < (int)wifiList.size(); r++) {
    int i = top + r, y = 22 + r * 17;
    if (i == browseCur) S.fillRect(6, y - 1, 228, 17, cSel);
    txt(fit(wifiList[i], 29), 12, y, i == browseCur ? cOn : cMid, &fonts::Font2);
  }
  if (wifiList.empty()) txt("NO NETWORKS FOUND", 120, 70, cOn, &fonts::Font2, D_MC);
  txt(";. SELECT  ENTER=CONNECT  DEL=BACK", 120, 124, cMid, &fonts::Font0, D_TC);
}
void drawDirList() {
  txt(fit(String("GIF FOLDER ") + browseDir, 32), 120, 4, cOn, &fonts::Font2, D_TC);
  int top = constrain(browseCur - 3, 0, max(0, (int)dirList.size() - 6));
  for (int r = 0; r < 6 && top + r < (int)dirList.size(); r++) {
    int i = top + r, y = 22 + r * 17;
    if (i == browseCur) S.fillRect(6, y - 1, 228, 17, cSel);
    String nm = dirList[i] == ".." ? String("[..]") : String("[") + dirList[i] + "]";
    txt(fit(nm, 30), 12, y, i == browseCur ? cOn : cMid, &fonts::Font2);
  }
  txt("ENTER=OPEN   S=USE THIS FOLDER   DEL=BACK", 120, 124, cMid, &fonts::Font0, D_TC);
}
'''
once(anchor, insert + '\n' + anchor, 'setup UI functions')
# Render mode dispatch
once('if (mode == M_PICK) drawPicker();\n  else if (mode == M_TEXT) drawTextInput();',
     'if (mode == M_PICK) drawPicker();\n  else if (mode == M_TEXT) drawTextInput();\n  else if (mode == M_SETUP) drawSetupMenu();\n  else if (mode == M_WIFI) drawWifiList();\n  else if (mode == M_DIR) drawDirList();',
     'render modes')
# Extend input handling after key vector setup and before M_TEXT
anchor = '  if (mode == M_TEXT) {'
insert = r'''
  if (mode == M_SETUP) {
    if (st.tab || st.del) { mode = M_NORMAL; setupWizard = false; return; }
    for (char c : ks) {
      if (c == ';') pickCur = (pickCur + 4) % 5;
      else if (c == '.') pickCur = (pickCur + 1) % 5;
    }
    if (st.enter) {
      if (pickCur == 0) {
        wifiList.clear(); WiFi.mode(WIFI_STA);
        int n = WiFi.scanNetworks(false, true);
        for (int i = 0; i < n && wifiList.size() < 32; i++) {
          String name = WiFi.SSID(i); if (name.length()) {
            bool dup = false; for (auto &v : wifiList) if (v == name) dup = true;
            if (!dup) wifiList.push_back(name);
          }
        }
        WiFi.scanDelete(); WiFi.mode(WIFI_OFF); browseCur = 0; mode = M_WIFI;
      } else if (pickCur == 1) {
        browseDir = "/"; scanSubdirs(); mode = M_DIR;
      } else if (pickCur == 2) { startText(2); }
      else if (pickCur == 3) { setupWizard = true; screen = SC_SET; sCur = 0; mode = M_NORMAL; }
      else {
        setupDone = true; setupWizard = false; saveCfg(); mode = M_NORMAL; screen = SC_CLOCK;
      }
    }
    return;
  }
  if (mode == M_WIFI) {
    if (st.del || st.tab) { mode = M_SETUP; return; }
    for (char c : ks) {
      if (c == ';' && !wifiList.empty()) browseCur = (browseCur + wifiList.size() - 1) % wifiList.size();
      else if (c == '.' && !wifiList.empty()) browseCur = (browseCur + 1) % wifiList.size();
    }
    if (st.enter && !wifiList.empty()) {
      ssid = wifiList[browseCur]; wpass = ""; markCfg(); startText(1);
    }
    return;
  }
  if (mode == M_DIR) {
    if (st.del || st.tab) { mode = M_SETUP; return; }
    for (char c : ks) {
      if (c == ';' && !dirList.empty()) browseCur = (browseCur + dirList.size() - 1) % dirList.size();
      else if (c == '.' && !dirList.empty()) browseCur = (browseCur + 1) % dirList.size();
      else if ((c == 's' || c == 'S')) {
        gifDir = browseDir; scanGifs();
        bool currentFound = false;
        for (const auto& item : gifList) if (item == gifPath) { currentFound = true; break; }
        if (!currentFound) { gifPath = ""; closeBg(); }
        if (!gifList.empty()) { pickCur = 0; mode = M_PICK; }
        else mode = M_SETUP;
        markCfg();
      }
    }
    if (st.enter && !dirList.empty()) { browseDir = joinDir(browseDir, dirList[browseCur]); scanSubdirs(); }
    return;
  }
'''
once(anchor, insert + '\n' + anchor, 'input modes')
# After password input, return to wizard if setup is active
once('else if (inTarget == 1) { wpass = inBuf; markCfg(); }',
     'else if (inTarget == 1) { wpass = inBuf; markCfg(); nextSyncAt = millis(); }',
     'wifi password sync')
once('mode = M_NORMAL; return;\n    }\n    if (st.del)',
     'mode = setupWizard ? M_SETUP : M_NORMAL; return;\n    }\n    if (st.del)',
     'text input return mode')
# Entering picker from settings scans selected folder already; setup's folder select opens GIF list
# After choosing GIF return to setup wizard when in setup context
once('mode = M_NORMAL;\n      gifPath = pickCur == 0 ? String("") : gifList[pickCur - 1];',
     'mode = setupWizard ? M_SETUP : M_NORMAL;\n      gifPath = pickCur == 0 ? String("") : gifList[pickCur - 1];',
     'picker return mode')
# Manual settings exit: TAB/ESC returns to setup while wizard is active; finish wizard via menu
once('if (st.tab) { goScreen(screen + 1); return; }',
     'if (st.tab) { if (setupWizard && screen == SC_SET) { mode = M_SETUP; return; } goScreen(screen + 1); return; }',
     'settings return')
# Setup startup decision after loading config and initializing display
once('loadCfg();\n  applyBacklight(); updateColors();',
     'loadCfg();\n  setupWizard = !setupDone;\n  if (setupWizard) { mode = M_SETUP; pickCur = 0; }\n  applyBacklight(); updateColors();',
     'first boot wizard')
# Settings menu can be opened any time; existing settings navigation remains
# Save directory and first-start state in config.
p.write_text(s)


# Reliability pass: replace fragile input paths and fix storage/GIF edge cases.
def replace_between(start, end, replacement, label):
    global s
    a = s.find(start)
    if a < 0: raise SystemExit("PATCH START NOT FOUND: " + label)
    b = s.find(end, a + len(start))
    if b < 0: raise SystemExit("PATCH END NOT FOUND: " + label)
    s = s[:a] + replacement + s[b:]

replace_between("void saveCfg() {", "void loadCfg() {", r'''void saveCfg() {
  if (!sdOK) { cfgDirty = true; return; }
  SD.mkdir("/clock");
  const char* tmp = "/clock/clock.tmp";
  const char* bak = "/clock/clock.bak";
  SD.remove(tmp);
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) { cfgDirty = true; return; }
  f.printf("dialColor=%d\n", dialColor); f.printf("dialBr=%d\n", dialBr);
  f.print("gif="); f.println(gifPath);
  f.print("gifDir="); f.println(gifDir);
  f.printf("setupDone=%d\n", setupDone ? 1 : 0);
  f.printf("gifTint=%d\n", gifTint); f.printf("gifBr=%d\n", gifBr);
  f.printf("glass=%d\n", glassLvl); f.printf("backlight=%d\n", backlight);
  f.printf("h24=%d\n", h24); f.printf("autoTz=%d\n", autoTz); f.printf("tzMin=%d\n", tzMin);
  f.print("ssid="); f.println(ssid);
  f.print("pass="); f.println(wpass);
  for (int i = 0; i < 4; i++) f.printf("al%d=%d,%d,%d,%d\n", i, al[i].h, al[i].m, al[i].on, al[i].rep);
  time_t n = time(nullptr);
  f.printf("epoch=%lu\n", (unsigned long)(n > 1700000000 ? n : 0));
  f.flush(); f.close();
  File check = SD.open(tmp, FILE_READ);
  bool valid = check && check.size() >= 16;
  if (check) check.close();
  if (!valid) { SD.remove(tmp); cfgDirty = true; return; }
  SD.remove(bak);
  bool hadOld = SD.exists(CFG);
  if (hadOld && !SD.rename(CFG, bak)) { SD.remove(tmp); cfgDirty = true; return; }
  if (!SD.rename(tmp, CFG)) {
    if (hadOld) SD.rename(bak, CFG);
    cfgDirty = true;
    return;
  }
  SD.remove(bak);
  cfgDirty = false;
}
''', "atomic config save")

once('if ((pFile->iSize - pFile->iPos) < iLen) n = pFile->iSize - pFile->iPos - 1;',
     'if ((pFile->iSize - pFile->iPos) < iLen) n = pFile->iSize - pFile->iPos;',
     'GIF reader EOF off-by-one')

replace_between("  if (mode == M_TEXT) {", "  if (mode == M_PICK) {", r'''  if (mode == M_TEXT) {
    if (st.tab) { mode = setupWizard ? M_SETUP : M_NORMAL; dirty = true; return; }
    if (st.enter) {
      if (inTarget == 0) { ssid = inBuf; markCfg(); nextSyncAt = millis(); }
      else if (inTarget == 1) { wpass = inBuf; markCfg(); nextSyncAt = millis(); }
      else {
        if (inBuf.length() != 12) { syncMsg = "USE DDMMYYYYHHMM"; dirty = true; return; }
        int d = inBuf.substring(0, 2).toInt(), mo = inBuf.substring(2, 4).toInt();
        int y = inBuf.substring(4, 8).toInt(), hh = inBuf.substring(8, 10).toInt();
        int mi = inBuf.substring(10, 12).toInt();
        if (!setLocalTime(y, mo, d, hh, mi)) { syncMsg = "BAD DATE/TIME"; dirty = true; return; }
        saveCfg();
      }
      mode = setupWizard ? M_SETUP : M_NORMAL;
      dirty = true;
      return;
    }
    if (st.del) { if (inBuf.length()) inBuf.remove(inBuf.length() - 1); dirty = true; return; }
    for (char c : ks) {
      if (inTarget == 2) { if (c >= '0' && c <= '9' && inBuf.length() < 12) inBuf += c; }
      else if (c >= 32 && c < 127 && inBuf.length() < 63) inBuf += c;
    }
    dirty = true;
    return;
  }
''', "text input state")

replace_between("  if (mode == M_PICK) {", "  if (st.tab) { if (setupWizard && screen == SC_SET) { mode = M_SETUP; return; } goScreen(screen + 1); return; }", r'''  if (mode == M_PICK) {
    int n = gifList.size() + 1;
    if (st.tab || st.del) { mode = setupWizard ? M_SETUP : M_NORMAL; dirty = true; return; }
    if (st.enter) {
      gifPath = pickCur == 0 ? String("") : gifList[pickCur - 1];
      applyGif(); markCfg();
      mode = setupWizard ? M_SETUP : M_NORMAL;
      dirty = true;
      return;
    }
    for (char c : ks) {
      if (c == ';') pickCur = (pickCur + n - 1) % n;
      else if (c == '.') pickCur = (pickCur + 1) % n;
      else if (c == 96) mode = setupWizard ? M_SETUP : M_NORMAL;
      else if ((c == 'r' || c == 'R') && pickCur > 0) {
        SD.remove(cachePath(gifList[pickCur - 1]).c_str());
        gifCached[pickCur - 1] = 0;
      }
    }
    dirty = true;
    return;
  }

''', "GIF picker state")

replace_between("  if (mode == M_SETUP) {", "  if (mode == M_WIFI) {", r'''  if (mode == M_SETUP) {
    for (char c : ks) {
      if (c == ';') pickCur = (pickCur + 4) % 5;
      else if (c == '.') pickCur = (pickCur + 1) % 5;
    }
    if (st.enter) {
      if (pickCur == 0) {
        wifiList.clear();
        WiFi.mode(WIFI_STA);
        int n = WiFi.scanNetworks(false, true);
        for (int i = 0; i < n && wifiList.size() < 32; i++) {
          String name = WiFi.SSID(i);
          if (!name.length()) continue;
          bool duplicate = false;
          for (const auto& v : wifiList) if (v == name) { duplicate = true; break; }
          if (!duplicate) wifiList.push_back(name);
        }
        WiFi.scanDelete(); WiFi.mode(WIFI_OFF);
        browseCur = 0; mode = M_WIFI;
      } else if (pickCur == 1) {
        browseDir = "/"; scanSubdirs(); mode = M_DIR;
      } else if (pickCur == 2) startText(2);
      else if (pickCur == 3) {
        setupWizard = true; screen = SC_SET; sCur = 0; mode = M_NORMAL;
      } else {
        setupDone = true; setupWizard = false; saveCfg();
        mode = M_NORMAL; screen = SC_CLOCK;
      }
    }
    dirty = true;
    return;
  }
''', "setup menu input")

once("  for (char c : ks) if (c == '" + chr(96) + "') { goScreen(SC_CLOCK); return; }",
     "  for (char c : ks) if (c == 96) { if (setupWizard && screen == SC_SET) { mode = M_SETUP; dirty = true; return; } goScreen(SC_CLOCK); return; }",
     "manual settings back key")

once('  txt(";. SELECT   ENTER=OPEN", 120, 124, cMid, &fonts::Font0, D_TC);',
     '  String status = syncState ? syncMsg : (synced ? "TIME SYNCED" : syncMsg);\n  txt(fit(status, 30), 120, 116, cMid, &fonts::Font0, D_TC);\n  txt(";. SELECT   ENTER=OPEN", 120, 130, cMid, &fonts::Font0, D_TC);',
     "setup status line")
once('  syncMsg = msg; syncState = 0;', '  syncMsg = msg; syncState = 0; dirty = true;', "sync status redraw")

p.write_text(s)


# Final GIF/playback hardening. This phase runs against the pristine sketch
# after the setup overlay has been applied above.
s = p.read_text()

def final_once(old, new, label):
    global s
    if old not in s:
        raise SystemExit("FINAL PATCH ANCHOR NOT FOUND: " + label)
    s = s.replace(old, new, 1)

final_once("static const uint32_t BG_MAXF = 400;",
           "static const uint32_t BG_MAXF = 4096;",
           "larger bounded animation cache")
final_once("uint16_t* cvBuf = nullptr; uint32_t* cvHist = nullptr; uint8_t* cvLut = nullptr;",
           "uint16_t* cvBuf = nullptr; uint16_t* cvRestore = nullptr; uint32_t* cvHist = nullptr; uint8_t* cvLut = nullptr;\n"
           "bool cvPrevValid = false; uint8_t cvPrevDisp = 0; int cvPrevX = 0, cvPrevY = 0, cvPrevW = 0, cvPrevH = 0; uint16_t cvPrevBg = 0;",
           "GIF disposal state")
final_once('String cachePath(const String& src) { return String(CACHE_DIR) + "/" + baseName(src) + ".cbg"; }',
r'''String cachePath(const String& src) {
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < src.length(); ++i) { h ^= (uint8_t)src[i]; h *= 16777619UL; }
  char name[24]; snprintf(name, sizeof(name), "/%08lX.cbg", (unsigned long)h);
  return String(CACHE_DIR) + name;
}''', "collision-safe GIF cache")

def replace_gifdraw():
    global s
    a = s.find("void GIFDraw(GIFDRAW* d) {")
    b = s.find("// ---- converter UI", a)
    if a < 0 or b < 0: raise SystemExit("FINAL PATCH ANCHOR NOT FOUND: GIFDraw")
    replacement = r'''void GIFDraw(GIFDRAW* d) {
  uint8_t* src = d->pPixels;
  uint16_t* pal = d->pPalette;
  bool hasT = d->ucHasTransparency;
  uint8_t tr = d->ucTransparent;

  if (cvPass == 1) {
    if (!cvHist || (d->y & 1)) return;
    for (int x = 0; x < d->iWidth; x++) {
      uint8_t ix = src[x];
      if (hasT && ix == tr) continue;
      uint16_t bin = bin444(pal[ix]);
      if (cvHist[bin] != UINT32_MAX) cvHist[bin]++;
    }
    return;
  }
  if (cvPass != 2 || !cvBuf) return;

  // Dispose the previous image before drawing the next image descriptor.
  if (d->y == 0) {
    if (cvPrevValid) {
      if (cvPrevDisp == 2) {
        for (int dy = 0; dy < SCR_H; ++dy) {
          int sy = syMap[dy];
          if (sy < cvPrevY || sy >= cvPrevY + cvPrevH) continue;
          uint16_t* row = cvBuf + dy * SCR_W;
          for (int dx = 0; dx < SCR_W; ++dx) {
            int sx = sxMap[dx];
            if (sx >= cvPrevX && sx < cvPrevX + cvPrevW) row[dx] = cvPrevBg;
          }
        }
      } else if (cvPrevDisp == 3 && cvRestore) {
        memcpy(cvBuf, cvRestore, SCR_W * SCR_H * sizeof(uint16_t));
      }
    }
    if (d->ucDisposalMethod == 3 && cvRestore)
      memcpy(cvRestore, cvBuf, SCR_W * SCR_H * sizeof(uint16_t));
    cvPrevX = d->iX; cvPrevY = d->iY;
    cvPrevW = d->iWidth; cvPrevH = d->iHeight;
    cvPrevDisp = d->ucDisposalMethod;
    cvPrevBg = pal[d->ucBackground];
    cvPrevValid = true;
  }

  int sy = d->iY + d->y;
  const uint16_t* first = std::lower_bound(syMap, syMap + SCR_H, sy);
  const uint16_t* last = std::upper_bound(syMap, syMap + SCR_H, sy);
  if (first == last) return;
  int dy0 = (int)(first - syMap), dy1 = (int)(last - syMap);
  int x0 = d->iX, x1 = d->iX + d->iWidth;
  for (int dy = dy0; dy < dy1; ++dy) {
    uint16_t* out = cvBuf + dy * SCR_W;
    for (int dx = 0; dx < SCR_W; ++dx) {
      int sx = sxMap[dx];
      if (sx < x0 || sx >= x1) continue;
      uint8_t ix = src[sx - x0];
      if (hasT && ix == tr) continue;
      out[dx] = pal[ix];
    }
  }
}
'''
    s = s[:a] + replacement + s[b:]
replace_gifdraw()

s = s.replace(
'''  uint32_t n = 0, D = 0; int fd = 0, r;''',
'''  uint32_t n = 0; uint64_t D = 0; int fd = 0, r;''', 1)
s = s.replace(
'''  uint32_t avg = n ? D / n : 100;
  uint32_t outMs = avg < 50 ? 50 : (avg > 200 ? 200 : avg);
  uint32_t total = (D + outMs - 1) / outMs;
  if (total > BG_MAXF) {
    outMs = (D + BG_MAXF - 1) / BG_MAXF; if (outMs > 200) outMs = 200;
    total = (D + outMs - 1) / outMs; if (total > BG_MAXF) total = BG_MAXF;
  }
  if (total < 1) total = 1;''',
'''  uint32_t avg = n ? (uint32_t)(D / n) : 100;
  uint32_t outMs = avg < 83 ? 83 : (avg > 200 ? 200 : avg);
  uint64_t total64 = (D + outMs - 1) / outMs;
  if (total64 > BG_MAXF) {
    uint64_t needed = (D + BG_MAXF - 1) / BG_MAXF;
    outMs = (uint32_t)min((uint64_t)65535, needed);
    total64 = (D + outMs - 1) / outMs;
  }
  uint32_t total = (uint32_t)min((uint64_t)BG_MAXF, max((uint64_t)1, total64));''', 1)
s = s.replace(
'''  gif.reset(); cvPass = 2; cvPos = 0;''',
'''  gif.reset(); cvPass = 2; cvPos = 0;
  cvPrevValid = false; cvPrevDisp = 0;''', 1)
s = s.replace(
'''  uint32_t t0 = 0, emitted = 0; bool fail = false;''',
'''  uint64_t t0 = 0; uint32_t emitted = 0; bool fail = false;''', 1)
s = s.replace(
'''    uint32_t t1 = t0 + fd;
    while (emitted < total && emitted * outMs < t1) {''',
'''    uint64_t t1 = t0 + (uint32_t)fd;
    while (emitted < total && (uint64_t)emitted * outMs < t1) {''', 1)

s = s.replace(
'''  cvBuf = (uint16_t*)heap_caps_malloc(SCR_W * SCR_H * 2, MALLOC_CAP_8BIT);
  cvHist = (uint32_t*)heap_caps_calloc(4096, 4, MALLOC_CAP_8BIT);
  cvLut = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  bool ok = false;
  if (cvBuf && cvHist && cvLut) { memset(cvBuf, 0, SCR_W * SCR_H * 2); ok = doConvert(src, dst, srcSize, srcTime, err); }
  else err = "LOW MEMORY";
  if (cvBuf) free(cvBuf); if (cvHist) free(cvHist); if (cvLut) free(cvLut);
  cvBuf = nullptr; cvHist = nullptr; cvLut = nullptr; cvPass = 0;
  S.setColorDepth(16); S.createSprite(SCR_W, SCR_H);''',
'''  const size_t canvasBytes = SCR_W * SCR_H * sizeof(uint16_t);
  cvBuf = (uint16_t*)heap_caps_malloc(canvasBytes, MALLOC_CAP_8BIT);
  cvRestore = (uint16_t*)heap_caps_malloc(canvasBytes, MALLOC_CAP_8BIT);
  cvHist = (uint32_t*)heap_caps_calloc(4096, sizeof(uint32_t), MALLOC_CAP_8BIT);
  cvLut = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  bool ok = false;
  if (cvBuf && cvRestore && cvHist && cvLut) {
    memset(cvBuf, 0, canvasBytes); memset(cvRestore, 0, canvasBytes);
    ok = doConvert(src, dst, srcSize, srcTime, err);
  } else err = "LOW MEMORY";
  if (cvBuf) free(cvBuf); if (cvRestore) free(cvRestore);
  if (cvHist) free(cvHist); if (cvLut) free(cvLut);
  cvBuf = nullptr; cvRestore = nullptr; cvHist = nullptr; cvLut = nullptr; cvPass = 0;
  S.setColorDepth(16);
  if (!S.createSprite(SCR_W, SCR_H)) { err = "DISPLAY MEMORY"; ok = false; }''', 1)

s = s.replace(
'''  uint32_t period = ringKind ? 80 : ((screen == SC_STOPW && swRun) ? 33 : 250);''',
'''  uint32_t period = ringKind ? 80 :
                    (bgOn ? constrain((int)bgFrameMs, 33, 250) :
                    ((screen == SC_STOPW && swRun) ? 33 : 250));''', 1)

s = s.replace(
'''      } else {
        setupDone = true; setupWizard = false; saveCfg();
        mode = M_NORMAL; screen = SC_CLOCK;
      }''',
'''      } else {
        if (!sdOK) { syncMsg = "INSERT FAT32 SD"; dirty = true; return; }
        setupDone = true; setupWizard = false; saveCfg();
        if (cfgDirty) { syncMsg = "SAVE FAILED"; setupDone = false; mode = M_SETUP; }
        else { mode = M_NORMAL; screen = SC_CLOCK; }
      }''', 1)

# Fail the build rather than silently shipping if any critical hardening edit missed.
required = [
    "static const uint32_t BG_MAXF = 4096;",
    "uint16_t* cvRestore = nullptr;",
    "cvPrevDisp == 2",
    "cvPrevDisp == 3",
    "const uint16_t* first = std::lower_bound",
    "uint64_t D = 0",
    "uint64_t t0 = 0",
    "constrain((int)bgFrameMs, 33, 250)",
    "INSERT FAT32 SD",
    "String(CACHE_DIR) + name"
]
for token in required:
    if token not in s:
        raise SystemExit("FINAL OUTPUT CHECK FAILED: " + token)

final_once("int tzMin = 180;", "int tzMin = 60;", "sensible first-boot UTC offset")
final_once('  txt(fit(status, 30), 120, 116, cMid, &fonts::Font0, D_TC);',
           '  txt(fit(sdOK ? status : String("NO FAT32 SD"), 30), 120, 116, cMid, &fonts::Font0, D_TC);',
           "show setup diagnostics")

p.write_text(s)


// ---------------------------------------------------------------------------
// Final runtime fixes: robust NTP validation and full-frame GIF compositing.
// These transformations are intentionally applied after all legacy UI setup
// patches so the compiled sketch is the exact final source.
// ---------------------------------------------------------------------------
s = p.read_text()

def replace_region(start, end, replacement, label):
    global s
    a = s.find(start)
    if a < 0:
        raise SystemExit("RUNTIME PATCH START NOT FOUND: " + label)
    b = s.find(end, a + len(start))
    if b < 0:
        raise SystemExit("RUNTIME PATCH END NOT FOUND: " + label)
    s = s[:a] + replacement + s[b:]

replace_region("void startSync() {", "bool setLocalTime(", r'''void startSync() {
  if (syncState != 0) return;
  if (ssid.isEmpty()) { syncMsg = "NO SSID"; dirty = true; return; }
  if (ESP.getMaxAllocHeap() < 35000) {
    syncMsg = "LOW MEM"; nextSyncAt = millis() + 600000UL; dirty = true; return;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.begin(ssid.c_str(), wpass.c_str());
  syncState = 1;
  syncT0 = millis();
  syncMsg = "WIFI...";
  dirty = true;
}

void finishSync(bool ok, const char* msg) {
  syncMsg = msg;
  syncState = 0;
  if (ok) {
    synced = true;
    nextSyncAt = millis() + 3UL * 3600UL * 1000UL;
    saveCfg();
  } else {
    nextSyncAt = millis() + 5UL * 60UL * 1000UL;
  }
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  dirty = true;
}

void tickSync(uint32_t now) {
  if (syncState == 0) {
    if (ssid.length() && (int32_t)(now - nextSyncAt) >= 0) startSync();
    return;
  }

  if (syncState == 1) {
    if (WiFi.status() == WL_CONNECTED) {
      sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
      configTime(0, 0,
                 "pool.ntp.org",
                 "time.google.com",
                 "time.cloudflare.com",
                 "time.nist.gov");
      syncState = 2;
      syncT0 = now;
      syncMsg = "NTP...";
      dirty = true;
    } else if ((uint32_t)(now - syncT0) >= 25000UL) {
      finishSync(false, "WIFI FAIL");
    }
    return;
  }

  if (syncState == 2) {
    // Do not trust SNTP's status flag alone: validate the actual system epoch.
    time_t epoch = time(nullptr);
    if (epoch >= (time_t)1700000000) {
      if (autoTz) {
        HTTPClient http;
        http.setTimeout(2500);
        if (http.begin("http://ip-api.com/json/?fields=status,offset")) {
          int code = http.GET();
          if (code == HTTP_CODE_OK) {
            String body = http.getString();
            if (body.indexOf("\"status\":\"success\"") >= 0) {
              int at = body.indexOf("\"offset\":");
              if (at >= 0) {
                long seconds = body.substring(at + 9).toInt();
                if (seconds >= -43200L && seconds <= 50400L)
                  tzMin = (int)(seconds / 60L);
              }
            }
          }
          http.end();
        }
      }
      finishSync(true, "SYNC OK");
    } else if ((uint32_t)(now - syncT0) >= 30000UL) {
      finishSync(false, "NTP FAIL");
    }
  }
}
''', "NTP state machine")

// GIF frame composition: retain pixels through transparency and honor disposal
// methods 2 (restore background) and 3 (restore previous canvas).
replace_region("void GIFDraw(GIFDRAW* d) {", "// ---- converter UI", r'''void GIFDraw(GIFDRAW* d) {
  uint8_t* src = d->pPixels;
  uint16_t* pal = d->pPalette;
  const bool transparent = d->ucHasTransparency;
  const uint8_t transparentIndex = d->ucTransparent;

  if (cvPass == 1) {
    if (!cvHist || (d->y & 1)) return;
    for (int x = 0; x < d->iWidth; ++x) {
      uint8_t index = src[x];
      if (transparent && index == transparentIndex) continue;
      uint16_t bin = bin444(pal[index]);
      if (cvHist[bin] != UINT32_MAX) ++cvHist[bin];
    }
    return;
  }
  if (cvPass != 2 || !cvBuf) return;

  // AnimatedGIF calls the callback once per decoded scanline. y==0 marks
  // the first line of a new image descriptor.
  if (d->y == 0) {
    if (cvPrevValid) {
      if (cvPrevDisp == 2) {
        for (int dy = 0; dy < SCR_H; ++dy) {
          int sy = syMap[dy];
          if (sy < cvPrevY || sy >= cvPrevY + cvPrevH) continue;
          uint16_t* row = cvBuf + dy * SCR_W;
          for (int dx = 0; dx < SCR_W; ++dx) {
            int sx = sxMap[dx];
            if (sx >= cvPrevX && sx < cvPrevX + cvPrevW)
              row[dx] = cvPrevBg;
          }
        }
      } else if (cvPrevDisp == 3 && cvRestore) {
        memcpy(cvBuf, cvRestore, SCR_W * SCR_H * sizeof(uint16_t));
      }
    }
    if (d->ucDisposalMethod == 3 && cvRestore)
      memcpy(cvRestore, cvBuf, SCR_W * SCR_H * sizeof(uint16_t));

    cvPrevX = d->iX;
    cvPrevY = d->iY;
    cvPrevW = d->iWidth;
    cvPrevH = d->iHeight;
    cvPrevDisp = d->ucDisposalMethod;
    cvPrevBg = pal[d->ucBackground];
    cvPrevValid = true;
  }

  const int sourceY = d->iY + d->y;
  const uint16_t* first = std::lower_bound(syMap, syMap + SCR_H, sourceY);
  const uint16_t* last = std::upper_bound(syMap, syMap + SCR_H, sourceY);
  if (first == last) return;

  const int dy0 = (int)(first - syMap);
  const int dy1 = (int)(last - syMap);
  const int x0 = d->iX;
  const int x1 = d->iX + d->iWidth;
  for (int dy = dy0; dy < dy1; ++dy) {
    uint16_t* out = cvBuf + dy * SCR_W;
    for (int dx = 0; dx < SCR_W; ++dx) {
      const int sx = sxMap[dx];
      if (sx < x0 || sx >= x1) continue;
      const uint8_t index = src[sx - x0];
      if (transparent && index == transparentIndex) continue;
      out[dx] = pal[index];
    }
  }
}
''', "GIF compositing")

// Correct frame scheduling for long animations and prevent cache count overflow.
s = s.replace("uint32_t n = 0, D = 0; int fd = 0, r;",
              "uint32_t n = 0; uint64_t D = 0; int fd = 0, r;", 1)
s = s.replace("uint32_t t0 = 0, emitted = 0; bool fail = false;",
              "uint64_t t0 = 0; uint32_t emitted = 0; bool fail = false;", 1)
s = s.replace("uint32_t t1 = t0 + fd;\n    while (emitted < total && emitted * outMs < t1) {",
              "uint64_t t1 = t0 + (uint32_t)fd;\n    while (emitted < total && (uint64_t)emitted * outMs < t1) {", 1)
s = s.replace("  gif.reset(); cvPass = 2; cvPos = 0;",
              "  gif.reset(); cvPass = 2; cvPos = 0;\n  cvPrevValid = false; cvPrevDisp = 0;", 1)

if "uint64_t D = 0" not in s or "cvPrevDisp == 3" not in s or "epoch >= (time_t)1700000000" not in s:
    raise SystemExit("RUNTIME PATCH VALIDATION FAILED")
p.write_text(s)
