/*
 * NEON BASTION v2 - retro neon tower defense for M5Stack Cardputer ADV (ESP32-S3)
 *
 * Board:     M5Stack -> M5Cardputer
 * Libraries: M5Cardputer, M5Unified, M5GFX
 *
 * 5 MAPS x 15 WAVES, each map with its own theme, path and 5 monsters (4 + boss):
 *   1 GRID ZERO    scout / tank / swarm / phantom / OVERLORD
 *   2 CRYO VAULT   frost wisp / ice golem / snow mite / aurora wraith / GLACIER TITAN
 *   3 MOLTEN CORE  ember imp (dashes) / magma brute / ash mite / smoke spirit / INFERNO DRAKE
 *   4 TOXIC BOG    spore fly / bog beast (regenerates) / slime pack (splits) / mire stalker / HIVE QUEEN
 *   5 VOID STATION void drone (dashes) / rift walker (shields) / nano swarm / phase stalker (teleports) / VOID EYE
 *
 * Collisions: enemies are circles; bullets and shells are swept (they hit the first
 * enemy in their way), the LASER pierces everything on its beam line, splash and
 * frost use edge distance, tower targeting uses enemy radius, enemies walk on
 * slightly offset lanes so crowds never stack into one pixel.
 *
 * Modules (sections): GameCore, EnemySystem, TowerSystem, ProjectileSystem,
 * ParticleSystem, Renderer (own 4x6 font + pre-built sprites), InputManager,
 * AudioSystem, SaveSystem. Fixed object pools, delta-time, no allocation in the loop.
 *
 * CONTROLS
 *   W A S D  move cursor between pads        ENTER  build / upgrade / confirm
 *   Q / E    previous / next tower           1-4    pick tower
 *   SPACE    start next wave                 U upgrade    X sell
 *   ESC (`) or DEL  pause / back             M sound (title / pause)
 *   Map select: A / D choose map, ENTER start
 */
#include <M5Cardputer.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <vector>

// ============================== CONFIG =============================
static const int SW = 240, SH = 135, TILE = 10, MAPW = 24, MAPH = 10, MAPY = 12;
static const int NPADS = 12, MAXE = 64, MAXP = 48, MAXPART = 192, MAXRING = 20, MAXFLT = 8, NWAVES = 15, NMAPS = 5;
static const int CORE_X = 230, CORE_MAX = 20;

M5Canvas cv(&M5Cardputer.Display);
Preferences prefs;

// ============================== PALETTE ============================
static uint16_t RGB(uint8_t r, uint8_t g, uint8_t b) { return M5Cardputer.Display.color565(r, g, b); }
static uint16_t C_BG, C_BG2, C_TR, C_G, C_MG, C_DG, C_CY, C_CYD, C_AM, C_AMD, C_RD, C_RDD, C_WH, C_ICE, C_IND;
static uint16_t C_OR, C_ORD, C_LM, C_LMD, C_VI, C_VID, C_PK, C_PKD, C_BL, C_BLD, C_BLACK;
static uint16_t SHD[10][4];
enum { CI_G, CI_CY, CI_AM, CI_RD, CI_WH, CI_ICE, CI_OR, CI_LM, CI_VI, CI_PK };
static uint16_t TCOL[4];
struct Theme { uint16_t bg, bg2, trace, path, edge, dec1, dec2, glow; };
static Theme TH;

static uint16_t mixc(uint16_t c, float k) {
  int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  r = (int)(r * k); g = (int)(g * k); b = (int)(b * k);
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}
static void initPalette() {
  C_BLACK = RGB(0, 0, 0);
  C_BG = RGB(0x03, 0x07, 0x04); C_BG2 = RGB(0x05, 0x0D, 0x07); C_TR = RGB(0x0B, 0x4A, 0x1F);
  C_G = RGB(0x39, 0xFF, 0x58);  C_MG = RGB(0x1F, 0xA8, 0x3A); C_DG = RGB(0x06, 0x30, 0x14);
  C_CY = RGB(0x00, 0xE5, 0xFF); C_CYD = RGB(0x00, 0x6E, 0x80);
  C_AM = RGB(0xFF, 0xB0, 0x00); C_AMD = RGB(0x8A, 0x5F, 0x00);
  C_RD = RGB(0xFF, 0x30, 0x30); C_RDD = RGB(0x8A, 0x10, 0x10);
  C_WH = RGB(0xE8, 0xFF, 0xE8); C_ICE = RGB(0xA0, 0xE8, 0xFF); C_IND = RGB(0x3A, 0x8F, 0xB0);
  C_OR = RGB(0xFF, 0x7A, 0x00); C_ORD = RGB(0x9A, 0x3D, 0x00);
  C_LM = RGB(0xB8, 0xFF, 0x3A); C_LMD = RGB(0x4F, 0x7A, 0x12);
  C_VI = RGB(0xB0, 0x4C, 0xFF); C_VID = RGB(0x5A, 0x1F, 0x8A);
  C_PK = RGB(0xFF, 0x4C, 0xE0); C_PKD = RGB(0x8A, 0x1F, 0x78);
  C_BL = RGB(0x4C, 0x7C, 0xFF); C_BLD = RGB(0x1F, 0x3A, 0x8A);
  uint16_t base[10] = {C_G, C_CY, C_AM, C_RD, C_WH, C_ICE, C_OR, C_LM, C_VI, C_PK};
  for (int i = 0; i < 10; i++) {
    SHD[i][0] = base[i]; SHD[i][1] = mixc(base[i], 0.62f);
    SHD[i][2] = mixc(base[i], 0.36f); SHD[i][3] = mixc(base[i], 0.18f);
  }
  TCOL[0] = C_G; TCOL[1] = C_CY; TCOL[2] = C_ICE; TCOL[3] = C_RD;
}
static void setTheme(int m) {
  switch (m) {
    case 1: TH = Theme{RGB(2, 8, 12), RGB(4, 13, 19), RGB(14, 60, 86), RGB(8, 32, 52), RGB(70, 180, 220), RGB(60, 140, 180), C_ICE, C_ICE}; break;
    case 2: TH = Theme{RGB(12, 4, 2), RGB(18, 7, 3), RGB(80, 30, 8), RGB(58, 16, 6), RGB(255, 122, 0), RGB(120, 48, 10), C_OR, C_OR}; break;
    case 3: TH = Theme{RGB(4, 9, 3), RGB(7, 14, 4), RGB(30, 70, 14), RGB(20, 42, 10), RGB(140, 220, 40), RGB(60, 110, 20), C_LM, C_LM}; break;
    case 4: TH = Theme{RGB(6, 3, 12), RGB(10, 5, 19), RGB(50, 24, 90), RGB(26, 12, 52), C_VI, RGB(90, 40, 150), C_PK, C_VI}; break;
    default: TH = Theme{C_BG, C_BG2, C_TR, RGB(6, 48, 20), C_MG, C_TR, C_MG, C_G}; break;
  }
}

// ============================== FONT (4x6 pixel font) ==============
#define G6(a, b, c, d, e, f) (((uint32_t)(a) << 20) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 12) | ((uint32_t)(d) << 8) | ((uint32_t)(e) << 4) | (uint32_t)(f))
static const char FCH[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-:/%.!><x*()=?";
static const uint32_t FGL[51] = {
  G6(0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110), G6(0b0010, 0b0110, 0b0010, 0b0010, 0b0010, 0b0111),
  G6(0b0110, 0b1001, 0b0001, 0b0110, 0b1000, 0b1111), G6(0b1110, 0b0001, 0b0110, 0b0001, 0b0001, 0b1110),
  G6(0b1001, 0b1001, 0b1111, 0b0001, 0b0001, 0b0001), G6(0b1111, 0b1000, 0b1110, 0b0001, 0b1001, 0b0110),
  G6(0b0110, 0b1000, 0b1110, 0b1001, 0b1001, 0b0110), G6(0b1111, 0b0001, 0b0010, 0b0010, 0b0100, 0b0100),
  G6(0b0110, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110), G6(0b0110, 0b1001, 0b1001, 0b0111, 0b0001, 0b0110),
  G6(0b0110, 0b1001, 0b1001, 0b1111, 0b1001, 0b1001), G6(0b1110, 0b1001, 0b1110, 0b1001, 0b1001, 0b1110),
  G6(0b0111, 0b1000, 0b1000, 0b1000, 0b1000, 0b0111), G6(0b1110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1110),
  G6(0b1111, 0b1000, 0b1110, 0b1000, 0b1000, 0b1111), G6(0b1111, 0b1000, 0b1110, 0b1000, 0b1000, 0b1000),
  G6(0b0111, 0b1000, 0b1011, 0b1001, 0b1001, 0b0111), G6(0b1001, 0b1001, 0b1111, 0b1001, 0b1001, 0b1001),
  G6(0b1110, 0b0100, 0b0100, 0b0100, 0b0100, 0b1110), G6(0b0111, 0b0001, 0b0001, 0b0001, 0b1001, 0b0110),
  G6(0b1001, 0b1010, 0b1100, 0b1010, 0b1001, 0b1001), G6(0b1000, 0b1000, 0b1000, 0b1000, 0b1000, 0b1111),
  G6(0b1001, 0b1111, 0b1111, 0b1001, 0b1001, 0b1001), G6(0b1001, 0b1101, 0b1111, 0b1011, 0b1001, 0b1001),
  G6(0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110), G6(0b1110, 0b1001, 0b1001, 0b1110, 0b1000, 0b1000),
  G6(0b0110, 0b1001, 0b1001, 0b1001, 0b1011, 0b0111), G6(0b1110, 0b1001, 0b1001, 0b1110, 0b1010, 0b1001),
  G6(0b0111, 0b1000, 0b0110, 0b0001, 0b0001, 0b1110), G6(0b1111, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100),
  G6(0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110), G6(0b1001, 0b1001, 0b1001, 0b1001, 0b0110, 0b0110),
  G6(0b1001, 0b1001, 0b1001, 0b1111, 0b1111, 0b1001), G6(0b1001, 0b1001, 0b0110, 0b0110, 0b1001, 0b1001),
  G6(0b1001, 0b1001, 0b0110, 0b0100, 0b0100, 0b0100), G6(0b1111, 0b0001, 0b0010, 0b0100, 0b1000, 0b1111),
  G6(0, 0b0100, 0b1110, 0b0100, 0, 0), G6(0, 0, 0b1110, 0, 0, 0), G6(0, 0b0100, 0, 0, 0b0100, 0),
  G6(0b0001, 0b0001, 0b0010, 0b0100, 0b1000, 0b1000), G6(0b1001, 0b0001, 0b0010, 0b0100, 0b1000, 0b1001),
  G6(0, 0, 0, 0, 0, 0b0100), G6(0b0100, 0b0100, 0b0100, 0b0100, 0, 0b0100),
  G6(0b1000, 0b0100, 0b0010, 0b0100, 0b1000, 0), G6(0b0010, 0b0100, 0b1000, 0b0100, 0b0010, 0),
  G6(0, 0, 0b1001, 0b0110, 0b0110, 0b1001), G6(0, 0b1010, 0b0100, 0b1010, 0, 0),
  G6(0b0010, 0b0100, 0b0100, 0b0100, 0b0100, 0b0010), G6(0b0100, 0b0010, 0b0010, 0b0010, 0b0010, 0b0100),
  G6(0, 0b1110, 0, 0b1110, 0, 0), G6(0b0110, 0b1001, 0b0010, 0b0100, 0, 0b0100)
};
static void T(const char* s, int x, int y, uint16_t col, int sc = 1) {
  for (; *s; s++) {
    char c = (char)toupper((unsigned char)*s);
    const char* p = (c != ' ') ? strchr(FCH, c) : nullptr;
    if (p) {
      uint32_t g = FGL[p - FCH];
      for (int r = 0; r < 6; r++) {
        uint32_t bits = (g >> ((5 - r) * 4)) & 15;
        for (int cc = 0; cc < 4; cc++)
          if (bits & (8 >> cc)) {
            if (sc == 1) cv.drawPixel(x + cc, y + r, col);
            else cv.fillRect(x + cc * sc, y + r * sc, sc, sc, col);
          }
      }
    }
    x += 5 * sc;
  }
}
static int Tw(const char* s, int sc = 1) { return (int)strlen(s) * 5 * sc - sc; }
static void Tc(const char* s, int cx, int y, uint16_t col, int sc = 1) { T(s, cx - Tw(s, sc) / 2, y, col, sc); }
static void Tr(const char* s, int xr, int y, uint16_t col, int sc = 1) { T(s, xr - Tw(s, sc), y, col, sc); }

// ============================== SPRITES ============================
struct Spr { uint8_t w, h; uint16_t px[256]; };
enum { SP_PULSE = 0, SP_PAD = 8, SP_E0 = 9, SP_COIN = 19, SP_HEART = 20, SP_SKULL = 21, SP_COUNT = 22 };
static Spr SPR[SP_COUNT];

static uint16_t chCol(char c, int f, uint16_t alt) {
  switch (c) {
    case 'G': return C_G;   case 'm': return C_MG;  case 'g': return C_DG;
    case 'C': return C_CY;  case 'c': return C_CYD;
    case 'A': return C_AM;  case 'a': return C_AMD;
    case 'R': return C_RD;  case 'r': return C_RDD;
    case 'W': return C_WH;  case 'k': return C_BG;
    case 'I': return C_ICE; case 'i': return C_IND;
    case 'O': return C_OR;  case 'o': return C_ORD;
    case 'L': return C_LM;  case 'l': return C_LMD;
    case 'V': return C_VI;  case 'v': return C_VID;
    case 'P': return C_PK;  case 'n': return C_PKD;
    case 'B': return C_BL;  case 'b': return C_BLD;
    case 'e': return TH.edge; case 'f': return TH.path;
    case 'p': return f ? alt : C_WH;
    case 'q': return f ? C_CYD : C_CY;
    case 't': return f ? C_AMD : C_AM;
    case 'u': return f ? C_AM : C_AMD;
    default:  return 0;
  }
}
static void makeSpr(Spr& s, const char* const* rows, int w, int h, bool mirror, int f, uint16_t alt) {
  s.w = (uint8_t)w; s.h = (uint8_t)h;
  for (int y = 0; y < h; y++) {
    int len = (int)strlen(rows[y]);
    for (int x = 0; x < w; x++) {
      int sx = mirror ? (w - 1 - x) : x;
      char c = (sx < len) ? rows[y][sx] : '.';
      s.px[y * w + x] = chCol(c, f, alt);
    }
  }
}
// ---- towers (10x10)
static const char* const R_PULSE[10] = {"...kGGk...", "..kGWWGk..", ".kGWppWGk.", ".kGGppGGk.", "kGGmGGmGGk",
                                        "kGmGGGGmGk", "kmGGGGGGmk", "kGmmGGmmGk", "kggggggggk", ".kkkkkkkk."};
static const char* const R_LASER[10] = {"....CC....", "...CWWC...", "..kCqqCk..", ".kcCCCCck.", "kccCCCCcck",
                                        "kcCCqqCCck", "kccCCCCcck", "kcccIIccck", "kcccccccck", ".kkkkkkkk."};
static const char* const R_FROST[10] = {"....WW....", "...WIIW...", "..WIqqIW..", "..IIqqII..", ".kICCCCIk.",
                                        "kcICCCCIck", ".kccCCcck.", "..kccccck.", ".kkcccckk.", "..kkkkkk.."};
static const char* const R_BURST[10] = {"...kAAk...", "..kRAARk..", "..kRppRk..", "..kRrrRk..", ".kRRrrRRk.",
                                        "kRRRRRRRRk", "kRaRRRRaRk", "kRRaaaaRRk", "kaaaaaaaak", ".kkkkkkkk."};
static const char* const R_PAD[10] = {"ee......ee", "e........e", "..ffffff..", "..ffffff..", "..ffffff..",
                                      "..ffffff..", "..ffffff..", "..ffffff..", "e........e", "ee......ee"};
// ---- map 1: GRID ZERO
static const char* const R_SCOUT[8] = {"A.AAAA.A", ".AaWWaA.", "AAAppAAA", ".AAaaAA.", "..aAAa..", "..a..a..", "........", "........"};
static const char* const R_TANK[10] = {"...kRRk...", "..kRWWRk..", ".kRRRRRRk.", "kRRaRRaRRk", "kRrRRRRrRk",
                                       "krRRaaRRrk", "kRRRRRRRRk", ".kkkkkkkk.", "tututututu", ".kkkkkkkk."};
static const char* const R_SWARM[6] = {".AAAA.", "AWAAWA", ".AAAA.", "a.aa.a", ".a..a.", "......"};
static const char* const R_PHANTOM[8] = {"..cCCc..", ".cCCCCc.", "cCWCCWCc", "cCCCCCCc", "cCCCCCCc", "cCcCCcCc", "c.cCCc.c", ".c....c."};
static const char* const R_OVERLORD[16] = {
  ".....rRRRRr.....", "...rRRRRRRRRRr...", "..rRRRaaaaRRRr..", ".rRRaaAAAAaaRRr.", ".rRRaAWWWWAaRRr.",
  "rRRRaAWppWAaRRRr", "rRRRaAWWWWAaRRRr", "rRRRRaAAAAaRRRRr", ".rRRRRaaaaRRRRr.", ".rrRRRRRRRRRRrr.",
  "..rrRaRRRRaRrr..", "..kkrRRRRRRrkk..", "..krRaaRRaaRrk..", ".kAarRRRRRRraAk.", ".kAAkkrrrrkkAAk.", "..kk........kk.."};
// ---- map 2: CRYO VAULT
static const char* const R_WISP[8] = {"...II...", "..IWWI..", ".IWCCWI.", ".IWCCWI.", "..IWWI..", "..i..i..", ".i.ii.i.", "........"};
static const char* const R_GOLEM[10] = {".I..II..I.", ".IIiIIiII.", "..IWIIWI..", ".iIIIIIIi.", "iIIiIIiIIi",
                                        "iIIIWWIIIi", "iIIIIIIIIi", ".iIIiiIIi.", ".iiI..Iii.", "..ii..ii.."};
static const char* const R_MITE[6] = {".II.I.", "IWIIWI", ".IIII.", "i.ii.i", "i....i", "......"};
static const char* const R_WRAITH[8] = {"..IIII..", ".ICCCCI.", "ICWCCWCI", "ICCVVCCI", ".ICCCCI.", ".IcCCcI.", "..c..c..", ".c....c."};
static const char* const R_TITAN[16] = {
  "..I..I.WW.I..I..", ".IIiIIIIIIIIiII.", ".IIIWWIIIIWWIII.", "iIIIWCIIIICWIIIi", "iIIiIIIIIIIIiIIi",
  "iIIIIIWppWIIIIIi", "iIIiIIIWWIIIiIIi", "iIIIIiIIIIiIIIIi", ".iIIIIiIIiIIIIi.", ".iiIIIIIIIIIIii.",
  "..iiIIIIIIIIii..", "..kiiIIIIIIIiik.", "..kIIiIIIIiIIk..", ".kIIiiIIIIiiIIk.", ".kiiikIIIIkiiik.", ".kkk.kkkkkk.kkk."};
// ---- map 3: MOLTEN CORE
static const char* const R_IMP[8] = {"O..OO..O", ".OOAAOO.", "OAAWWAAO", ".OAAAAO.", "..OAAO..", ".O.OO.O.", ".o....o.", "........"};
static const char* const R_BRUTE[10] = {"..oooooo..", ".oRroorRo.", "oorOOOOroo", "orOrAArOro", "orOOAAOOro",
                                        "oorOOOOroo", ".orOrrOro.", ".oo.OO.oo.", ".oo.oo.oo.", "..oo..oo.."};
static const char* const R_ASH[6] = {".oAAo.", "oAWWAo", ".oAAo.", "o.oo.o", ".o..o.", "......"};
static const char* const R_SMOKE[8] = {"..roor..", ".roooor.", "roAooAor", "rooooooR", "roooooor", ".roooor.", "r.roor.r", ".r....r."};
static const char* const R_DRAKE[16] = {
  "O...O......O...O", ".O..OO.AA.OO..O.", ".OOoOOOAAOOOoOO.", "oOOOOAAAAAAOOOOo", "oOOoAAWppWAAoOOo",
  "oOOoAAAAAAAAoOOo", ".oOOoAAAAAAoOOo.", ".oOOOoAAAAoOOOo.", "..oOOOooRRoOOOo.", "..oooOOrrrrOOooo",
  "...ooorrOOrrooo.", "...kooOOOOOOook.", "..koRRooOOooRRok", ".koRRooOOOOooRRk", ".kAAkkooooookAAk.", "..kk........kk.."};
// ---- map 4: TOXIC BOG
static const char* const R_FLY[8] = {"LL....LL", "LLL..LLL", ".lLLLLl.", "..LppL..", "..lLLl..", "...ll...", "..l..l..", "........"};
static const char* const R_BEAST[10] = {"..llllll..", ".lLLLLLLl.", "lLLWLLWLLl", "lLLLllLLLl", "lLmLLLLmLl",
                                        "lLLmmmmLLl", "llLLLLLLll", ".lLLllLLl.", ".ll.LL.ll.", "..ll..ll.."};
static const char* const R_SLIME[6] = {".LLLL.", "LWLLWL", "LLLLLL", "lLLLLl", ".llll.", "......"};
static const char* const R_STALKER[8] = {"..vVVv..", ".vVVVVv.", "vVLVVLVv", "vVVVVVVv", ".vVvvVv.", ".v.vv.v.", "v..vv..v", "........"};
static const char* const R_QUEEN[16] = {
  "...V..VVVV..V...", "..VvV.VVVV.VvV..", "...vVVVVVVVVv...", "..lLLLLLLLLLLl..", ".lLLWWLLLLWWLLl.",
  ".lLLWppLLppWLLl.", "lLLLLWWLLWWLLLLl", "lLLLLLLLLLLLLLLl", "lLmLLLmmmmLLLmLl", ".lLmmLLLLLLmmLl.",
  ".llLLmLmmLmLLll.", "..llLLLLLLLLll..", "..vlllLLLLlllv..", ".vVvllLLLLllvVv.", ".vVv.vllllv.vVv.", "..vv..v..v..vv.."};
// ---- map 5: VOID STATION
static const char* const R_DRONE[8] = {"..VVVV..", ".VBBBBV.", "VBBppBBV", "VBBppBBV", ".VBBBBV.", "..vVVv..", ".v.vv.v.", "v......v"};
static const char* const R_WALKER[10] = {"..bbbbbb..", ".bBBBBBBb.", "bBBVppVBBb", "bBBBBBBBBb", "bVBbBBbBVb",
                                         "bVVBBBBVVb", ".bVBBBBVb.", ".bb.bb.bb.", ".bb.bb.bb.", "bbb.bb.bbb"};
static const char* const R_NANO[6] = {".VVVV.", "VPVVPV", ".VVVV.", "v.vv.v", ".v..v.", "......"};
static const char* const R_RIFT[8] = {"..nPPn..", ".nPPPPn.", "nPWPPWPn", "nPPPPPPn", ".nPnnPn.", ".n.PP.n.", "n..nn..n", "........"};
static const char* const R_EYE[16] = {
  "....vvVVVVvv....", "..vvVVVVVVVVvv..", ".vVVVBBBBBBVVVv.", ".vVVBBWWWWBBVVv.", "vVVBBWPPPPWBBVVv",
  "vVBBWPppppPWBBVv", "vVBBWPppppPWBBVv", "vVVBBWPPPPWBBVVv", ".vVVBBWWWWBBVVv.", ".vVVVBBBBBBVVVv.",
  "..vvVVVVVVVVvv..", "...vvVVVVVVvv...", "..PvP.vVVv.PvP..", ".Pv.P..vv..P.vP.", ".P..P...vv...P.P", "P....P......P..P"};
static const char* const R_COIN[7] = {"..AAA..", ".AAAAA.", "AAAaAAA", "AAAaAAA", "AAAaAAA", ".AAAAA.", "..AAA.."};
static const char* const R_HEART[7] = {".RR.RR.", "RRRRRRR", "RRRRRRR", ".RRRRR.", "..RRR..", "...R...", "......."};
static const char* const R_SKULL[7] = {".WWWWW.", "WWWWWWW", "WkWWWkW", "WWWkWWW", ".WWWWW.", ".W.W.W.", "......."};

// ---- species (5 per map: 0 fast, 1 tank, 2 swarm, 3 special, 4 boss)
enum { AB_NONE, AB_PHASE, AB_REGEN, AB_SPLIT, AB_DASH, AB_TELE, AB_SHIELD, AB_BOSS };
struct Species { const char* name; uint8_t w, h; const char* const* rows; float hp, speed, armor; uint8_t reward, dmg, ab; char altc; };
static const Species SPC[NMAPS][5] = {
  {{"SCOUT", 8, 8, R_SCOUT, 18, 38, 0, 6, 1, AB_NONE, 'R'}, {"TANK", 10, 10, R_TANK, 95, 17, 3, 16, 3, AB_NONE, 'W'},
   {"SWARM", 6, 6, R_SWARM, 9, 30, 0, 2, 1, AB_NONE, 'W'}, {"PHANTOM", 8, 8, R_PHANTOM, 32, 33, 1, 9, 2, AB_PHASE, 'W'},
   {"OVERLORD", 16, 16, R_OVERLORD, 420, 13, 4, 120, 10, AB_BOSS, 'R'}},
  {{"FROST WISP", 8, 8, R_WISP, 16, 44, 0, 6, 1, AB_NONE, 'C'}, {"ICE GOLEM", 10, 10, R_GOLEM, 120, 15, 4, 18, 3, AB_NONE, 'W'},
   {"SNOW MITE", 6, 6, R_MITE, 10, 30, 0, 2, 1, AB_NONE, 'W'}, {"AURORA WRAITH", 8, 8, R_WRAITH, 36, 33, 1, 10, 2, AB_PHASE, 'W'},
   {"GLACIER TITAN", 16, 16, R_TITAN, 500, 12, 5, 130, 10, AB_BOSS, 'C'}},
  {{"EMBER IMP", 8, 8, R_IMP, 20, 34, 0, 7, 1, AB_DASH, 'R'}, {"MAGMA BRUTE", 10, 10, R_BRUTE, 125, 16, 3, 19, 3, AB_NONE, 'W'},
   {"ASH MITE", 6, 6, R_ASH, 9, 32, 0, 2, 1, AB_NONE, 'W'}, {"SMOKE SPIRIT", 8, 8, R_SMOKE, 34, 34, 1, 10, 2, AB_PHASE, 'W'},
   {"INFERNO DRAKE", 16, 16, R_DRAKE, 540, 13, 4, 140, 10, AB_BOSS, 'R'}},
  {{"SPORE FLY", 8, 8, R_FLY, 18, 40, 0, 7, 1, AB_NONE, 'R'}, {"BOG BEAST", 10, 10, R_BEAST, 135, 15, 3, 20, 3, AB_REGEN, 'W'},
   {"SLIME PACK", 6, 6, R_SLIME, 16, 26, 0, 4, 1, AB_SPLIT, 'W'}, {"MIRE STALKER", 8, 8, R_STALKER, 38, 34, 1, 11, 2, AB_PHASE, 'W'},
   {"HIVE QUEEN", 16, 16, R_QUEEN, 580, 12, 5, 150, 10, AB_BOSS, 'R'}},
  {{"VOID DRONE", 8, 8, R_DRONE, 22, 38, 0, 8, 1, AB_DASH, 'P'}, {"RIFT WALKER", 10, 10, R_WALKER, 150, 16, 3, 22, 3, AB_SHIELD, 'P'},
   {"NANO SWARM", 6, 6, R_NANO, 10, 34, 0, 3, 1, AB_NONE, 'W'}, {"PHASE STALKER", 8, 8, R_RIFT, 40, 34, 1, 12, 2, AB_TELE, 'W'},
   {"VOID EYE", 16, 16, R_EYE, 620, 13, 5, 160, 10, AB_BOSS, 'P'}}
};
static void buildEnemySprites(int m) {
  for (int k = 0; k < 5; k++) {
    const Species& S = SPC[m][k];
    uint16_t alt = chCol(S.altc, 0, 0);
    makeSpr(SPR[SP_E0 + k * 2], S.rows, S.w, S.h, false, 0, alt);
    makeSpr(SPR[SP_E0 + k * 2 + 1], S.rows, S.w, S.h, true, 1, alt);
  }
  makeSpr(SPR[SP_PAD], R_PAD, 10, 10, false, 0, 0);
}
static void buildSprites() {
  for (int f = 0; f < 2; f++) {
    makeSpr(SPR[SP_PULSE + 0 + f], R_PULSE, 10, 10, false, f, C_MG);
    makeSpr(SPR[SP_PULSE + 2 + f], R_LASER, 10, 10, false, f, C_CYD);
    makeSpr(SPR[SP_PULSE + 4 + f], R_FROST, 10, 10, false, f, C_CYD);
    makeSpr(SPR[SP_PULSE + 6 + f], R_BURST, 10, 10, false, f, C_AM);
  }
  makeSpr(SPR[SP_COIN], R_COIN, 7, 7, false, 0, 0);
  makeSpr(SPR[SP_HEART], R_HEART, 7, 7, false, 0, 0);
  makeSpr(SPR[SP_SKULL], R_SKULL, 7, 7, false, 0, 0);
}
// mode 0 normal, 1 solid override, 2 checker override, 3 dotted override (ghost)
static void blit(const Spr& s, int x, int y, int mode = 0, uint16_t ov = 0) {
  for (int j = 0; j < s.h; j++) {
    int py = y + j;
    if (py < 0 || py >= SH) continue;
    for (int i = 0; i < s.w; i++) {
      uint16_t c = s.px[j * s.w + i];
      if (!c) continue;
      if (mode == 1) c = ov;
      else if (mode == 2) { if (((i + j) & 1) == 0) c = ov; }
      else if (mode == 3) { if ((i + j) & 1) continue; c = ov ? ov : c; }
      cv.drawPixel(x + i, py, c);
    }
  }
}

// ============================== MAPS / WAVES =======================
struct MapDef { const char* name; const char* sub; uint8_t nwp; int8_t wp[10][2]; uint8_t pads[NPADS][2]; };
static const MapDef MAPS[NMAPS] = {
  {"GRID ZERO", "NEON MAINFRAME", 10, {{0, 2}, {4, 2}, {4, 7}, {9, 7}, {9, 2}, {14, 2}, {14, 7}, {19, 7}, {19, 3}, {21, 3}},
   {{2, 4}, {6, 5}, {6, 9}, {11, 5}, {11, 8}, {12, 0}, {7, 1}, {16, 4}, {16, 9}, {21, 5}, {21, 1}, {2, 8}}},
  {"CRYO VAULT", "FROZEN FACILITY", 10, {{0, 7}, {3, 7}, {3, 2}, {8, 2}, {8, 6}, {12, 6}, {12, 1}, {17, 1}, {17, 5}, {21, 5}},
   {{1, 5}, {5, 4}, {6, 0}, {1, 9}, {6, 8}, {10, 4}, {10, 8}, {14, 3}, {14, 7}, {19, 2}, {19, 7}, {20, 3}}},
  {"MOLTEN CORE", "LAVA FORGE", 10, {{0, 1}, {6, 1}, {6, 5}, {2, 5}, {2, 8}, {11, 8}, {11, 3}, {16, 3}, {16, 7}, {21, 7}},
   {{3, 3}, {8, 3}, {8, 6}, {4, 7}, {0, 7}, {0, 3}, {13, 5}, {14, 1}, {14, 6}, {13, 8}, {18, 5}, {19, 9}}},
  {"TOXIC BOG", "POISON SWAMP", 8, {{0, 5}, {5, 5}, {5, 1}, {10, 1}, {10, 8}, {15, 8}, {15, 2}, {21, 2}},
   {{2, 3}, {2, 7}, {7, 3}, {7, 6}, {8, 8}, {12, 4}, {12, 6}, {12, 1}, {17, 4}, {17, 7}, {19, 4}, {19, 0}}},
  {"VOID STATION", "DEEP SPACE RIFT", 10, {{0, 4}, {4, 4}, {4, 8}, {9, 8}, {9, 1}, {14, 1}, {14, 6}, {18, 6}, {18, 3}, {21, 3}},
   {{2, 2}, {2, 6}, {6, 6}, {7, 4}, {6, 2}, {11, 4}, {11, 7}, {12, 3}, {16, 3}, {16, 8}, {20, 5}, {20, 1}}}
};
struct Grp { uint8_t type, count; float gap; };
#define Z {0, 0, 0}
static const Grp WVT[NMAPS][NWAVES][3] = {
  {  // GRID ZERO
    {{0, 6, 1.1f}, Z, Z}, {{0, 9, 0.9f}, Z, Z}, {{0, 6, 0.9f}, {2, 8, 0.35f}, Z}, {{1, 3, 2.0f}, {0, 6, 0.8f}, Z},
    {{4, 1, 1.0f}, {2, 10, 0.3f}, Z}, {{3, 5, 1.2f}, {0, 8, 0.6f}, Z}, {{1, 5, 1.6f}, {3, 4, 1.0f}, Z},
    {{2, 16, 0.25f}, {1, 4, 1.5f}, {3, 4, 0.9f}}, {{3, 8, 0.9f}, {0, 10, 0.5f}, Z}, {{4, 1, 1.0f}, {0, 8, 0.7f}, {1, 3, 1.5f}},
    {{1, 6, 1.2f}, {2, 14, 0.25f}, Z}, {{3, 10, 0.7f}, {1, 4, 1.3f}, Z}, {{0, 16, 0.35f}, {3, 8, 0.7f}, {1, 6, 1.1f}},
    {{2, 20, 0.2f}, {1, 8, 1.0f}, {3, 8, 0.6f}}, {{4, 1, 1.0f}, {1, 8, 0.9f}, {3, 8, 0.6f}}
  },
  {  // CRYO VAULT
    {{0, 8, 0.8f}, Z, Z}, {{0, 6, 0.8f}, {2, 10, 0.3f}, Z}, {{1, 3, 2.0f}, {0, 6, 0.7f}, Z}, {{3, 5, 1.1f}, {0, 8, 0.6f}, Z},
    {{4, 1, 1.0f}, {2, 12, 0.3f}, Z}, {{1, 5, 1.6f}, {3, 6, 0.9f}, Z}, {{2, 18, 0.22f}, {1, 4, 1.5f}, Z},
    {{3, 8, 0.8f}, {0, 10, 0.5f}, {1, 3, 1.5f}}, {{1, 6, 1.3f}, {2, 16, 0.22f}, {3, 4, 0.9f}}, {{4, 1, 1.0f}, {0, 10, 0.6f}, {1, 4, 1.4f}},
    {{3, 10, 0.7f}, {2, 14, 0.24f}, Z}, {{1, 8, 1.1f}, {0, 14, 0.4f}, Z}, {{2, 22, 0.2f}, {1, 6, 1.2f}, {3, 8, 0.7f}},
    {{0, 18, 0.35f}, {3, 10, 0.6f}, {1, 8, 1.0f}}, {{4, 1, 1.0f}, {1, 10, 0.9f}, {3, 10, 0.6f}}
  },
  {  // MOLTEN CORE
    {{0, 7, 1.0f}, Z, Z}, {{0, 8, 0.8f}, {2, 8, 0.3f}, Z}, {{1, 3, 2.0f}, {0, 8, 0.7f}, Z}, {{3, 6, 1.0f}, {2, 10, 0.28f}, Z},
    {{4, 1, 1.0f}, {0, 8, 0.6f}, Z}, {{1, 5, 1.5f}, {3, 6, 0.9f}, Z}, {{0, 14, 0.45f}, {1, 4, 1.4f}, Z},
    {{2, 20, 0.22f}, {3, 8, 0.8f}, Z}, {{1, 7, 1.2f}, {0, 10, 0.5f}, {3, 5, 0.9f}}, {{4, 1, 1.0f}, {2, 14, 0.25f}, {1, 3, 1.4f}},
    {{0, 16, 0.4f}, {1, 6, 1.2f}, Z}, {{3, 10, 0.7f}, {2, 16, 0.22f}, Z}, {{1, 8, 1.0f}, {0, 14, 0.4f}, {3, 8, 0.7f}},
    {{2, 24, 0.18f}, {1, 8, 1.0f}, {0, 10, 0.4f}}, {{4, 1, 1.0f}, {1, 10, 0.9f}, {0, 12, 0.4f}}
  },
  {  // TOXIC BOG
    {{0, 8, 0.9f}, Z, Z}, {{2, 6, 0.7f}, {0, 6, 0.8f}, Z}, {{1, 3, 2.2f}, {0, 7, 0.7f}, Z}, {{3, 5, 1.1f}, {2, 8, 0.6f}, Z},
    {{4, 1, 1.0f}, {0, 8, 0.6f}, Z}, {{1, 5, 1.6f}, {2, 8, 0.6f}, Z}, {{3, 8, 0.9f}, {0, 10, 0.5f}, Z},
    {{2, 12, 0.5f}, {1, 5, 1.4f}, {3, 5, 0.9f}}, {{1, 8, 1.2f}, {0, 12, 0.4f}, Z}, {{4, 1, 1.0f}, {2, 10, 0.5f}, {3, 6, 0.8f}},
    {{1, 8, 1.1f}, {2, 12, 0.45f}, Z}, {{3, 10, 0.7f}, {0, 14, 0.4f}, {1, 4, 1.3f}}, {{2, 16, 0.4f}, {1, 8, 1.0f}, {3, 8, 0.7f}},
    {{0, 18, 0.3f}, {1, 8, 1.0f}, {2, 12, 0.4f}}, {{4, 1, 1.0f}, {1, 10, 0.9f}, {2, 14, 0.4f}}
  },
  {  // VOID STATION
    {{0, 7, 1.0f}, Z, Z}, {{2, 12, 0.25f}, {0, 6, 0.9f}, Z}, {{1, 3, 2.0f}, {0, 8, 0.7f}, Z}, {{3, 6, 1.0f}, {2, 12, 0.25f}, Z},
    {{4, 1, 1.0f}, {0, 8, 0.6f}, Z}, {{1, 6, 1.4f}, {3, 6, 0.9f}, Z}, {{2, 18, 0.2f}, {1, 5, 1.4f}, {3, 4, 1.0f}},
    {{0, 14, 0.4f}, {3, 8, 0.8f}, Z}, {{1, 8, 1.2f}, {2, 16, 0.2f}, {3, 6, 0.8f}}, {{4, 1, 1.0f}, {0, 10, 0.5f}, {1, 4, 1.4f}},
    {{3, 10, 0.7f}, {2, 18, 0.2f}, Z}, {{1, 8, 1.1f}, {0, 14, 0.4f}, {3, 6, 0.8f}}, {{2, 22, 0.18f}, {1, 8, 1.0f}, {3, 10, 0.6f}},
    {{0, 18, 0.3f}, {3, 12, 0.6f}, {1, 8, 1.0f}}, {{4, 1, 1.0f}, {1, 10, 0.9f}, {3, 12, 0.55f}}
  }
};
static const uint8_t MAPCI[NMAPS][2] = {{CI_AM, CI_RD}, {CI_ICE, CI_CY}, {CI_OR, CI_AM}, {CI_LM, CI_G}, {CI_VI, CI_PK}};
static const float MAPHP[NMAPS] = {1.0f, 1.1f, 1.2f, 1.32f, 1.45f};

// ============================== GAME DATA ==========================
enum State { ST_TITLE, ST_MAPS, ST_PLAY, ST_PAUSE, ST_WIN, ST_LOSE };
static State state = ST_TITLE;
static int curMap = 0, selMap = 0, wave = 0, coreHp = CORE_MAX, coins = 0, kills = 0, curPad = 0, selType = 0, pauseSel = 0;
static uint32_t score = 0, hiMap[NMAPS];
static uint8_t clearMask = 0;
static bool waveActive = false, spawnDone = true, sndOn = true;
static int gi = 0, gc = 0;
static float spT = 0, shakeT = 0, coreFlash = 0, msgT = 0, stateT = 0;
static char msgBuf[32] = "";
static uint8_t genCtr = 0;

static uint8_t PADS[NPADS][2];
static int8_t WP[10][2];
static int nWP = 10, coreCY = MAPY + 35;
static float wX[10], wY[10], wCum[10], pathLen = 0;
static bool pathMask[MAPH][MAPW];

struct TDef { const char* name; int cost; float dmg[3], rate[3], range[3], aux[3]; };
static const TDef TD[4] = {
  {"PULSE", 40, {5, 8, 12}, {0.28f, 0.24f, 0.18f}, {34, 38, 42}, {0, 0, 0}},
  {"LASER", 70, {32, 52, 80}, {1.6f, 1.4f, 1.2f}, {40, 46, 52}, {1.6f, 1.8f, 2.0f}},
  {"FROST", 55, {3, 5, 8}, {0.9f, 0.8f, 0.7f}, {30, 34, 38}, {0.55f, 0.45f, 0.35f}},
  {"BURST", 80, {16, 26, 40}, {1.4f, 1.25f, 1.1f}, {44, 48, 52}, {16, 20, 24}}
};
struct Tower { bool built, overheated; uint8_t type, lvl; int8_t beamTarget; int invested; float cd, heat, coolT, flash, disabledT, aim, bex, bey; };
static Tower tw[NPADS];
struct Enemy { bool act, vis; uint8_t type, gen, anim, bossPhase; float d, x, y, hp, maxhp, slowT, slowF, freezeT, flash, dying, age, t1, dashT, shieldT, lat; };
static Enemy en[MAXE];
struct Proj { bool act; uint8_t kind, tgen; int8_t target; float x, y, tx, ty, spd, dmg, splash, life, trX[3], trY[3]; };
static Proj pr[MAXP];
struct Part { bool act; float x, y, vx, vy, life, maxl; uint8_t col, sz; };
static Part pt[MAXPART];
struct Ring { bool act; float x, y, maxr, life, maxl; uint8_t col; };
static Ring rg[MAXRING];
struct Flt { bool act; float x, y, life; int v; };
static Flt fl[MAXFLT];
struct Decor { uint8_t x, y, t; };
static Decor decor[90];
static int nDecor = 0;

static float frnd() { return random(0, 1000) / 1000.f; }
static bool isPad(int x, int y) { for (int i = 0; i < NPADS; i++) if (PADS[i][0] == x && PADS[i][1] == y) return true; return false; }
static const Species& SPe(int type) { return SPC[curMap][type]; }

static void initPath() {
  for (int i = 0; i < nWP; i++) { wX[i] = WP[i][0] * TILE + TILE / 2; wY[i] = MAPY + WP[i][1] * TILE + TILE / 2; }
  wCum[0] = 0;
  for (int i = 1; i < nWP; i++) wCum[i] = wCum[i - 1] + fabsf(wX[i] - wX[i - 1]) + fabsf(wY[i] - wY[i - 1]);
  pathLen = wCum[nWP - 1];
  memset(pathMask, 0, sizeof(pathMask));
  for (int i = 0; i < nWP - 1; i++) {
    int x0 = WP[i][0] < WP[i + 1][0] ? WP[i][0] : WP[i + 1][0], x1 = WP[i][0] < WP[i + 1][0] ? WP[i + 1][0] : WP[i][0];
    int y0 = WP[i][1] < WP[i + 1][1] ? WP[i][1] : WP[i + 1][1], y1 = WP[i][1] < WP[i + 1][1] ? WP[i + 1][1] : WP[i][1];
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) pathMask[y][x] = true;
  }
  coreCY = MAPY + WP[nWP - 1][1] * TILE + 5;
  uint32_t s = 20241118u + curMap * 7919u;
  nDecor = 0;
  for (int y = 0; y < MAPH; y++)
    for (int x = 0; x < MAPW; x++) {
      if (pathMask[y][x] || isPad(x, y) || x == 0 || x >= 22) continue;
      s = s * 1664525u + 1013904223u;
      uint32_t r = s >> 16;
      if (r % 100 < 30 && nDecor < 90) { decor[nDecor].x = (uint8_t)x; decor[nDecor].y = (uint8_t)y; decor[nDecor].t = (uint8_t)((r >> 8) % 4); nDecor++; }
    }
}
static void loadMap(int m) {
  curMap = m;
  const MapDef& M = MAPS[m];
  nWP = M.nwp;
  for (int i = 0; i < 10; i++) { WP[i][0] = M.wp[i][0]; WP[i][1] = M.wp[i][1]; }
  for (int i = 0; i < NPADS; i++) { PADS[i][0] = M.pads[i][0]; PADS[i][1] = M.pads[i][1]; }
  setTheme(m);
  initPath();
  buildEnemySprites(m);
}
static void previewMap(int m) { setTheme(m); buildEnemySprites(m); }
static void posAtDir(float d, float& x, float& y, float& dx, float& dy) {
  if (d < 0.01f) d = 0.01f;
  if (d > pathLen - 0.01f) d = pathLen - 0.01f;
  int i = 1;
  while (i < nWP - 1 && wCum[i] < d) i++;
  float t = (d - wCum[i - 1]) / (wCum[i] - wCum[i - 1]);
  float sx = wX[i] - wX[i - 1], sy = wY[i] - wY[i - 1], L = fabsf(sx) + fabsf(sy);
  x = wX[i - 1] + sx * t; y = wY[i - 1] + sy * t; dx = sx / L; dy = sy / L;
}
static void posAt(float d, float& x, float& y) { float a, b; posAtDir(d, x, y, a, b); }
static int upCost(int type, int lvl) { return lvl == 1 ? TD[type].cost * 7 / 10 : TD[type].cost; }
static void setMsg(const char* s) { strncpy(msgBuf, s, 31); msgBuf[31] = 0; msgT = 1.8f; }
static float eRad(const Enemy& e) { return SPe(e.type).w * 0.42f + 0.6f; }
static bool targetable(const Enemy& e) { return e.act && e.dying <= 0 && !(SPe(e.type).ab == AB_PHASE && !e.vis); }
static float armorOf(const Enemy& e) { return SPe(e.type).armor + (e.shieldT > 0 ? 8.0f : 0.0f); }

// ============================== AUDIO SYSTEM =======================
enum { SFX_SHOT, SFX_HIT, SFX_LASER, SFX_FROST, SFX_BURST, SFX_EXPLODE, SFX_KILL, SFX_BUILD, SFX_UP, SFX_WAVE,
       SFX_CORE, SFX_WIN, SFX_LOSE, SFX_DENY, SFX_SELL, SFX_MOVE, SFX_N };
static uint32_t lastSfx[SFX_N];
static int sfxCh = 1;
static void tn(float f, int ms, bool queue = false) { if (sndOn) M5Cardputer.Speaker.tone(f, ms, sfxCh, !queue); }
static void sfx(int id) {
  static const uint16_t gap[SFX_N] = {45, 30, 70, 90, 60, 60, 35, 0, 0, 0, 0, 0, 0, 0, 0, 40};
  uint32_t now = millis();
  if (now - lastSfx[id] < gap[id]) return;
  lastSfx[id] = now;
  sfxCh = (sfxCh % 3) + 1;
  switch (id) {
    case SFX_SHOT:   tn(1500, 12); tn(900, 14, true); break;
    case SFX_HIT:    tn(420, 10); break;
    case SFX_LASER:  tn(1100, 22); break;
    case SFX_FROST:  tn(1900, 25); tn(1300, 40, true); break;
    case SFX_BURST:  tn(260, 45); break;
    case SFX_EXPLODE: tn(170, 50); tn(110, 70, true); tn(70, 90, true); break;
    case SFX_KILL:   tn(800, 15); tn(1200, 25, true); break;
    case SFX_BUILD:  tn(600, 40); tn(900, 40, true); tn(1300, 60, true); break;
    case SFX_UP:     tn(700, 35); tn(950, 35, true); tn(1250, 35, true); tn(1700, 70, true); break;
    case SFX_WAVE:   tn(440, 80); tn(660, 80, true); tn(880, 170, true); break;
    case SFX_CORE:   tn(200, 120); tn(140, 180, true); break;
    case SFX_WIN:    tn(523, 110); tn(659, 110, true); tn(784, 110, true); tn(1047, 260, true); break;
    case SFX_LOSE:   tn(392, 160); tn(330, 160, true); tn(262, 200, true); tn(165, 420, true); break;
    case SFX_DENY:   tn(180, 70); break;
    case SFX_SELL:   tn(900, 30); tn(500, 55, true); break;
    case SFX_MOVE:   tn(1700, 8); break;
  }
}

// ============================== SAVE SYSTEM ========================
static void saveLoad() {
  for (int i = 0; i < NMAPS; i++) { char k[8]; snprintf(k, sizeof(k), "hs%d", i); hiMap[i] = prefs.getUInt(k, 0); }
  clearMask = prefs.getUChar("cl", 0);
  sndOn = prefs.getBool("snd", true);
}
static void saveScore(bool win) {
  if (score > hiMap[curMap]) { hiMap[curMap] = score; char k[8]; snprintf(k, sizeof(k), "hs%d", curMap); prefs.putUInt(k, score); }
  if (win) { clearMask |= (uint8_t)(1 << curMap); prefs.putUChar("cl", clearMask); }
}

// ============================== PARTICLE SYSTEM ====================
static void addPart(float x, float y, float vx, float vy, float life, uint8_t col, uint8_t sz = 1) {
  for (auto& p : pt) if (!p.act) { p.act = true; p.x = x; p.y = y; p.vx = vx; p.vy = vy; p.life = p.maxl = life; p.col = col; p.sz = sz; return; }
}
static void burstPart(float x, float y, int n, uint8_t col, float spd, float life) {
  for (int i = 0; i < n; i++) {
    float a = random(0, 628) / 100.f, s = spd * (0.35f + random(0, 65) / 100.f);
    addPart(x, y, cosf(a) * s, sinf(a) * s, life * (0.6f + random(0, 40) / 100.f), col, (random(0, 4) == 0) ? 2 : 1);
  }
}
static void addRing(float x, float y, float maxr, float life, uint8_t col) {
  for (auto& r : rg) if (!r.act) { r.act = true; r.x = x; r.y = y; r.maxr = maxr; r.life = r.maxl = life; r.col = col; return; }
}
static void addFloat(float x, float y, int v) {
  for (auto& f : fl) if (!f.act) { f.act = true; f.x = x; f.y = y; f.life = 0.9f; f.v = v; return; }
}
static void updateFx(float dt) {
  for (auto& p : pt) if (p.act) {
    p.life -= dt;
    if (p.life <= 0) { p.act = false; continue; }
    p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 0.95f; p.vy *= 0.95f;
  }
  for (auto& r : rg) if (r.act) { r.life -= dt; if (r.life <= 0) r.act = false; }
  for (auto& f : fl) if (f.act) { f.life -= dt; f.y -= 14 * dt; if (f.life <= 0) f.act = false; }
}

// ============================== COLLISION HELPERS ==================
// closest approach of segment (x0,y0)-(x1,y1) to circle; tHit in 0..1 along the segment
static bool segCircle(float x0, float y0, float x1, float y1, float cx, float cy, float r, float& tHit) {
  float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, t = 0;
  if (l2 > 0.0001f) {
    t = ((cx - x0) * dx + (cy - y0) * dy) / l2;
    if (t < 0) t = 0; else if (t > 1) t = 1;
  }
  float px = x0 + dx * t - cx, py = y0 + dy * t - cy;
  if (px * px + py * py <= r * r) { tHit = t; return true; }
  return false;
}

// ============================== ENEMY SYSTEM =======================
static int spawnEnemy(uint8_t type, float d, int wv) {
  for (int i = 0; i < MAXE; i++) if (!en[i].act) {
    Enemy& e = en[i];
    memset(&e, 0, sizeof(e));
    e.act = true; e.type = type; e.gen = ++genCtr; e.d = d; e.vis = true; e.slowF = 1; e.bossPhase = 1; e.t1 = 2.0f + frnd() * 2.0f;
    e.lat = (type == 4) ? 0 : (float)random(-2, 3);
    e.maxhp = SPe(type).hp * MAPHP[curMap] * (1.0f + 0.10f * (wv - 1)); e.hp = e.maxhp;
    float dx, dy; posAtDir(d, e.x, e.y, dx, dy);
    e.x -= dy * e.lat; e.y += dx * e.lat;
    if (type == 4) { char b[32]; snprintf(b, sizeof(b), "BOSS: %s", SPe(4).name); setMsg(b); }
    return i;
  }
  return -1;
}
static void killEnemy(int i) {
  Enemy& e = en[i];
  if (!e.act || e.dying > 0) return;
  e.dying = 0.22f;
  const Species& S = SPe(e.type);
  int rw = S.reward;
  if (e.bossPhase == 9) { rw /= 2; if (rw < 1) rw = 1; }
  coins += rw; kills++; score += (uint32_t)rw * 5;
  uint8_t col = (e.type == 1 || e.type == 4) ? MAPCI[curMap][1] : MAPCI[curMap][0];
  burstPart(e.x, e.y, e.type == 4 ? 44 : (e.type == 2 ? 5 : 14), col, e.type == 4 ? 90 : 60, 0.55f);
  addFloat(e.x, e.y - 8, rw);
  if (S.ab == AB_SPLIT && e.bossPhase != 9) {
    for (int k = 0; k < 2; k++) {
      int j = spawnEnemy(e.type, fmaxf(0, e.d - 3 + k * 6), wave);
      if (j >= 0) { en[j].bossPhase = 9; en[j].maxhp *= 0.5f; en[j].hp = en[j].maxhp; en[j].lat = (k == 0) ? -2.0f : 2.0f; }
    }
  }
  if (e.type == 4) {
    addRing(e.x, e.y, 40, 0.5f, col); addRing(e.x, e.y, 26, 0.4f, CI_WH);
    shakeT = fmaxf(shakeT, 0.35f); sfx(SFX_EXPLODE);
  } else sfx(SFX_KILL);
}
static void rawHit(int i, float amt) {
  Enemy& e = en[i];
  if (!e.act || e.dying > 0) return;
  e.hp -= amt; e.flash = 0.07f;
  if (e.hp <= 0) killEnemy(i);
}
static void hitEnemy(int i, float dmg) {
  float eff = dmg - armorOf(en[i]);
  if (eff < 1) eff = 1;
  rawHit(i, eff);
}
static int aliveCount() { int n = 0; for (auto& e : en) if (e.act && e.dying <= 0) n++; return n; }
static void updateEnemies(float dt) {
  for (int i = 0; i < MAXE; i++) {
    Enemy& e = en[i];
    if (!e.act) continue;
    if (e.dying > 0) { e.dying -= dt; if (e.dying <= 0) e.act = false; continue; }
    const Species& S = SPe(e.type);
    e.age += dt;
    if (e.flash > 0) e.flash -= dt;
    float mul = 1.0f;
    if (e.freezeT > 0) { e.freezeT -= dt; mul = 0; }
    else if (e.slowT > 0) { e.slowT -= dt; mul = e.slowF; }
    switch (S.ab) {
      case AB_PHASE: e.vis = fmodf(e.age, 2.6f) < 1.7f; break;
      case AB_REGEN: if (e.hp < e.maxhp) e.hp = fminf(e.maxhp, e.hp + e.maxhp * 0.03f * dt); break;
      case AB_DASH:
        e.t1 -= dt;
        if (e.t1 <= 0) { e.t1 = 2.6f; e.dashT = 0.55f; }
        if (e.dashT > 0) { e.dashT -= dt; mul *= 2.4f; if (frnd() < 0.5f) addPart(e.x, e.y, 0, 0, 0.2f, MAPCI[curMap][1], 1); }
        break;
      case AB_TELE:
        e.t1 -= dt;
        if (e.t1 <= 0) { e.t1 = 3.2f; addRing(e.x, e.y, 10, 0.25f, CI_PK); burstPart(e.x, e.y, 6, CI_PK, 30, 0.3f); e.d += 24; }
        break;
      case AB_SHIELD:
        e.t1 -= dt;
        if (e.t1 <= 0) { e.t1 = 4.0f; e.shieldT = 1.5f; }
        if (e.shieldT > 0) e.shieldT -= dt;
        break;
      case AB_BOSS: {
        float r = e.hp / e.maxhp;
        uint8_t ph = r < 0.33f ? 3 : (r < 0.66f ? 2 : 1);
        if (ph > e.bossPhase) {
          e.bossPhase = ph;
          addRing(e.x, e.y, 36, 0.5f, ph == 2 ? MAPCI[curMap][0] : MAPCI[curMap][1]);
          shakeT = fmaxf(shakeT, 0.2f); sfx(SFX_WAVE);
          if (ph == 2) { spawnEnemy(0, fmaxf(0, e.d - 10), wave); spawnEnemy(0, fmaxf(0, e.d - 22), wave); }
        }
        if (e.bossPhase == 3) {
          mul *= 1.7f;
          e.t1 -= dt;
          if (e.t1 <= 0) {
            e.t1 = 4.0f;
            addRing(e.x, e.y, 56, 0.6f, CI_CY);
            for (int k = 0; k < NPADS; k++) if (tw[k].built) {
              float dx = PADS[k][0] * TILE + 5 - e.x, dy = MAPY + PADS[k][1] * TILE + 5 - e.y;
              if (dx * dx + dy * dy < 56 * 56) tw[k].disabledT = 1.6f;
            }
          }
        }
        break;
      }
      default: break;
    }
    e.d += S.speed * mul * dt;
    e.anim = ((int)(e.d / 5)) & 1;
    if (e.d >= pathLen) {
      coreHp -= S.dmg; if (coreHp < 0) coreHp = 0;
      shakeT = fmaxf(shakeT, 0.3f); coreFlash = 0.3f; sfx(SFX_CORE);
      burstPart(CORE_X - 8, coreCY, 10, CI_RD, 50, 0.4f);
      e.act = false;
      continue;
    }
    float dx, dy;
    posAtDir(e.d, e.x, e.y, dx, dy);
    e.x -= dy * e.lat; e.y += dx * e.lat;
  }
}

// ============================== PROJECTILE SYSTEM ==================
static void fireProj(uint8_t kind, float x, float y, int target, float tx, float ty, float spd, float dmg, float splash) {
  for (auto& p : pr) if (!p.act) {
    memset(&p, 0, sizeof(p));
    p.act = true; p.kind = kind; p.x = x; p.y = y; p.tx = tx; p.ty = ty; p.spd = spd; p.dmg = dmg; p.splash = splash;
    p.target = (int8_t)target; p.tgen = target >= 0 ? en[target].gen : 0; p.life = 3.0f;
    for (int k = 0; k < 3; k++) { p.trX[k] = x; p.trY[k] = y; }
    return;
  }
}
static void explode(float x, float y, float rad, float dmg) {
  addRing(x, y, rad + 4, 0.35f, CI_AM); addRing(x, y, rad - 4, 0.25f, CI_RD);
  burstPart(x, y, 22, CI_AM, 80, 0.5f); burstPart(x, y, 12, CI_RD, 55, 0.45f);
  shakeT = fmaxf(shakeT, 0.1f); sfx(SFX_EXPLODE);
  for (int i = 0; i < MAXE; i++) {
    Enemy& e = en[i];
    if (!e.act || e.dying > 0) continue;
    float dx = e.x - x, dy = e.y - y;
    float d = sqrtf(dx * dx + dy * dy) - eRad(e);             // edge distance
    if (d < 0) d = 0;
    if (d <= rad) hitEnemy(i, dmg * (1.0f - 0.4f * d / rad));
  }
}
static void updateProj(float dt) {
  for (auto& p : pr) {
    if (!p.act) continue;
    p.life -= dt;
    if (p.life <= 0) { p.act = false; continue; }
    p.trX[2] = p.trX[1]; p.trY[2] = p.trY[1]; p.trX[1] = p.trX[0]; p.trY[1] = p.trY[0]; p.trX[0] = p.x; p.trY[0] = p.y;
    if (p.kind == 0 && p.target >= 0) {
      Enemy& e = en[p.target];
      if (targetable(e) && e.gen == p.tgen) { p.tx = e.x; p.ty = e.y; } else p.target = -1;
    }
    float dx = p.tx - p.x, dy = p.ty - p.y, dist = sqrtf(dx * dx + dy * dy), step = p.spd * dt;
    float nx = p.x, ny = p.y;
    if (dist > 0.001f) { float k = (step < dist ? step : dist) / dist; nx = p.x + dx * k; ny = p.y + dy * k; }
    // swept collision against every visible enemy circle: the first one on the way is hit
    int hit = -1; float bestT = 2.0f;
    for (int j = 0; j < MAXE; j++) {
      const Enemy& e = en[j];
      if (!targetable(e)) continue;
      float t;
      if (segCircle(p.x, p.y, nx, ny, e.x, e.y, eRad(e) + 0.8f, t) && t < bestT) { bestT = t; hit = j; }
    }
    if (hit >= 0) {
      float hx = p.x + (nx - p.x) * bestT, hy = p.y + (ny - p.y) * bestT;
      if (p.kind == 0) { hitEnemy(hit, p.dmg); burstPart(hx, hy, 4, CI_G, 40, 0.25f); sfx(SFX_HIT); }
      else explode(hx, hy, p.splash, p.dmg);
      p.act = false;
      continue;
    }
    p.x = nx; p.y = ny;
    if (dist <= step + 0.5f) {
      if (p.kind == 1) explode(p.tx, p.ty, p.splash, p.dmg); else burstPart(p.tx, p.ty, 3, CI_G, 30, 0.2f);
      p.act = false;
    }
  }
}

// ============================== TOWER SYSTEM =======================
static int findTarget(float cx, float cy, float range) {
  int best = -1; float bd = -1;
  for (int i = 0; i < MAXE; i++) {
    const Enemy& e = en[i];
    if (!targetable(e)) continue;
    float dx = e.x - cx, dy = e.y - cy, rr = range + eRad(e);
    if (dx * dx + dy * dy > rr * rr) continue;
    if (e.d > bd) { bd = e.d; best = i; }
  }
  return best;
}
static void updateTowers(float dt) {
  for (int i = 0; i < NPADS; i++) {
    Tower& t = tw[i];
    if (!t.built) continue;
    if (t.flash > 0) t.flash -= dt;
    if (t.disabledT > 0) { t.disabledT -= dt; t.beamTarget = -1; continue; }
    float cx = PADS[i][0] * TILE + 5, cy = MAPY + PADS[i][1] * TILE + 5;
    const TDef& D = TD[t.type];
    int l = t.lvl - 1;
    float range = D.range[l];
    int tg = findTarget(cx, cy, range);
    if (tg >= 0) t.aim = atan2f(en[tg].y - cy, en[tg].x - cx);
    switch (t.type) {
      case 0:   // PULSE
        t.cd -= dt;
        if (tg >= 0 && t.cd <= 0) {
          fireProj(0, cx + cosf(t.aim) * 4, cy + sinf(t.aim) * 4, tg, en[tg].x, en[tg].y, 150, D.dmg[l], 0);
          t.cd = D.rate[l]; t.flash = 0.06f; sfx(SFX_SHOT);
        }
        break;
      case 1:   // LASER: piercing beam, overheats
        if (t.overheated) {
          t.beamTarget = -1; t.coolT -= dt;
          if (frnd() < 0.25f) addPart(cx + random(-3, 4), cy - 4, 0, -14, 0.5f, CI_WH, 1);
          if (t.coolT <= 0) { t.overheated = false; t.heat = 0; }
        } else if (tg >= 0) {
          t.beamTarget = (int8_t)tg;
          float ca = cosf(t.aim), sa = sinf(t.aim);
          t.bex = cx + ca * range; t.bey = cy + sa * range;
          t.cd -= dt;
          bool sparkNow = t.cd <= 0;
          if (sparkNow) { t.cd = 0.06f; sfx(SFX_LASER); }
          for (int j = 0; j < MAXE; j++) {
            Enemy& e = en[j];
            if (!targetable(e)) continue;
            float th;
            if (segCircle(cx, cy, t.bex, t.bey, e.x, e.y, eRad(e) + 1.2f, th)) {
              rawHit(j, D.dmg[l] * dt * (1.0f - fminf(0.5f, armorOf(e) * 0.07f)));
              if (sparkNow) addPart(e.x, e.y, random(-30, 31), random(-30, 31), 0.25f, CI_CY, 1);
            }
          }
          t.heat += dt;
          if (t.heat >= D.aux[l]) { t.overheated = true; t.coolT = D.rate[l]; t.beamTarget = -1; burstPart(cx, cy, 8, CI_AM, 35, 0.4f); }
        } else { t.beamTarget = -1; t.heat = fmaxf(0, t.heat - dt * 0.9f); }
        break;
      case 2: { // FROST
        t.cd -= dt;
        if (t.cd <= 0) {
          bool any = false;
          for (int j = 0; j < MAXE; j++) {
            Enemy& e = en[j];
            if (!targetable(e)) continue;
            float dx = e.x - cx, dy = e.y - cy, rr = range + eRad(e);
            if (dx * dx + dy * dy > rr * rr) continue;
            bool wasSlow = e.slowT > 0;
            e.slowF = D.aux[l]; e.slowT = 1.5f + 0.5f * l;
            if (l == 2 && wasSlow) e.freezeT = 0.7f;
            hitEnemy(j, D.dmg[l]);
            addPart(e.x, e.y, random(-20, 21), random(-20, 21), 0.35f, CI_ICE, 1);
            any = true;
          }
          if (any) { addRing(cx, cy, range, 0.3f, CI_ICE); t.cd = D.rate[l]; t.flash = 0.1f; sfx(SFX_FROST); }
          else t.cd = 0;
        }
        break;
      }
      case 3:   // BURST
        t.cd -= dt;
        if (tg >= 0 && t.cd <= 0) {
          const Enemy& e = en[tg];
          float dx = e.x - cx, dy = e.y - cy, tt = sqrtf(dx * dx + dy * dy) / 90.f, px, py;
          posAt(fminf(pathLen, e.d + SPe(e.type).speed * tt), px, py);
          fireProj(1, cx, cy, -1, px, py, 90, D.dmg[l], D.aux[l]);
          t.cd = D.rate[l]; t.flash = 0.1f; sfx(SFX_BURST);
        }
        break;
    }
  }
}

// ============================== GAME CORE ==========================
static void newGame(int m) {
  loadMap(m);
  memset(en, 0, sizeof(en)); memset(pr, 0, sizeof(pr)); memset(pt, 0, sizeof(pt));
  memset(rg, 0, sizeof(rg)); memset(fl, 0, sizeof(fl)); memset(tw, 0, sizeof(tw));
  for (auto& t : tw) t.beamTarget = -1;
  coins = 120 + 20 * m; coreHp = CORE_MAX; kills = 0; score = 0; wave = 0; waveActive = false; spawnDone = true;
  curPad = 0; selType = 0; shakeT = coreFlash = msgT = 0; stateT = 0;
  state = ST_PLAY;
  setMsg("BUILD, THEN PRESS SPACE");
}
static void winGame() {
  score += (uint32_t)coreHp * 50 + 500;
  saveScore(true); state = ST_WIN; stateT = 0; sfx(SFX_WIN);
}
static void loseGame() { saveScore(false); state = ST_LOSE; stateT = 0; sfx(SFX_LOSE); }
static void startWave() {
  if (waveActive || wave >= NWAVES) return;
  wave++; waveActive = true; spawnDone = false; gi = 0; gc = 0; spT = 0.8f;
  sfx(SFX_WAVE); addRing(5, MAPY + WP[0][1] * TILE + 5, 30, 0.6f, CI_CY);
  setMsg((wave == 5 || wave == 10 || wave == 15) ? "BOSS WAVE!" : "WAVE INCOMING");
}
static void updateWaveSpawn(float dt) {
  if (!waveActive || spawnDone) return;
  spT -= dt;
  if (spT > 0) return;
  const Grp& g = WVT[curMap][wave - 1][gi];
  if (g.count == 0) { spawnDone = true; return; }
  spawnEnemy(g.type, 0, wave);
  gc++;
  if (gc >= g.count) {
    gi++; gc = 0; spT = 2.0f;
    if (gi >= 3 || WVT[curMap][wave - 1][gi].count == 0) spawnDone = true;
  } else spT = g.gap;
}
static void completeWave() {
  waveActive = false;
  int bonus = 20 + wave * 5;
  coins += bonus; score += (uint32_t)wave * 50;
  if (wave >= NWAVES) { winGame(); return; }
  char b[32]; snprintf(b, sizeof(b), "WAVE CLEAR +%d", bonus);
  setMsg(b); sfx(SFX_UP);
}
static void updateGame(float dt) {
  if (shakeT > 0) shakeT -= dt;
  if (coreFlash > 0) coreFlash -= dt;
  if (msgT > 0) msgT -= dt;
  updateWaveSpawn(dt); updateEnemies(dt); updateTowers(dt); updateProj(dt); updateFx(dt);
  if (coreHp <= 0) { loseGame(); return; }
  if (waveActive && spawnDone && aliveCount() == 0) completeWave();
}
static void towerCenter(int i, float& cx, float& cy) { cx = PADS[i][0] * TILE + 5; cy = MAPY + PADS[i][1] * TILE + 5; }
static void upgradeTower() {
  Tower& t = tw[curPad];
  if (!t.built) return;
  if (t.lvl >= 3) { setMsg("MAX LEVEL"); sfx(SFX_DENY); return; }
  int cost = upCost(t.type, t.lvl);
  if (coins < cost) { setMsg("NOT ENOUGH ENERGY"); sfx(SFX_DENY); return; }
  coins -= cost; t.invested += cost; t.lvl++;
  float cx, cy; towerCenter(curPad, cx, cy);
  addRing(cx, cy, 16, 0.4f, CI_AM); burstPart(cx, cy, 14, CI_AM, 50, 0.45f); sfx(SFX_UP);
}
static void actPad() {
  Tower& t = tw[curPad];
  if (t.built) { upgradeTower(); return; }
  int cost = TD[selType].cost;
  if (coins < cost) { setMsg("NOT ENOUGH ENERGY"); sfx(SFX_DENY); return; }
  coins -= cost;
  memset(&t, 0, sizeof(t));
  t.built = true; t.type = (uint8_t)selType; t.lvl = 1; t.invested = cost; t.beamTarget = -1; t.aim = -1.57f;
  float cx, cy; towerCenter(curPad, cx, cy);
  addRing(cx, cy, 14, 0.4f, CI_G); burstPart(cx, cy, 12, CI_G, 40, 0.4f); sfx(SFX_BUILD);
}
static void sellTower() {
  Tower& t = tw[curPad];
  if (!t.built) return;
  int refund = t.invested * 7 / 10;
  coins += refund; t.built = false;
  float cx, cy; towerCenter(curPad, cx, cy);
  burstPart(cx, cy, 14, CI_AM, 45, 0.4f); sfx(SFX_SELL);
  char b[32]; snprintf(b, sizeof(b), "SOLD +%d", refund); setMsg(b);
}
static void movePad(int dx, int dy) {
  float cx = PADS[curPad][0], cy = PADS[curPad][1];
  int best = -1; float bs = 1e9f;
  for (int i = 0; i < NPADS; i++) {
    if (i == curPad) continue;
    float ddx = PADS[i][0] - cx, ddy = PADS[i][1] - cy, along = ddx * dx + ddy * dy;
    if (along <= 0) continue;
    float perp = dx != 0 ? fabsf(ddy) : fabsf(ddx), sc2 = along + 2.2f * perp;
    if (sc2 < bs) { bs = sc2; best = i; }
  }
  if (best >= 0) { curPad = best; sfx(SFX_MOVE); }
}

// ============================== RENDERER ===========================
static void drawDecor(const Decor& d) {
  int x = d.x * TILE, y = MAPY + d.y * TILE;
  uint16_t a = TH.dec1, b = TH.dec2;
  switch (curMap * 4 + d.t) {
    case 0: cv.drawPixel(x + 2, y + 2, b); cv.drawPixel(x + 7, y + 6, a); cv.drawPixel(x + 4, y + 8, a); break;
    case 1: cv.drawFastHLine(x + 1, y + 5, 7, a); cv.fillRect(x + 7, y + 4, 2, 2, b); break;
    case 2: cv.drawFastVLine(x + 5, y + 1, 7, a); cv.fillRect(x + 4, y + 1, 2, 2, b); break;
    case 3: cv.drawRect(x + 2, y + 2, 6, 6, a); cv.drawPixel(x + 5, y + 5, b); cv.drawPixel(x + 1, y + 4, a); cv.drawPixel(x + 8, y + 5, a); break;
    case 4: cv.drawPixel(x + 5, y + 5, b); cv.drawPixel(x + 4, y + 5, a); cv.drawPixel(x + 6, y + 5, a); cv.drawPixel(x + 5, y + 4, a); cv.drawPixel(x + 5, y + 6, a); break;
    case 5: cv.fillTriangle(x + 5, y + 2, x + 3, y + 8, x + 7, y + 8, a); cv.drawFastVLine(x + 5, y + 4, 3, b); break;
    case 6: cv.drawLine(x + 2, y + 8, x + 8, y + 2, a); cv.drawPixel(x + 8, y + 2, b); break;
    case 7: cv.drawPixel(x + 2, y + 3, b); cv.drawPixel(x + 6, y + 7, a); cv.drawPixel(x + 8, y + 2, a); cv.drawPixel(x + 3, y + 8, a); break;
    case 8: cv.drawLine(x + 1, y + 3, x + 4, y + 6, a); cv.drawLine(x + 4, y + 6, x + 8, y + 4, a); cv.drawPixel(x + 4, y + 6, b); break;
    case 9: cv.fillRect(x + 2, y + 4, 5, 3, a); cv.drawFastHLine(x + 3, y + 3, 3, a); cv.drawPixel(x + 3, y + 5, b); break;
    case 10: cv.drawPixel(x + 4, y + 5, b); cv.drawPixel(x + 5, y + 5, b); cv.drawPixel(x + 4, y + 4, a); cv.drawPixel(x + 7, y + 8, a); break;
    case 11: cv.fillEllipse(x + 5, y + 6, 3, 2, a); cv.fillEllipse(x + 5, y + 6, 1, 1, b); break;
    case 12: cv.drawFastVLine(x + 3, y + 3, 5, a); cv.drawFastVLine(x + 6, y + 2, 6, a); cv.drawPixel(x + 3, y + 3, b); cv.drawPixel(x + 6, y + 2, b); break;
    case 13: cv.drawCircle(x + 5, y + 5, 2, a); cv.drawPixel(x + 4, y + 4, b); break;
    case 14: cv.fillEllipse(x + 5, y + 6, 3, 1, a); cv.drawPixel(x + 5, y + 5, b); break;
    case 15: cv.fillRect(x + 4, y + 6, 2, 3, a); cv.fillRect(x + 2, y + 4, 6, 2, b); break;
    case 16: cv.drawPixel(x + 5, y + 5, b); cv.drawPixel(x + 4, y + 5, a); cv.drawPixel(x + 6, y + 5, a); cv.drawPixel(x + 5, y + 4, a); cv.drawPixel(x + 5, y + 6, a); break;
    case 17: cv.drawCircle(x + 5, y + 5, 3, a); cv.drawPixel(x + 8, y + 5, b); break;
    case 18: cv.drawLine(x + 2, y + 3, x + 7, y + 6, a); cv.drawPixel(x + 7, y + 6, b); cv.drawPixel(x + 3, y + 8, a); break;
    default: cv.drawRect(x + 2, y + 3, 6, 4, a); cv.drawPixel(x + 4, y + 5, b); break;
  }
}
static void drawAmbient(uint32_t now) {
  float t = now * 0.001f;
  for (int i = 0; i < 16; i++) {
    uint32_t h = (uint32_t)(i + 1) * 2654435761u;
    float fx = ((h >> 8) & 255) / 255.f, fy = ((h >> 16) & 255) / 255.f, x, y;
    uint16_t c;
    switch (curMap) {
      case 1: y = fmodf(fy * 100 + t * (8 + (i % 3) * 4), 100.f); x = fmodf(fx * 240 + sinf(t + i) * 6 + 240, 240.f); c = TH.dec2; break;
      case 2: y = 100 - fmodf(fy * 100 + t * (10 + (i % 4) * 5), 100.f); x = fmodf(fx * 240 + sinf(t * 2 + i) * 4 + 240, 240.f); c = (i & 1) ? C_OR : C_AM; break;
      case 3: y = fmodf(fy * 100 + sinf(t * 0.7f + i) * 8 + 100, 100.f); x = fmodf(fx * 240 + t * (4 + (i % 3) * 2), 240.f); c = TH.dec2; break;
      case 4: x = fx * 240; y = fy * 100; c = (((now / 200) + i) % 5 == 0) ? C_WH : TH.dec1; break;
      default: x = fx * 240; y = fmodf(fy * 100 + t * 6, 100.f); c = TH.dec1; break;
    }
    cv.drawPixel((int)x, MAPY + (int)y, c);
  }
}
static void drawBackground(uint32_t now) {
  for (int y = MAPY; y < MAPY + MAPH * TILE; y++) cv.drawFastHLine(0, y, SW, (y & 1) ? TH.bg2 : TH.bg);
  for (int ty = 0; ty < MAPH; ty++) for (int tx = 0; tx < MAPW; tx++) cv.drawPixel(tx * TILE, MAPY + ty * TILE, TH.trace);
  for (int i = 0; i < nDecor; i++) drawDecor(decor[i]);
  for (int ty = 0; ty < MAPH; ty++)
    for (int tx = 0; tx < MAPW; tx++) {
      if (!pathMask[ty][tx]) continue;
      int x = tx * TILE, y = MAPY + ty * TILE;
      cv.fillRect(x, y, TILE, TILE, TH.path);
      if (ty == 0 || !pathMask[ty - 1][tx]) cv.drawFastHLine(x, y, TILE, TH.edge);
      if (ty == MAPH - 1 || !pathMask[ty + 1][tx]) cv.drawFastHLine(x, y + TILE - 1, TILE, TH.edge);
      if (tx > 0 && !pathMask[ty][tx - 1]) cv.drawFastVLine(x, y, TILE, TH.edge);
      if (tx == MAPW - 1 || !pathMask[ty][tx + 1]) cv.drawFastVLine(x + TILE - 1, y, TILE, TH.edge);
    }
  float off = fmodf(now * 0.03f, 14.0f), px, py;
  for (float d = off; d < pathLen; d += 14.0f) {
    posAt(d, px, py);
    cv.fillRect((int)px - 1, (int)py - 1, 2, 2, TH.edge); cv.drawPixel((int)px, (int)py, TH.glow);
  }
  int by = MAPY + (int)((now / 30) % (MAPH * TILE + 30)) - 15;
  if (by >= MAPY && by < MAPY + MAPH * TILE) cv.drawFastHLine(0, by, SW, TH.trace);
  drawAmbient(now);
  for (int i = 0; i < NPADS; i++) blit(SPR[SP_PAD], PADS[i][0] * TILE, MAPY + PADS[i][1] * TILE);
}
static void drawPortal(uint32_t now) {
  int cx = 5, cy = MAPY + WP[0][1] * TILE + 5;
  cv.fillRect(0, cy - 13, 3, 26, TH.path); cv.drawFastVLine(3, cy - 13, 26, TH.edge);
  for (int k = 0; k < 3; k++) {
    float ph = fmodf(now * 0.004f + k * 0.33f, 1.0f);
    cv.drawCircle(cx, cy, 2 + (int)(ph * 9), mixc(TH.glow, ph < 0.4f ? 1.0f : (ph < 0.7f ? 0.6f : 0.35f)));
  }
  cv.fillCircle(cx, cy, 2, TH.glow);
}
static void drawCore(uint32_t now) {
  float pul = 0.5f + 0.5f * sinf(now * 0.006f), hr = (float)coreHp / CORE_MAX;
  uint16_t col = hr > 0.5f ? C_G : (hr > 0.25f ? C_AM : C_RD);
  int cx = CORE_X, cy = coreCY;
  cv.fillRect(cx - 10, cy - 16, 20, 32, TH.path); cv.drawRect(cx - 10, cy - 16, 20, 32, TH.edge);
  cv.fillRect(cx - 10, cy - 12, 3, 8, TH.edge); cv.fillRect(cx + 7, cy - 12, 3, 8, TH.edge);
  cv.fillRect(cx - 10, cy + 4, 3, 8, TH.edge); cv.fillRect(cx + 7, cy + 4, 3, 8, TH.edge);
  cv.drawCircle(cx, cy, (int)(9 + pul * 2), mixc(col, 0.35f));
  cv.drawCircle(cx, cy, (int)(11 + pul * 2), mixc(col, 0.18f));
  cv.fillCircle(cx, cy, 7, mixc(col, 0.5f)); cv.fillCircle(cx, cy, 5 + (pul > 0.5f ? 1 : 0), col); cv.fillCircle(cx, cy, 2, C_WH);
  if (hr < 0.5f) { cv.drawLine(cx - 5, cy - 8, cx - 1, cy - 1, C_BG); cv.drawLine(cx + 4, cy + 7, cx + 1, cy + 1, C_BG); }
  if (coreFlash > 0 && ((now >> 5) & 1)) cv.fillCircle(cx, cy, 9, C_WH);
}
static void drawRange(float cx, float cy, float range, uint32_t now) {
  cv.drawCircle((int)cx, (int)cy, (int)range, C_CYD);
  for (int k = 0; k < 10; k++) {
    float a = k * 0.628f + now * 0.0008f;
    cv.drawPixel((int)(cx + cosf(a) * range), (int)(cy + sinf(a) * range), C_CY);
  }
}
static void drawTowerAt(const Tower& t, int px, int py, uint32_t now, bool ghost) {
  int f = (now >> 8) & 1;
  const Spr& s = SPR[SP_PULSE + t.type * 2 + f];
  uint16_t acc = TCOL[t.type];
  if (ghost) { blit(s, px, py, 3, acc); return; }
  if (t.disabledT > 0 && ((now >> 6) & 1)) { blit(s, px, py, 3, C_RDD); return; }
  cv.fillEllipse(px + 5, py + 9, 5, 1, C_BLACK);  blit(s, px, py);
  if (t.lvl >= 2) { cv.drawPixel(px, py + 5, acc); cv.drawPixel(px, py + 6, acc); cv.drawPixel(px + 9, py + 5, acc); cv.drawPixel(px + 9, py + 6, acc); }
  if (t.lvl >= 3) {
    cv.drawPixel(px, py + 4, acc); cv.drawPixel(px + 9, py + 4, acc); cv.drawPixel(px, py + 7, acc); cv.drawPixel(px + 9, py + 7, acc);
    cv.drawPixel(px + 2, py + 1, C_WH); cv.drawPixel(px + 7, py + 1, C_WH);
    for (int i = 1; i < 9; i++) cv.drawPixel(px + i, py + 9, C_AM);
  }
  float ca = cosf(t.aim), sa = sinf(t.aim);
  int bx = px + 5 + (int)lroundf(ca * 3), by = py + 4 + (int)lroundf(sa * 3);
  cv.drawPixel(bx, by, acc);
  if (t.type != 2) cv.drawPixel(px + 5 + (int)lroundf(ca * 4), py + 4 + (int)lroundf(sa * 4), C_WH);
  if (t.flash > 0) cv.fillRect(bx - 1, by - 1, 3, 3, C_WH);
  if (t.type == 1) {
    int w = (int)(8 * fminf(1.f, t.heat / TD[1].aux[t.lvl - 1]));
    cv.drawFastHLine(px + 1, py - 2, 8, C_DG);
    if (w > 0) cv.drawFastHLine(px + 1, py - 2, w, (t.overheated || w > 6) ? C_RD : C_AM);
    if (t.overheated && ((now >> 6) & 1)) blit(s, px, py, 3, C_RD);
  }
}
static void drawEnemy(const Enemy& e, uint32_t now) {
  const Species& S = SPe(e.type);
  const Spr& s = SPR[SP_E0 + e.type * 2 + (e.anim & 1)];
  int x = (int)e.x - s.w / 2, y = (int)e.y - s.h / 2 + (e.anim & 1);
  if (e.dying > 0) { blit(s, x, y, 2, C_WH); return; }
  bool ghost = (S.ab == AB_PHASE && !e.vis);
  if (ghost) { blit(s, x, y, 3, mixc(TH.glow, 0.5f)); return; }
  cv.fillEllipse((int)e.x, (int)e.y + s.h / 2, s.w / 3 + 1, 1, C_BLACK);
  if (e.flash > 0) blit(s, x, y, 1, C_WH);
  else if (e.freezeT > 0) { blit(s, x, y); blit(s, x, y, 2, C_ICE); }
  else {
    blit(s, x, y);
    if (e.type == 4 && e.bossPhase == 3 && ((now >> 6) & 1)) blit(s, x, y, 3, C_AM);
  }
  if (e.shieldT > 0 && ((now >> 5) & 1)) cv.drawCircle((int)e.x, (int)e.y, s.w / 2 + 2, C_CY);
  if (e.dashT > 0) { cv.drawPixel(x - 2, y + s.h / 2, MAPCI[curMap][0] == CI_AM ? C_AM : C_WH); cv.drawPixel(x - 4, y + s.h / 2, C_WH); }
  if (e.slowT > 0 && e.freezeT <= 0) { cv.drawPixel(x + 1, y + s.h, C_ICE); cv.drawPixel(x + s.w - 2, y + s.h, C_ICE); }
  if (e.hp < e.maxhp && e.type != 4) {
    float r = e.hp / e.maxhp;
    int fw = (int)(s.w * r); if (fw < 1) fw = 1;
    cv.fillRect(x, y - 3, s.w, 2, C_BG);
    cv.fillRect(x, y - 3, fw, 2, r > 0.5f ? C_G : (r > 0.25f ? C_AM : C_RD));
  }
}
static void drawProjectiles() {
  for (auto& p : pr) {
    if (!p.act) continue;
    if (p.kind == 0) {
      for (int k = 2; k >= 0; k--) cv.drawPixel((int)p.trX[k], (int)p.trY[k], SHD[CI_G][k + 1]);
      cv.fillRect((int)p.x - 1, (int)p.y - 1, 2, 2, C_G); cv.drawPixel((int)p.x, (int)p.y, C_WH);
    } else {
      for (int k = 2; k >= 0; k--) cv.fillRect((int)p.trX[k] - 1, (int)p.trY[k] - 1, 2, 2, SHD[CI_AM][k + 1]);
      cv.fillRect((int)p.x - 1, (int)p.y - 1, 3, 3, C_AM); cv.drawPixel((int)p.x, (int)p.y, C_RD);
    }
  }
}
static void drawBeams(uint32_t now) {
  for (int i = 0; i < NPADS; i++) {
    const Tower& t = tw[i];
    if (!t.built || t.type != 1 || t.beamTarget < 0 || t.disabledT > 0) continue;
    float cx, cy; towerCenter(i, cx, cy);
    int x0 = (int)cx, y0 = (int)cy - 1, x1 = (int)t.bex, y1 = (int)t.bey;
    bool horiz = abs(x1 - x0) > abs(y1 - y0);
    int ox = horiz ? 0 : 1, oy = horiz ? 1 : 0;
    cv.drawLine(x0 + ox, y0 + oy, x1 + ox, y1 + oy, C_CYD);
    cv.drawLine(x0 - ox, y0 - oy, x1 - ox, y1 - oy, C_CYD);
    cv.drawLine(x0, y0, x1, y1, ((now >> 5) & 1) ? C_WH : C_CY);
    const Enemy& e = en[t.beamTarget];
    if (e.act && e.dying <= 0) cv.fillCircle((int)e.x, (int)e.y, 2 + ((now >> 4) & 1), C_CY);
  }
}
static void drawFx() {
  for (auto& r : rg) if (r.act) {
    float k = r.life / r.maxl;
    cv.drawCircle((int)r.x, (int)r.y, (int)(r.maxr * (1.0f - k)), SHD[r.col][k > 0.66f ? 0 : (k > 0.33f ? 1 : 2)]);
  }
  for (auto& p : pt) if (p.act) {
    float k = p.life / p.maxl;
    uint16_t c = SHD[p.col][k > 0.75f ? 0 : (k > 0.5f ? 1 : (k > 0.25f ? 2 : 3))];
    if (p.sz == 1) cv.drawPixel((int)p.x, (int)p.y, c); else cv.fillRect((int)p.x, (int)p.y, 2, 2, c);
  }
  for (auto& f : fl) if (f.act) {
    char b[8]; snprintf(b, sizeof(b), "+%d", f.v);
    T(b, (int)f.x - 5, (int)f.y, f.life > 0.35f ? C_AM : C_AMD);
  }
}
static void drawCursor(uint32_t now) {
  int x = PADS[curPad][0] * TILE, y = MAPY + PADS[curPad][1] * TILE, o = (now >> 7) & 1;
  int a = x - 1 - o, b = y - 1 - o, e = x + TILE + o, f = y + TILE + o;
  uint16_t c = C_WH;
  cv.drawFastHLine(a, b, 4, c); cv.drawFastVLine(a, b, 4, c);
  cv.drawFastHLine(e - 3, b, 4, c); cv.drawFastVLine(e, b, 4, c);
  cv.drawFastHLine(a, f, 4, c); cv.drawFastVLine(a, f - 3, 4, c);
  cv.drawFastHLine(e - 3, f, 4, c); cv.drawFastVLine(e, f - 3, 4, c);
}
static void statLine(int type, int l, char* out, size_t n) {
  const TDef& D = TD[type];
  switch (type) {
    case 0: snprintf(out, n, "DMG %d RNG %d RATE %.2f", (int)D.dmg[l], (int)D.range[l], D.rate[l]); break;
    case 1: snprintf(out, n, "DPS %d RNG %d HEAT %.1f COOL %.1f", (int)D.dmg[l], (int)D.range[l], D.aux[l], D.rate[l]); break;
    case 2: snprintf(out, n, "DMG %d SLOW %d%% RNG %d", (int)D.dmg[l], (int)(100 - D.aux[l] * 100), (int)D.range[l]); break;
    default: snprintf(out, n, "DMG %d AOE %d RNG %d", (int)D.dmg[l], (int)D.aux[l], (int)D.range[l]); break;
  }
}
static void drawHUD(uint32_t now) {
  cv.fillRect(0, 0, SW, 11, C_BG); cv.drawFastHLine(0, 11, SW, TH.edge);
  char b[32];
  snprintf(b, sizeof(b), "WAVE %02d/%d", wave, NWAVES);
  T(b, 3, 3, C_G);
  blit(SPR[SP_HEART], 62, 2);
  cv.drawRect(71, 3, 42, 6, C_MG);
  int fw = 40 * coreHp / CORE_MAX;
  float hr = (float)coreHp / CORE_MAX;
  if (fw > 0) cv.fillRect(72, 4, fw, 4, hr > 0.5f ? C_G : (hr > 0.25f ? C_AM : C_RD));
  blit(SPR[SP_COIN], 118, 2);
  snprintf(b, sizeof(b), "%d", coins); T(b, 127, 3, C_AM);
  blit(SPR[SP_SKULL], 158, 2);
  snprintf(b, sizeof(b), "%d", kills); T(b, 167, 3, C_WH);
  if (waveActive) { snprintf(b, sizeof(b), "FOES %d", aliveCount()); Tr(b, 237, 3, C_RD); }
  else if (wave < NWAVES && ((now >> 9) & 1)) Tr("SPACE=GO", 237, 3, C_CY);

  cv.fillRect(0, 112, SW, 23, C_BG); cv.drawFastHLine(0, 112, SW, TH.edge);
  const Tower& t = tw[curPad];
  char l2[48];
  if (t.built) {
    blit(SPR[SP_PULSE + t.type * 2], 2, 114);
    snprintf(b, sizeof(b), "%s TOWER L%d", TD[t.type].name, t.lvl);
    T(b, 15, 114, TCOL[t.type]);
    snprintf(b, sizeof(b), "SELL +%d", t.invested * 7 / 10);
    Tr(b, 237, 114, C_AM);
    statLine(t.type, t.lvl - 1, l2, sizeof(l2));
    T(l2, 15, 121, C_G);
    if (t.lvl < 3) { snprintf(b, sizeof(b), "U:UP $%d", upCost(t.type, t.lvl)); Tr(b, 237, 121, coins >= upCost(t.type, t.lvl) ? C_CY : C_RDD); }
    else Tr("MAX LEVEL", 237, 121, C_AM);
  } else {
    blit(SPR[SP_PULSE + selType * 2], 2, 114);
    snprintf(b, sizeof(b), "BUILD %s TOWER", TD[selType].name);
    T(b, 15, 114, TCOL[selType]);
    snprintf(b, sizeof(b), "COST %d", TD[selType].cost);
    Tr(b, 237, 114, coins >= TD[selType].cost ? C_AM : C_RD);
    statLine(selType, 0, l2, sizeof(l2));
    T(l2, 15, 121, C_G);
    snprintf(b, sizeof(b), "%d/4", selType + 1); Tr(b, 237, 121, C_MG);
  }
  if (((now / 2600) & 1) == 0) T("WASD MOVE  ENTER BUILD  Q/E TYPE  1-4 PICK", 3, 128, C_MG);
  else T("SPACE WAVE  U UPGRADE  X SELL  ESC PAUSE", 3, 128, C_MG);
}
static void drawBossBar() {
  for (auto& e : en) if (e.act && e.type == 4 && e.dying <= 0) {
    int w = 120, x = 60, y = 14;
    cv.fillRect(x - 1, y - 1, w + 2, 6, C_BG); cv.drawRect(x - 1, y - 1, w + 2, 6, TH.edge);
    int fw = (int)(w * e.hp / e.maxhp);
    if (fw > 0) cv.fillRect(x, y, fw, 4, e.bossPhase == 3 ? C_RD : (e.bossPhase == 2 ? C_AM : C_G));
    Tc(SPe(4).name, 120, y + 7, C_RD);
    return;
  }
}
static void drawBanner(uint32_t now) {
  if (msgT <= 0) return;
  int w = Tw(msgBuf) + 10, x = (SW - w) / 2, y = MAPY + MAPH * TILE - 14;
  cv.fillRect(x, y, w, 11, C_BG); cv.drawRect(x, y, w, 11, ((now >> 6) & 1) ? C_G : C_MG);
  Tc(msgBuf, 120, y + 3, C_WH);
}
static void drawWorld(uint32_t now) {
  cv.fillScreen(C_BG);
  drawBackground(now);
  drawPortal(now); drawCore(now);
  float cx, cy; towerCenter(curPad, cx, cy);
  const Tower& sel = tw[curPad];
  if (state == ST_PLAY) drawRange(cx, cy, sel.built ? TD[sel.type].range[sel.lvl - 1] : TD[selType].range[0], now);
  for (int i = 0; i < NPADS; i++) if (tw[i].built) drawTowerAt(tw[i], PADS[i][0] * TILE, MAPY + PADS[i][1] * TILE, now, false);
  if (state == ST_PLAY && !sel.built) { Tower g; memset(&g, 0, sizeof(g)); g.type = (uint8_t)selType; g.lvl = 1; drawTowerAt(g, PADS[curPad][0] * TILE, MAPY + PADS[curPad][1] * TILE, now, true); }
  drawBeams(now);
  for (auto& e : en) if (e.act) drawEnemy(e, now);
  drawProjectiles(); drawFx();
  if (state == ST_PLAY) drawCursor(now);
  drawBossBar(); drawBanner(now);
  drawHUD(now);
  if (coreFlash > 0) { cv.drawRect(0, MAPY, SW, MAPH * TILE, C_RD); cv.drawRect(1, MAPY + 1, SW - 2, MAPH * TILE - 2, C_RDD); }
}
static void panel(int x, int y, int w, int h) { cv.fillRect(x, y, w, h, C_BG); cv.drawRect(x, y, w, h, C_G); cv.drawRect(x + 2, y + 2, w - 4, h - 4, C_DG); }
static void drawPause() {
  panel(60, 22, 120, 84);
  Tc("PAUSED", 120, 29, C_G, 2);
  const char* it[4] = {"RESUME", "RESTART", sndOn ? "SOUND: ON" : "SOUND: OFF", "QUIT TO MAPS"};
  for (int i = 0; i < 4; i++) {
    int y = 52 + i * 12;
    if (i == pauseSel) { cv.fillRect(68, y - 2, 104, 10, C_G); Tc(it[i], 120, y, C_BG); }
    else Tc(it[i], 120, y, C_G);
  }
}
static void drawEnd(bool win, uint32_t now) {
  panel(40, 14, 160, 104);
  Tc(win ? "VICTORY" : "CORE LOST", 120, 21, win ? C_G : C_RD, 2);
  char b[40];
  snprintf(b, sizeof(b), "%s", MAPS[curMap].name); Tc(b, 120, 38, TH.glow);
  snprintf(b, sizeof(b), "WAVES %d/%d", win ? NWAVES : (wave > 0 ? wave - (waveActive ? 1 : 0) : 0), NWAVES); Tc(b, 120, 48, C_WH);
  snprintf(b, sizeof(b), "KILLS %d   CORE %d/%d", kills, coreHp, CORE_MAX); Tc(b, 120, 58, C_WH);
  snprintf(b, sizeof(b), "SCORE %lu", (unsigned long)score); Tc(b, 120, 70, C_AM);
  snprintf(b, sizeof(b), "BEST %lu", (unsigned long)hiMap[curMap]); Tc(b, 120, 80, C_MG);
  if (score >= hiMap[curMap] && score > 0 && ((now >> 8) & 1)) Tc("NEW BEST!", 120, 91, C_CY);
  Tc("ENTER RETRY  ESC MAPS", 120, 107, C_MG);
}
static void drawMapPreview(int m, int ox, int oy) {
  const MapDef& M = MAPS[m];
  cv.fillRect(ox - 1, oy - 1, 122, 52, TH.bg2); cv.drawRect(ox - 1, oy - 1, 122, 52, TH.edge);
  for (int i = 0; i < M.nwp - 1; i++) {
    int ax = M.wp[i][0] * 5, ay = M.wp[i][1] * 5, bx = M.wp[i + 1][0] * 5, by = M.wp[i + 1][1] * 5;
    int x = ax < bx ? ax : bx, y = ay < by ? ay : by;
    cv.fillRect(ox + x, oy + y, abs(ax - bx) + 3, abs(ay - by) + 3, TH.edge);
  }
  for (int p = 0; p < NPADS; p++) cv.fillRect(ox + M.pads[p][0] * 5 + 1, oy + M.pads[p][1] * 5 + 1, 3, 3, TH.dec2);
  cv.fillCircle(ox + 1, oy + M.wp[0][1] * 5 + 1, 3, TH.glow);
  cv.fillRect(ox + 112, oy + M.wp[M.nwp - 1][1] * 5 - 3, 6, 9, C_G);
}
static void drawMaps(uint32_t now) {
  for (int y = 0; y < SH; y++) cv.drawFastHLine(0, y, SW, (y & 1) ? TH.bg2 : TH.bg);
  char b[40];
  snprintf(b, sizeof(b), "SELECT MAP %d/%d", selMap + 1, NMAPS);
  T(b, 4, 3, TH.glow);
  snprintf(b, sizeof(b), "BEST %lu", (unsigned long)hiMap[selMap]); Tr(b, 236, 3, C_AM);
  cv.drawFastHLine(0, 11, SW, TH.edge);
  drawMapPreview(selMap, 10, 18);
  T(MAPS[selMap].name, 140, 18, C_WH);
  T(MAPS[selMap].sub, 140, 27, TH.dec2);
  T("15 WAVES", 140, 38, C_MG);
  if (clearMask & (1 << selMap)) T("CLEARED *", 140, 47, C_G); else T("NOT CLEARED", 140, 47, TH.dec1);
  snprintf(b, sizeof(b), "BOSS: %s", SPC[selMap][4].name); T(b, 140, 56, C_RD);
  int act = (int)((now / 1400) % 5);
  for (int k = 0; k < 5; k++) {
    const Spr& s = SPR[SP_E0 + k * 2 + (((now >> 8) + k) & 1)];
    int x = 18 + k * 44, y = 78 + (16 - s.h) / 2;
    blit(s, x - s.w / 2 + 8, y);
    if (k == act) { cv.drawFastHLine(x - 2, 96, 20, TH.glow); }
  }
  Tc(SPC[selMap][act].name, 120, 100, TH.glow);
  for (int i = 0; i < NMAPS; i++) cv.fillRect(100 + i * 8, 110, 5, 3, i == selMap ? TH.glow : TH.dec1);
  Tc("A/D MAP   ENTER START   ESC BACK", 120, 124, C_MG);
  T("<", 2, 40, TH.glow, 2); T(">", 228, 40, TH.glow, 2);
}
static void drawTitle(uint32_t now) {
  for (int y = 0; y < SH; y++) cv.drawFastHLine(0, y, SW, (y & 1) ? C_BG2 : C_BG);
  for (int x = 0; x < SW; x += 10) for (int y = 0; y < SH; y += 10) cv.drawPixel(x, y, C_TR);
  const char* a = "NEON";
  const char* b = "BASTION";
  int wa = Tw(a, 4), wb = Tw(b, 3);
  T(a, 120 - wa / 2 + 1, 13, C_CYD, 4); T(a, 120 - wa / 2, 12, C_G, 4);
  T(b, 120 - wb / 2 + 1, 41, C_CYD, 3); T(b, 120 - wb / 2, 40, C_CY, 3);
  cv.drawFastHLine(30, 64, 180, C_MG);
  Tc("TOWER DEFENSE   5 MAPS   75 WAVES", 120, 69, C_AM);
  cv.fillRect(0, 100, SW, 10, C_DG); cv.drawFastHLine(0, 100, SW, C_MG); cv.drawFastHLine(0, 109, SW, C_MG);
  for (int i = 0; i < 4; i++) blit(SPR[SP_PULSE + i * 2 + ((now >> 8) & 1)], 36 + i * 44, 84);
  for (int i = 0; i < 7; i++) {
    int x = (int)((now / 22 + i * 40) % 280) - 20;
    const Spr& s = SPR[SP_E0 + (i % 4) * 2 + (((now >> 7) + i) & 1)];
    blit(s, x, 105 - s.h / 2);
  }
  if ((now >> 9) & 1) Tc("PRESS ENTER", 120, 118, C_WH);
  int cleared = 0; for (int i = 0; i < NMAPS; i++) if (clearMask & (1 << i)) cleared++;
  char h[32]; snprintf(h, sizeof(h), "MAPS CLEARED %d/%d", cleared, NMAPS);
  T(h, 3, 3, C_MG);
  Tr(sndOn ? "M: SOUND ON" : "M: SOUND OFF", 237, 3, C_MG);
  Tc("WASD CURSOR  ENTER BUILD  SPACE WAVE", 120, 127, C_DG);
}

// ============================== INPUT MANAGER ======================
struct Inp { bool w, a, s, d, ok, q, e, start, sell, upg, esc, mute; int digit; };
static std::vector<char> prevK;
static bool pEnter = false, pDel = false;
static Inp pollInput() {
  Inp k{};
  M5Cardputer.update();
  std::vector<char> cur;
  bool en2 = false, de = false;
  if (M5Cardputer.Keyboard.isPressed()) {
    auto st = M5Cardputer.Keyboard.keysState();
    cur = st.word; en2 = st.enter; de = st.del;
  }
  for (char c : cur) {
    bool was = false;
    for (char p : prevK) if (p == c) was = true;
    if (was) continue;
    char l = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    switch (l) {
      case 'w': case ';': k.w = true; break;
      case 'a': case ',': k.a = true; break;
      case 's': case '.': k.s = true; break;
      case 'd': case '/': k.d = true; break;
      case 'q': k.q = true; break;
      case 'e': k.e = true; break;
      case ' ': k.start = true; break;
      case 'x': k.sell = true; break;
      case 'u': k.upg = true; break;
      case 'm': k.mute = true; break;
      case '`': k.esc = true; break;
      case '1': case '2': case '3': case '4': k.digit = l - '0'; break;
    }
  }
  if (en2 && !pEnter) k.ok = true;
  if (de && !pDel) k.esc = true;
  prevK = cur; pEnter = en2; pDel = de;
  return k;
}

// ============================== MAIN ===============================
static uint32_t lastUs = 0;
static void toggleSound() { sndOn = !sndOn; prefs.putBool("snd", sndOn); if (!sndOn) M5Cardputer.Speaker.stop(); }

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(0);
  // Cardputer ADV display orientation is fixed at rotation 0.
  cv.setColorDepth(16);
  cv.createSprite(SW, SH);
  prefs.begin("nbastion", false);
  saveLoad();
  M5Cardputer.Speaker.setVolume(150);
  initPalette();
  buildSprites();
  randomSeed(esp_random());
  loadMap(0);
  for (auto& t : tw) t.beamTarget = -1;
  lastUs = micros();
}

void loop() {
  uint32_t nowUs = micros();
  float dt = (nowUs - lastUs) * 1e-6f;
  lastUs = nowUs;
  if (dt > 0.05f) dt = 0.05f;
  uint32_t now = millis();
  Inp k = pollInput();
  stateT += dt;

  switch (state) {
    case ST_TITLE:
      if (k.mute) toggleSound();
      if (k.ok || k.start) { state = ST_MAPS; previewMap(selMap); break; }
      drawTitle(now);
      break;

    case ST_MAPS:
      if (k.a) { selMap = (selMap + NMAPS - 1) % NMAPS; previewMap(selMap); sfx(SFX_MOVE); }
      if (k.d) { selMap = (selMap + 1) % NMAPS; previewMap(selMap); sfx(SFX_MOVE); }
      if (k.esc) { state = ST_TITLE; break; }
      if (k.ok) { newGame(selMap); break; }
      drawMaps(now);
      break;

    case ST_PLAY:
      if (k.esc) { state = ST_PAUSE; pauseSel = 0; M5Cardputer.Speaker.stop(); break; }
      if (k.w) movePad(0, -1);
      if (k.s) movePad(0, 1);
      if (k.a) movePad(-1, 0);
      if (k.d) movePad(1, 0);
      if (k.q) { selType = (selType + 3) % 4; sfx(SFX_MOVE); }
      if (k.e) { selType = (selType + 1) % 4; sfx(SFX_MOVE); }
      if (k.digit) { selType = k.digit - 1; sfx(SFX_MOVE); }
      if (k.ok) actPad();
      if (k.upg) upgradeTower();
      if (k.sell) sellTower();
      if (k.start) startWave();
      updateGame(dt);
      drawWorld(now);
      break;

    case ST_PAUSE:
      if (k.esc) { state = ST_PLAY; break; }
      if (k.w) pauseSel = (pauseSel + 3) % 4;
      if (k.s) pauseSel = (pauseSel + 1) % 4;
      if (k.mute) toggleSound();
      if (k.ok) {
        if (pauseSel == 0) state = ST_PLAY;
        else if (pauseSel == 1) { newGame(curMap); break; }
        else if (pauseSel == 2) toggleSound();
        else { state = ST_MAPS; selMap = curMap; previewMap(selMap); break; }
      }
      drawWorld(now);
      if (state == ST_PAUSE) drawPause();
      break;

    case ST_WIN:
    case ST_LOSE:
      updateFx(dt);
      if (shakeT > 0) shakeT -= dt;
      if (stateT > 0.8f) {
        if (k.ok) { newGame(curMap); break; }
        if (k.esc) { state = ST_MAPS; selMap = curMap; previewMap(selMap); break; }
      }
      drawWorld(now);
      drawEnd(state == ST_WIN, now);
      break;
  }

  int sx = 0, sy = 0;
  if (shakeT > 0 && state == ST_PLAY) { sx = (int)random(-1, 2); sy = (int)random(-1, 2); }
  cv.pushSprite(sx, sy);
}