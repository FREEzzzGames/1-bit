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
     'f.print("gif="); f.println(gifPath); f.print("gifDir="); f.println(gifDir); f.printf("setupDone=%d\\\\n", setupDone ? 1 : 0);',
     'save config')
once('else if (k == "gif") gifPath = v;',
     'else if (k == "gif") gifPath = v;\n    else if (k == "gifDir") gifDir = v;\n    else if (k == "setupDone") setupDone = v.toInt() != 0;',
     'load config')
once('void scanGifs() {\\n  gifList.clear(); gifCached.clear();\\n  if (!sdOK) return;\\n  scanDir("/gifs"); scanDir("/");',
     'void scanGifs() {\\n  gifList.clear(); gifCached.clear();\\n  if (!sdOK) return;\\n  scanDir(gifDir.c_str());',
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
once(anchor, insert + '\\n' + anchor, 'setup UI functions')
# Render mode dispatch
once('if (mode == M_PICK) drawPicker();\\n  else if (mode == M_TEXT) drawTextInput();',
     'if (mode == M_PICK) drawPicker();\\n  else if (mode == M_TEXT) drawTextInput();\\n  else if (mode == M_SETUP) drawSetupMenu();\\n  else if (mode == M_WIFI) drawWifiList();\\n  else if (mode == M_DIR) drawDirList();',
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
once(anchor, insert + '\\n' + anchor, 'input modes')
# After password input, return to wizard if setup is active
once('else if (inTarget == 1) { wpass = inBuf; markCfg(); }',
     'else if (inTarget == 1) { wpass = inBuf; markCfg(); nextSyncAt = millis(); }',
     'wifi password sync')
once('mode = M_NORMAL; return;\\n    }\\n    if (st.del)',
     'mode = setupWizard ? M_SETUP : M_NORMAL; return;\\n    }\\n    if (st.del)',
     'text input return mode')
# Entering picker from settings scans selected folder already; setup's folder select opens GIF list
# After choosing GIF return to setup wizard when in setup context
once('mode = M_NORMAL;\\n      gifPath = pickCur == 0 ? String("") : gifList[pickCur - 1];',
     'mode = setupWizard ? M_SETUP : M_NORMAL;\\n      gifPath = pickCur == 0 ? String("") : gifList[pickCur - 1];',
     'picker return mode')
# Manual settings exit: TAB/ESC returns to setup while wizard is active; finish wizard via menu
once('if (st.tab) { goScreen(screen + 1); return; }',
     'if (st.tab) { if (setupWizard && screen == SC_SET) { mode = M_SETUP; return; } goScreen(screen + 1); return; }',
     'settings return')
# Setup startup decision after loading config and initializing display
once('loadCfg();\\n  applyBacklight(); updateColors();',
     'loadCfg();\\n  setupWizard = !setupDone;\\n  if (setupWizard) { mode = M_SETUP; pickCur = 0; }\\n  applyBacklight(); updateColors();',
     'first boot wizard')
# Settings menu can be opened any time; existing settings navigation remains
# Save directory and first-start state in config.
p.write_text(s)
