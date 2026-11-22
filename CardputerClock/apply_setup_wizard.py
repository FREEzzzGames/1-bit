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
     'void scanGifs() {\n  gifList.clear(); gifCached.clear();\n  if (!sdOK) return;\n  scanDir(gifDir.c_str());',
     'scan configured folder')
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

replace_between("  if (mode == M_PICK) {", "  if (st.tab) { goScreen(screen + 1); return; }", r'''  if (mode == M_PICK) {
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

once("  for (char c : ks) if (c == " + chr(96) + ") { goScreen(SC_CLOCK); return; }",
     "  for (char c : ks) if (c == 96) { if (setupWizard && screen == SC_SET) { mode = M_SETUP; dirty = true; return; } goScreen(SC_CLOCK); return; }",
     "manual settings back key")
once("  if (st.tab) { goScreen(screen + 1); return; }",
     "  if (st.tab) { if (setupWizard && screen == SC_SET) { mode = M_SETUP; dirty = true; return; } goScreen(screen + 1); return; }",
     "manual settings tab key")

once('  txt(";. SELECT   ENTER=OPEN", 120, 124, cMid, &fonts::Font0, D_TC);',
     '  String status = syncState ? syncMsg : (synced ? "TIME SYNCED" : syncMsg);\n  txt(fit(status, 30), 120, 116, cMid, &fonts::Font0, D_TC);\n  txt(";. SELECT   ENTER=OPEN", 120, 130, cMid, &fonts::Font0, D_TC);',
     "setup status line")
once('    else if (k == "gif") gifPath = v;',
     '    else if (k == "gif") gifPath = v;\n    else if (k == "gifDir") gifDir = v;\n    else if (k == "setupDone") setupDone = v.toInt() != 0;',
     "load setup keys")
once('  scanDir("/gifs"); scanDir("/");', '  scanDir(gifDir.c_str());', "selected GIF directory")
once('  syncMsg = msg; syncState = 0;', '  syncMsg = msg; syncState = 0; dirty = true;', "sync status redraw")

p.write_text(s)
