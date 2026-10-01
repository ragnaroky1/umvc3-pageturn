// UMvC3 PageTurn 0.1.0 (Phase 2 minimum version)
// Pages the character select grid over Clone Engine's extended rows.
//   Two-player select: each player owns one half (P1 left, P2 right); each half is a 28-slot page.
//   Solo select: full-width 56-slot pages.
// Loads via Ultimate ASI Loader (dinput8.dll). Requires UMvC3 Community Edition / Clone Engine.
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include "MinHook.h"

#define PT_VERSION "0.2.3"

// ---------------------------------------------------------------- logging
static FILE* g_log = nullptr;
static void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}

// ---------------------------------------------------------------- game addresses (Steam exe 2017-04-03, no ASLR)
static const uintptr_t ADDR_GAME_NAME       = 0x140B12D10; // "umvc3"
static const uintptr_t ADDR_GRID_LOOKUP     = 0x140361FD0; // int lookup(int x, int y) -> chrId   (CE repoints its table inside)
static const uintptr_t ADDR_BGMAIN_SETCUR   = 0x14036E820; // BgMain::setCursorPos(this, player, slot<0x38)
static const uintptr_t ADDR_BGMAIN_ICONS    = 0x14036DF90; // BgMain::buildIcons(this)
static const uintptr_t ADDR_BGMAIN_UPDATE   = 0x14036CE80; // BgMain::update(this)
static const uintptr_t ADDR_CURSOR_CTOR     = 0x140372900; // uMenuChrSelCursor::ctor(this, player)
static const uintptr_t ADDR_CURSOR_UPDATE   = 0x140372E50; // uMenuChrSelCursor::update(this)
static const uintptr_t ADDR_CURSOR_TICK     = 0x140373120; // uMenuChrSelCursor::tick(this)  (drives inner uiCursor + mouse)
static const uintptr_t ADDR_UICURSOR_UPDATE = 0x140323920; // uiCursor::update(this)  (generic; we replace it for our cursors)
static const uintptr_t ADDR_UICURSOR_SETEN  = 0x140373270; // uiCursor::setEnabled(this, bool)
static const uintptr_t ADDR_UICURSOR_SETPL  = 0x140255150; // uiCursor::setPlayer(this, int)
static const uintptr_t ADDR_INPUTMGR_GET    = 0x140001AE0; // getInputMgr()
static const uintptr_t ADDR_MOUSE_GET       = 0x140001AD0; // getMouse()
static const uintptr_t ADDR_MOUSE_SLOT      = 0x14025C3C0; // mouseSlot(mouse, 0) -> slot or -1
static const uintptr_t ADDR_INPUT_BLOCKED   = 0x140282600; // inputBlocked(inputMgr, 0)

// inner uiCursor offsets (inner = outer + 0x78)
enum : int { UC_STATE = 0x48, UC_POS = 0x4C, UC_PREV = 0x50, UC_COLS = 0x54, UC_ROWS = 0x58, UC_PLAYER = 0x60,
             UC_MOVEFLAGS = 0x68, UC_REPEAT = 0x74, UC_REPEATLIM = 0x78, UC_ENABLED = 0x7C, UC_DT = 0x28 };
// outer uMenuChrSelCursor offsets
enum : int { OC_INNER = 0x78, OC_BGMAIN = 0x110, OC_PLAYER = 0x124, OC_ENABLED = 0x128 };
// vtable slots of the inner cursor
enum : int { VT_IN_LEFT = 0x88, VT_IN_RIGHT = 0x90, VT_IN_UP = 0x98, VT_IN_DOWN = 0xA0, VT_CONFIRM = 0xA8, VT_CANCEL = 0xB8, VT_BLOCKED = 0xF8 };
// NOTE: from uiCursor::update: vt+0xa0 -> x++, vt+0x98 -> x--, vt+0x90 -> y++, vt+0x88 -> y--.

typedef int   (__fastcall* tLookup)(int x, int y);
typedef void  (__fastcall* tSetCur)(void* self, uint32_t player, uint32_t slot);
typedef void  (__fastcall* tVoidThis)(void* self);
typedef void* (__fastcall* tCursorCtor)(void* self, int player);
typedef void  (__fastcall* tSetInt)(void* self, int v);
typedef void  (__fastcall* tSetBool)(void* self, uint8_t v);
typedef void* (__fastcall* tGetPtr)();
typedef int   (__fastcall* tMouseSlot)(void* mouse, int i);
typedef char  (__fastcall* tInputBlocked)(void* mgr, int i);
typedef char  (__fastcall* tVtBool1)(void* self, int player);
typedef char  (__fastcall* tVtBlocked)(void* self, int x, int y);

static tLookup    o_Lookup = nullptr;
static tSetCur    o_SetCur = nullptr;
static tVoidThis  o_BuildIcons = nullptr;
static tVoidThis  o_BgUpdate = nullptr;
static tCursorCtor o_CursorCtor = nullptr;
static tVoidThis  o_CursorUpdate = nullptr;
static tVoidThis  o_CursorTick = nullptr;
static tVoidThis  o_UiCursorUpdate = nullptr;

// ---------------------------------------------------------------- state
struct PlayerCursor {
    uint8_t* outer = nullptr;   // uMenuChrSelCursor
    uint8_t* inner = nullptr;   // uiCursor at outer+0x78
    int      realPos = -1;      // authoritative slot in CE's 8 x rows grid
    int      shownPage = -1;    // page currently painted on this player's face (-1 = never)
    ULONGLONG lastTick = 0;     // GetTickCount64 of this cursor's last tick (solo modes never tick P2's cursor)
};
static PlayerCursor g_pc[2];
static uint8_t* g_bgMain = nullptr;
static int  g_cols = 8, g_rows = 18;        // read from the inner cursor after ctor
static bool g_translate = false;            // lookup(x,y) receives visible coords -> translate to real

static inline int vtcall_bool(void* self, int slot, int player) { return (*(tVtBool1*)(*(uint8_t**)self + slot))(self, player); }
static inline char vt_blocked(void* self, int x, int y)          { return (*(tVtBlocked*)(*(uint8_t**)self + VT_BLOCKED))(self, x, y); }

static PlayerCursor* FindByInner(void* inner) { for (auto& p : g_pc) if (p.inner == inner) return &p; return nullptr; }
static PlayerCursor* FindByOuter(void* outer) { for (auto& p : g_pc) if (p.outer == outer) return &p; return nullptr; }

// Split mode = both cursors exist and are enabled. Otherwise solo (full width).
static bool SplitMode() {
    ULONGLONG now = GetTickCount64();
    return g_pc[0].inner && g_pc[1].inner && now - g_pc[0].lastTick < 500 && now - g_pc[1].lastTick < 500;
}
static int  g_ceCount = 0;                   // number of Clone Engine slots (real pos 56 .. 56+g_ceCount-1)
static int  BasePages()    { return SplitMode() ? 2 : 1; }
static int  PageW()        { return SplitMode() ? 4 : 8; }
// Cells hidden behind the CAPCOM / MARVEL logos on the top row: x = 1,2 and 5,6. Modded pages never use them.
static bool IsHole(int cx, int cy) { if (cy != 0) return false; int x = SplitMode() ? cx : cx; return (x % 4) == 1 || (x % 4) == 2; }
static int  CellsPerPage() { return PageW() * 7 - (SplitMode() ? 2 : 4); }
static int  PageCount()    { int cpp = CellsPerPage(); return BasePages() + (g_ceCount + cpp - 1) / cpp; }
// index of a cell within a modded page's fill order (row-major, skipping holes), or -1 for a hole
static int  CellIndex(int cx, int cy) { if (IsHole(cx, cy)) return -1; int k = 0; for (int y = 0; y <= cy; y++) for (int x = 0; x < PageW(); x++) { if (y == cy && x == cx) return k; if (!IsHole(x, y)) k++; } return -1; }
static void CellFromIndex(int k, int& cx, int& cy) { int i = 0; for (int y = 0; y < 7; y++) for (int x = 0; x < PageW(); x++) { if (IsHole(x, y)) continue; if (i == k) { cx = x; cy = y; return; } i++; } cx = cy = 0; }
// real pos -> page / cell
static int  PageOf(int pos) { int x = pos & 7, y = pos >> 3; if (pos < 56) return SplitMode() ? x / 4 : 0; return BasePages() + (pos - 56) / CellsPerPage(); }
static void PageCell(int pos, int& cx, int& cy) {
    if (pos < 56) { int x = pos & 7, y = pos >> 3; cx = SplitMode() ? x % 4 : x; cy = y; return; }
    CellFromIndex((pos - 56) % CellsPerPage(), cx, cy);
}
// page / cell -> real pos, or -1 for a hole / beyond the roster
static int  RealFromPage(int page, int cx, int cy) {
    if (page < BasePages()) return SplitMode() ? cy * 8 + page * 4 + cx : cy * 8 + cx;
    int k = CellIndex(cx, cy); if (k < 0) return -1;
    int n = (page - BasePages()) * CellsPerPage() + k;
    return n < g_ceCount ? 56 + n : -1;
}
// visible slot (0..55) on screen for a real pos of player p
static int VisibleFromReal(int p, int pos) { int cx, cy; PageCell(pos, cx, cy); return SplitMode() ? cy * 8 + p * 4 + cx : cy * 8 + cx; }
// real pos for a visible slot (face f = x/4 in split mode); -1 for holes / beyond roster
static int RealFromVisible(int vis) {
    int x = vis & 7, y = vis >> 3;
    if (!SplitMode()) return RealFromPage(g_pc[0].realPos < 0 ? 0 : PageOf(g_pc[0].realPos), x, y);
    int face = x / 4;
    int page = g_pc[face].realPos < 0 ? face : PageOf(g_pc[face].realPos);
    return RealFromPage(page, x % 4, y);
}
// vt+0xf8 (0x140372d90) returns TRUE when the cell holds a selectable character, FALSE when empty/taken.
static bool RealBlocked(void* inner, int pos) { if (pos < 0) return true; int x = pos & 7, y = pos >> 3; return y >= g_rows || vt_blocked(inner, x, y) == 0; }

// ---------------------------------------------------------------- hooks
static int g_lookupLogBudget = 0;
static int __fastcall h_Lookup(int x, int y) {
    if (g_translate && x >= 0 && x < 8 && y >= 0 && y < 7) {
        int r = RealFromVisible(y * 8 + x);
        int id = r < 0 ? 0 : o_Lookup(r & 7, r >> 3);
        if (g_lookupLogBudget > 0) { g_lookupLogBudget--; Log("   lookup vis(%d,%d) -> real(%d,%d) chrId %d", x, y, r & 7, r >> 3, id); }
        return id;
    }
    return o_Lookup(x, y);
}

typedef void* (__fastcall* tFindNode)(void* root, const char* name);   // 0x1402de580
typedef void* (__fastcall* tChildByIdx)(void* node, int idx);          // 0x140326b90
static void ProbeFaceNodes(void* bgMain) {
    void* root = *(void**)((uint8_t*)bgMain + 0x58);
    if (!root) { Log("probe: no model root"); return; }
    void* face = ((tFindNode)0x1402DE580)(root, "chs_meku_face_a");
    Log("probe: root=%p vt=%p face_a=%p vt=%p", root, root ? *(void**)root : nullptr, face, face ? *(void**)face : nullptr);
    if (!face) return;
    for (int i = 0; i < 2; i++) {
        void* n = ((tChildByIdx)0x140326B90)(face, i);
        Log("probe: face_a child %d = %p vt=%p vt[+0x50]=%p", i, n, n ? *(void**)n : nullptr, n ? *(void**)(*(uint8_t**)n + 0x50) : nullptr);
    }
}
// ---------------------------------------------------------------- resident texture cache
// At runtime the resource manager no longer resolves arc-packed textures by name (it goes straight to disk and
// raises "Failed open file"), so we remember the texture objects the game itself loaded for the grid at screen load.
typedef void* (__fastcall* tResLoadFn)(void* mgr, void* dti, const char* path, int flag);
static tResLoadFn o_ResLoad = nullptr;
static bool g_resLoadHooked = false;
struct TexCacheEntry { char path[160]; void* tex; };
static TexCacheEntry g_texCache[128]; static int g_texCacheN = 0;
static void* CacheFind(const char* path) {
    char alt[176]; snprintf(alt, sizeof alt, "%s_typeC", path);
    for (int i = 0; i < g_texCacheN; i++) if (!_stricmp(g_texCache[i].path, path) || !_stricmp(g_texCache[i].path, alt)) return g_texCache[i].tex;
    return nullptr;
}
static void CacheAdd(const char* path, void* tex) {
    if (CacheFind(path) || g_texCacheN >= 128) return;
    strncpy(g_texCache[g_texCacheN].path, path, 159); g_texCache[g_texCacheN].path[159] = 0; g_texCache[g_texCacheN].tex = tex; g_texCacheN++;
}
static void* __fastcall h_ResLoad(void* mgr, void* dti, const char* path, int flag) {
    void* tex = o_ResLoad(mgr, dti, path, flag);
    static int seen = 0; if (g_translate && seen < 70) { seen++; Log("   game loaded [%s] dti=%p flag=%d -> %p", path ? path : "(null)", dti, flag, tex); }
    if (g_translate && tex && path && strstr(path, "chs_face_a")) CacheAdd(path, tex);
    return tex;
}
static void EnsureResLoadHook() {
    if (g_resLoadHooked) return;
    void* mgr = ((void*(__fastcall*)())0x140001B10)();
    if (!mgr) return;
    void* fn = *(void**)(*(uint8_t**)mgr + 0x60);
    MH_STATUS a = MH_CreateHook(fn, (void*)h_ResLoad, (void**)&o_ResLoad);
    MH_STATUS b = MH_EnableHook(fn);
    g_resLoadHooked = (a == MH_OK && b == MH_OK);
    Log("resource load fn %p hook: %d/%d", fn, a, b);
}

static void __fastcall h_BuildIcons(void* self) {
    EnsureResLoadHook();
    Log("buildIcons bgmain=%p (split=%d, P1 page %d, P2 page %d)", self, SplitMode(), g_pc[0].realPos >= 0 ? PageOf(g_pc[0].realPos) + 1 : 0, g_pc[1].realPos >= 0 ? PageOf(g_pc[1].realPos) + 1 : 0);
    g_lookupLogBudget = 8;
    bool prev = g_translate; g_translate = true;
    o_BuildIcons(self);
    g_translate = prev;
    static int probes = 0; if (probes++ < 2) ProbeFaceNodes(self);
}
static void __fastcall h_BgUpdate(void* self) {
    g_bgMain = (uint8_t*)self;
    bool prev = g_translate; g_translate = true;
    o_BgUpdate(self);
    g_translate = prev;
}

static void* __fastcall h_CursorCtor(void* self, int player) {
    void* r = o_CursorCtor(self, player);
    if (player >= 0 && player < 2) {
        g_pc[player].outer = (uint8_t*)self;
        g_pc[player].inner = (uint8_t*)self + OC_INNER;
        g_pc[player].realPos = *(int*)(g_pc[player].inner + UC_POS);
        g_pc[player].shownPage = -1;
        g_texCacheN = 0;
        g_cols = *(int*)(g_pc[player].inner + UC_COLS);
        g_rows = *(int*)(g_pc[player].inner + UC_ROWS);
        g_ceCount = 0;
        for (int sl = 56; sl < g_rows * 8; sl++) if (o_Lookup(sl & 7, sl >> 3) != 0) g_ceCount = sl - 56 + 1;
        Log("cursor ctor player=%d outer=%p inner=%p cols=%d rows=%d startPos=%d", player, self, g_pc[player].inner, g_cols, g_rows, g_pc[player].realPos);
        { typedef void* (__fastcall* tG)(); typedef char (__fastcall* tF)(void*); void* game = ((tG)0x140004700)();
          typedef int (__fastcall* tI)(void*);
          Log("   mode: id44b0=%d [+0x34c]=%d flags 6af0=%d 6ab0=%d 6770=%d 6920=%d 6870=%d 6800=%d", ((tI)0x1400044B0)(game), *(int*)((uint8_t*)game + 0x34c), ((tF)0x140006AF0)(game), ((tF)0x140006AB0)(game), ((tF)0x140006770)(game), ((tF)0x140006920)(game), ((tF)0x140006870)(game), ((tF)0x140006800)(game)); }
    }
    return r;
}

// Our replacement for uiCursor::update, for the two select-screen cursors only.
static void PagedCursorUpdate(PlayerCursor& pc) {
    uint8_t* c = pc.inner;
    if (*(int*)(c + UC_STATE) != -1 || !c[UC_ENABLED]) return;
    int player = *(int*)(c + UC_PLAYER);
    *(int*)(c + UC_MOVEFLAGS) = 0; *(int*)(c + 0x70) = 0;
    if (player < 0) return;
    if (vtcall_bool(c, VT_CONFIRM, player)) { *(int*)(c + UC_STATE) = 0; return; }
    if (vtcall_bool(c, VT_CANCEL, player))  { *(int*)(c + UC_STATE) = -2; return; }

    float rep = *(float*)(c + UC_REPEAT);
    if (rep < *(float*)(c + UC_REPEATLIM)) *(float*)(c + UC_REPEAT) = rep + *(float*)(c + UC_DT);
    int pos = pc.realPos; if (pos < 0) pos = *(int*)(c + UC_POS);
    *(int*)(c + UC_PREV) = pos;

    int dx = 0, dy = 0;
    if (vtcall_bool(c, VT_IN_DOWN, player))       { dx = +1; *(int*)(c + UC_MOVEFLAGS) |= 2; }   // vt+0xa0: x++
    else if (vtcall_bool(c, VT_IN_UP, player))    { dx = -1; *(int*)(c + UC_MOVEFLAGS) |= 1; }   // vt+0x98: x--
    if (vtcall_bool(c, VT_IN_RIGHT, player))      { dy = +1; *(int*)(c + UC_MOVEFLAGS) |= 4; }   // vt+0x90: y++
    else if (vtcall_bool(c, VT_IN_LEFT, player))  { dy = -1; *(int*)(c + UC_MOVEFLAGS) |= 8; }   // vt+0x88: y--
    if (!dx && !dy) return;
    *(float*)(c + UC_REPEAT) = 0;

    int page = PageOf(pos), cx, cy; PageCell(pos, cx, cy);
    int npages = PageCount(), w = PageW();
    // step in page space; skip blocked cells continuing in the same direction (like the game does)
    for (int guard = 0; guard < npages * 56; guard++) {
        cx += dx; cy += dy;
        bool newPage = false;
        if (cx < 0)  { cx = w - 1; page = (page - 1 + npages) % npages; newPage = true; }
        if (cx >= w) { cx = 0;     page = (page + 1) % npages; newPage = true; }
        if (cy < 0)  { cy = 6; }                                   // up/down wrap inside the page
        if (cy >= 7) { cy = 0; }                                   // only left/right flip pages (Will's preference)
        int np = RealFromPage(page, cx, cy);
        if (!RealBlocked(c, np)) { pos = np; break; }
        if (newPage) {
            // just flipped onto a page whose entry cell is empty: take the nearest valid cell on this page
            // (same row first, scanning in the movement direction, then the other rows), else keep flipping
            int best = -1;
            for (int ring = 0; ring < 7 && best < 0; ring++) {
                for (int sgn = -1; sgn <= 1 && best < 0; sgn += 2) {
                    int ry = cy + sgn * ring; if (ry < 0 || ry >= 7) continue;
                    for (int i = 0; i < w; i++) {
                        int rx = dx >= 0 ? (cx + i) % w : ((cx - i) % w + w) % w;
                        int cand = RealFromPage(page, rx, ry);
                        if (!RealBlocked(c, cand)) { best = cand; break; }
                    }
                    if (ring == 0) break;   // ring 0: only one row
                }
            }
            if (best >= 0) { pos = best; break; }
            cx = dx > 0 ? w - 1 : 0;        // nothing on this page: continue flipping in the same direction
        }
    }
    Log("move p%d d=(%d,%d) -> page %d cell (%d,%d) real slot %d (x=%d y=%d)", player, dx, dy, page + 1, cx, cy, pos, pos & 7, pos >> 3);
    pc.realPos = pos;
    *(int*)(c + UC_POS) = pos;
}

static void __fastcall h_UiCursorUpdate(void* self) {
    PlayerCursor* pc = FindByInner(self);
    if (!pc) { o_UiCursorUpdate(self); return; }
    PagedCursorUpdate(*pc);
}

// Replacement for uMenuChrSelCursor::tick: enable/player, movement, mouse; then leave the VISIBLE pos in +0x4c
// for the rest of uMenuChrSelCursor::update (cursor sprite animation + BgMain::setCursorPos).
static void __fastcall h_CursorTick(void* self) {
    PlayerCursor* pc = FindByOuter(self);
    if (!pc) { o_CursorTick(self); return; }
    uint8_t* o = (uint8_t*)self; uint8_t* in = pc->inner;
    pc->lastTick = GetTickCount64();
    { static ULONGLONG lastLog[2] = {0, 0}; int pl = *(int*)(o + OC_PLAYER); ULONGLONG now = GetTickCount64();
      if (pl >= 0 && pl < 2 && now - lastLog[pl] > 1000) { lastLog[pl] = now;
        Log("tick p%d outerEnabled=%d innerEnabled=%d innerPlayer=%d state=%d split=%d", pl, o[OC_ENABLED], in[UC_ENABLED], *(int*)(in + UC_PLAYER), *(int*)(in + UC_STATE), SplitMode()); } }
    if (!((tInputBlocked)ADDR_INPUT_BLOCKED)(((tGetPtr)ADDR_INPUTMGR_GET)(), 0)) {
        int before = pc->realPos < 0 ? *(int*)(in + UC_POS) : pc->realPos;
        *(int*)(in + UC_POS) = before;
        ((tSetBool)ADDR_UICURSOR_SETEN)(in, o[OC_ENABLED]);
        ((tSetInt)ADDR_UICURSOR_SETPL)(in, *(int*)(o + OC_PLAYER));
        (*(tVoidThis*)(*(uint8_t**)in + 0x40))(in);          // -> h_UiCursorUpdate -> PagedCursorUpdate
        pc->realPos = *(int*)(in + UC_POS);
        if (*(int*)(in + UC_STATE) == -1 && o[OC_ENABLED] && *(int*)(o + OC_PLAYER) == 0 && pc->realPos == before) {
            int m = ((tMouseSlot)ADDR_MOUSE_SLOT)(((tGetPtr)ADDR_MOUSE_GET)(), 0);
            if (m >= 0 && m < 0x38) {
                if (!SplitMode() || (m & 7) / 4 == 0) {          // P1's half only in split mode
                    int r = RealFromVisible(m);
                    if (r >= 0 && r != before && !RealBlocked(in, r)) pc->realPos = r;
                }
            }
        }
    }
    if (pc->realPos >= 0) *(int*)(in + UC_POS) = VisibleFromReal(*(int*)(o + OC_PLAYER), pc->realPos);
}

// ---------------------------------------------------------------- our own grid repaint (never asks the engine for a file that does not exist)
typedef void* (__fastcall* tGetResMgr)();                                        // 0x140001b10
typedef void* (__fastcall* tResLoad)(void* mgr, void* dti, const char* path, int flag); // mgr vt+0x60
typedef void* (__fastcall* tTexHandle)(void* tex);                                 // 0x1400b0f60
typedef void  (__fastcall* tResRelease)(void* res);                                // 0x14050d5a0
typedef void  (__fastcall* tNodeSetTex)(void* node, void* handle);                 // node vt+0x50
typedef const char* (__fastcall* tChrName)(int chrId);                             // 0x140058f90 (CE repoints its table)
static const uintptr_t ADDR_RESMGR_GET = 0x140001B10, ADDR_TEX_HANDLE = 0x1400B0F60, ADDR_RES_RELEASE = 0x14050D5A0,
                       ADDR_CHR_NAME = 0x140058F90, ADDR_FIND_NODE = 0x1402DE580, ADDR_CHILD_BY_IDX = 0x140326B90,
                       ADDR_TEX_DTI = 0x140E17570;
static char g_gameDir[MAX_PATH];

static void ToBackslashes(char* p) { for (; *p; p++) if (*p == '/') *p = (char)92; }
static bool LooseFileExists(const char* resPath) {
    char full[MAX_PATH]; snprintf(full, sizeof full, "%snativePCx64/%s.tex", g_gameDir, resPath); ToBackslashes(full);
    return GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES;
}
// Pick a texture resource path for a character id. Vanilla ids come from the arc (always present).
static void IconPathFor(int chrId, char* out, size_t n) {
    if (chrId == 0x35)                { snprintf(out, n, "ui/chs/chs_face_a/chs_cs_f/f_Random_BM_HQ_NOMIP"); return; }
    if (chrId == 0x36)                { snprintf(out, n, "ui/chs/chs_face_a/chs_cs_f/f_Random_all_BM_HQ_NOMIP"); return; }
    if (chrId <= 0 || chrId == 0x34 || chrId == 0x37) { snprintf(out, n, "ui/PageTurn/blank_BM_HQ_NOMIP"); return; }   // our transparent icon
    const char* name = ((tChrName)ADDR_CHR_NAME)(chrId);
    if (!name || !*name)              { snprintf(out, n, "ui/chs/chs_face_a/chs_cs_f/f_Hatena_BM_HQ_NOMIP"); return; }
    if (chrId < 60)                   { snprintf(out, n, "ui/chs/chs_face_a/chs_cs_f/f_%s00_BM_HQ_NOMIP", name); return; }
    // Clone Engine character: only loose files can exist. Try a real icon, then the body portrait, else "?".
    snprintf(out, n, "ui/chs/chs_face_a/chs_cs_f/f_%s00_BM_HQ_NOMIP", name);   if (LooseFileExists(out)) return;
    snprintf(out, n, "ui/chs/chs_b1p/chs_body/b_%s255_BM_HQ_NOMIP", name);      if (LooseFileExists(out)) return;  // CE clone bodies use 255
    snprintf(out, n, "ui/chs/chs_b1p/chs_body/b_%s99_BM_HQ_NOMIP", name);       if (LooseFileExists(out)) return;
    snprintf(out, n, "ui/chs/chs_face_a/chs_cs_f/f_Hatena_BM_HQ_NOMIP");
}
static int g_repaintLog = 60;
static void RepaintIcons(void* bgMain) {
    void* root = *(void**)((uint8_t*)bgMain + 0x58);
    if (!root) return;
    void* mgr = ((tGetResMgr)ADDR_RESMGR_GET)();
    int painted = 0, missing = 0;
    for (int face = 0; face < 2; face++) {
        void* mesh = ((tFindNode)ADDR_FIND_NODE)(root, face ? "chs_meku_face_b" : "chs_meku_face_a");
        if (!mesh) continue;
        for (int i = 0; i < 28; i++) {
            int x = face ? 4 + i / 7 : 3 - i / 7, y = i % 7;
            int real = RealFromVisible(y * 8 + x);
            int chrId = real < 0 ? 0 : o_Lookup(real & 7, real >> 3);
            char path[160]; IconPathFor(chrId, path, sizeof path); ToBackslashes(path);
            if (g_repaintLog > 0) { g_repaintLog--; Log("   node %c%d vis(%d,%d) real %d chrId %d -> %s", face ? 'b' : 'a', i, x, y, real, chrId, path); }
            void* tex = CacheFind(path); bool fromCache = tex != nullptr;
            if (!tex) {
                bool loose = (chrId >= 60) || strstr(path, "PageTurn") != nullptr;
                if (loose && LooseFileExists(path) && o_ResLoad) tex = o_ResLoad(mgr, (void*)ADDR_TEX_DTI, path, 1);
                if (!tex) { char alt[160]; snprintf(alt, sizeof alt, "ui/chs/chs_face_a/chs_cs_f/f_Hatena_BM_HQ_NOMIP"); ToBackslashes(alt); tex = CacheFind(alt); fromCache = tex != nullptr; }
            }
            if (g_repaintLog >= 0 && g_repaintLog < 60) Log("      tex %p %s", tex, fromCache ? "(cached)" : "(loose)");
            if (!tex) { missing++; continue; }
            void* node = ((tChildByIdx)ADDR_CHILD_BY_IDX)(mesh, i);
            if (node) { (*(tNodeSetTex*)(*(uint8_t**)node + 0x50))(node, ((tTexHandle)ADDR_TEX_HANDLE)(tex)); painted++; }
            if (!fromCache) ((tResRelease)ADDR_RES_RELEASE)(tex);
        }
    }
    Log("repaint: %d nodes painted, %d textures unavailable", painted, missing);
}

static void __fastcall h_CursorUpdate(void* self) {
    PlayerCursor* pc = FindByOuter(self);
    if (!pc) { o_CursorUpdate(self); return; }
    o_CursorUpdate(self);                                     // uses visible pos left by h_CursorTick
    if (pc->realPos >= 0) *(int*)(pc->inner + UC_POS) = pc->realPos;   // restore real pos for everyone else
    uint8_t* bg = *(uint8_t**)((uint8_t*)self + OC_BGMAIN);
    if (bg) g_bgMain = bg;
    // repaint when this player's page changed (or mode changed)
    int player = *(int*)((uint8_t*)self + OC_PLAYER);
    int page = pc->realPos >= 0 ? PageOf(pc->realPos) : 0;
    int key = page * 2 + (SplitMode() ? 1 : 0);
    if (key != pc->shownPage) {
        pc->shownPage = key;
        if (g_bgMain) {
            Log("player %d -> page %d/%d (%s); repainting grid", player, page + 1, PageCount(), SplitMode() ? "split" : "solo");
            RepaintIcons(g_bgMain);
        }
    }
}

static void __fastcall h_SetCur(void* self, uint32_t player, uint32_t slot) {
    g_bgMain = (uint8_t*)self;
    o_SetCur(self, player, slot);
}

// ---------------------------------------------------------------- crash diagnostics
static void LogStack(const char* why) {
    void* frames[48]; USHORT n = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
    Log("%s - stack (%u frames):", why, n);
    for (USHORT i = 0; i < n; i++) {
        HMODULE m = nullptr; char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)frames[i], &m) && m) {
            GetModuleFileNameA(m, name, MAX_PATH); const char* b = strrchr(name, (int)92); if (b) memmove(name, b + 1, strlen(b));
        }
        Log("   #%02u %p  %s+%llx", i, frames[i], name, (unsigned long long)((uintptr_t)frames[i] - (uintptr_t)m));
    }
}
typedef void (__cdecl* tInvParamHandler)(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t);
static void __cdecl OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {
    LogStack("ucrtbase invalid parameter (this is the crash)");
}
static LONG WINAPI OnUnhandled(EXCEPTION_POINTERS* ep) {
    Log("unhandled exception %lx at %p", ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    LogStack("unhandled exception");
    return EXCEPTION_CONTINUE_SEARCH;
}
static void InstallCrashDiag() {
    HMODULE u = GetModuleHandleA("ucrtbase.dll"); if (!u) u = LoadLibraryA("ucrtbase.dll");
    typedef tInvParamHandler (__cdecl* tSet)(tInvParamHandler);
    tSet set = u ? (tSet)GetProcAddress(u, "_set_invalid_parameter_handler") : nullptr;
    if (set) { set(OnInvalidParameter); Log("ucrtbase invalid-parameter handler installed"); }
    AddVectoredExceptionHandler(0, [](EXCEPTION_POINTERS* ep) -> LONG {
        DWORD c = ep->ExceptionRecord->ExceptionCode;
        if (c == 0xC0000409 || c == 0xC0000005 || c == 0xC000001D) { OnUnhandled(ep); }
        return EXCEPTION_CONTINUE_SEARCH; });
}

typedef int (WINAPI* tMsgBoxA)(HWND, LPCSTR, LPCSTR, UINT);
typedef int (WINAPI* tMsgBoxW)(HWND, LPCWSTR, LPCWSTR, UINT);
static tMsgBoxA o_MsgBoxA = nullptr; static tMsgBoxW o_MsgBoxW = nullptr;
static int WINAPI h_MsgBoxA(HWND h, LPCSTR t, LPCSTR c, UINT u) { Log("MessageBoxA [%s]: %s", c ? c : "", t ? t : ""); LogStack("MessageBoxA caller"); return o_MsgBoxA(h, t, c, u); }
static int WINAPI h_MsgBoxW(HWND h, LPCWSTR t, LPCWSTR c, UINT u) { Log("MessageBoxW [%ls]: %ls", c ? c : L"", t ? t : L""); LogStack("MessageBoxW caller"); return o_MsgBoxW(h, t, c, u); }

// ---------------------------------------------------------------- init
template<typename T> static bool Hook(uintptr_t addr, void* detour, T** orig, const char* name) {
    MH_STATUS s = MH_CreateHook((void*)addr, detour, (void**)orig);
    if (s != MH_OK) { Log("hook %s at %llx failed: %d", name, (unsigned long long)addr, s); return false; }
    return true;
}

static void Init() {
    char path[MAX_PATH]; GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* p = strrchr(path, (int)92); if (p) *(p + 1) = 0;
    strncpy(g_gameDir, path, sizeof g_gameDir - 1);
    char logPath[MAX_PATH]; snprintf(logPath, sizeof logPath, "%sUMvC3PageTurn.log", path);
    g_log = fopen(logPath, "w");
    Log("UMvC3 PageTurn %s loaded", PT_VERSION);
    if (strcmp((const char*)ADDR_GAME_NAME, "umvc3") != 0) {
        Log("version check FAILED");
        MessageBoxA(nullptr, "UMvC3 PageTurn: unsupported game version. Mod disabled.", "UMvC3 PageTurn", MB_ICONWARNING);
        return;
    }
    InstallCrashDiag();
    if (MH_Initialize() != MH_OK) { Log("MH_Initialize failed"); return; }
    bool ok = true;
    ok &= Hook(ADDR_GRID_LOOKUP,     (void*)h_Lookup,         &o_Lookup,         "lookup");
    ok &= Hook(ADDR_BGMAIN_SETCUR,   (void*)h_SetCur,         &o_SetCur,         "BgMain::setCursorPos");
    ok &= Hook(ADDR_BGMAIN_ICONS,    (void*)h_BuildIcons,     &o_BuildIcons,     "BgMain::buildIcons");
    ok &= Hook(ADDR_BGMAIN_UPDATE,   (void*)h_BgUpdate,       &o_BgUpdate,       "BgMain::update");
    ok &= Hook(ADDR_CURSOR_CTOR,     (void*)h_CursorCtor,     &o_CursorCtor,     "uMenuChrSelCursor::ctor");
    ok &= Hook(ADDR_CURSOR_UPDATE,   (void*)h_CursorUpdate,   &o_CursorUpdate,   "uMenuChrSelCursor::update");
    ok &= Hook(ADDR_CURSOR_TICK,     (void*)h_CursorTick,     &o_CursorTick,     "uMenuChrSelCursor::tick");
    ok &= Hook(ADDR_UICURSOR_UPDATE, (void*)h_UiCursorUpdate, &o_UiCursorUpdate, "uiCursor::update");
    HMODULE u32 = GetModuleHandleA("user32.dll");
    if (u32) { MH_CreateHook((void*)GetProcAddress(u32, "MessageBoxA"), (void*)h_MsgBoxA, (void**)&o_MsgBoxA);
               MH_CreateHook((void*)GetProcAddress(u32, "MessageBoxW"), (void*)h_MsgBoxW, (void**)&o_MsgBoxW); }
    MH_STATUS s = MH_EnableHook(MH_ALL_HOOKS);
    Log("hooks %s, MH_EnableHook -> %d", ok ? "created" : "PARTIAL", s);
}

extern "C" __declspec(dllexport) void InitializeASI() { Init(); }

BOOL WINAPI DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH && g_log) { Log("unloading"); fclose(g_log); }
    return TRUE;
}
