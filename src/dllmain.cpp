// UMvC3 PageTurn - diagnostic build 0.0.2
// Loads via Ultimate ASI Loader (dinput8.dll). Logs select-screen events only; changes nothing.
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include "MinHook.h"

static FILE* g_log = nullptr;
static void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}

// ---- Game addresses (Steam build, exe dated 2017-04-03, image base 0x140000000, no ASLR) ----
static const uintptr_t ADDR_GAME_NAME      = 0x140B12D10; // "umvc3"
static const uintptr_t ADDR_BGMAIN_SETCUR  = 0x14036E820; // BgMain::setCursorPos(this, player, slot) (slot < 0x38 filter)
static const uintptr_t ADDR_CURSOR_CTOR    = 0x140372900; // uMenuChrSelCursor::ctor(this, player)
static const uintptr_t ADDR_BGMAIN_ICONS   = 0x14036DF90; // BgMain::buildIcons(this)
static const uintptr_t ADDR_UICURSOR_SETDIM= 0x140373280; // uiCursor::setDims(this, cols, rows)
static const uintptr_t ADDR_GRID_LOOKUP    = 0x140361FD0; // lookup(x, y) -> chrId

typedef void   (__fastcall* tSetCur)(void* self, uint32_t player, uint32_t slot);
typedef void*  (__fastcall* tCursorCtor)(void* self, int player);
typedef void   (__fastcall* tBuildIcons)(void* self);
typedef void   (__fastcall* tSetDims)(void* self, int cols, int rows);
typedef int    (__fastcall* tLookup)(int x, int y);

static tSetCur     o_SetCur = nullptr;
static tCursorCtor o_CursorCtor = nullptr;
static tBuildIcons o_BuildIcons = nullptr;
static tSetDims    o_SetDims = nullptr;
static tLookup     o_Lookup = nullptr;

// Reads the rip-relative displacement of the 'lea rax,[grid table]' inside lookup(x,y). Clone Engine
// repoints it at its own bigger table (allocated at 0x1B0000000), so this tells us whether CE is active.
static void LogGridTable(const char* when) {
    const uint8_t* lea = (const uint8_t*)0x140361FE5; // 48 8D 05 disp32
    int32_t disp = *(const int32_t*)(lea + 3);
    const int32_t* tbl = (const int32_t*)(lea + 7 + disp);
    int count = 0; for (int i = 0; i < 0x3000 / 4; i++) { if (tbl[i] != 0) count = i + 1; if (i >= 56 && tbl[i] == 0 && tbl[i+1] == 0 && tbl[i+2] == 0) break; }
    const uint8_t* sd = (const uint8_t*)0x140373280;
    Log("%s: grid table at %p (%s), last non-zero index %d; setDims bytes %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        when, tbl, ((uintptr_t)tbl == 0x140B3E580) ? "vanilla" : "PATCHED (Clone Engine)", count - 1,
        sd[0],sd[1],sd[2],sd[3],sd[4],sd[5],sd[6],sd[7],sd[8],sd[9],sd[10],sd[11]);
    if ((uintptr_t)tbl != 0x140B3E580) { for (int i = 56; i < count && i < 56 + 8; i++) Log("   slot %d -> chrId %d", i, tbl[i]); }
}
static uint32_t g_lastSlot[2] = { 0xFFFFFFFF, 0xFFFFFFFF };

static void __fastcall h_SetCur(void* self, uint32_t player, uint32_t slot) {
    if (player < 2 && g_lastSlot[player] != slot) {
        g_lastSlot[player] = slot;
        int chr = (slot < 0x38) ? o_Lookup(slot & 7, slot >> 3) : -1;
        Log("setCursorPos bgmain=%p player=%u slot=%u (x=%u y=%u) chrId=%d%s",
            self, player, slot, slot & 7, slot >> 3, chr, slot >= 0x38 ? "  <-- beyond 56, game ignores" : "");
    }
    o_SetCur(self, player, slot);
}
static void* __fastcall h_CursorCtor(void* self, int player) {
    void* r = o_CursorCtor(self, player);
    uint8_t* inner = (uint8_t*)self + 0x78;
    if (player == 0) LogGridTable("at cursor ctor");
    Log("cursor ctor self=%p player=%d inner=%p cols=%d rows=%d total=%d",
        self, player, inner, *(int*)(inner + 0x54), *(int*)(inner + 0x58), *(int*)(inner + 0x5c));
    return r;
}
static void __fastcall h_BuildIcons(void* self) {
    Log("buildIcons bgmain=%p", self);
    o_BuildIcons(self);
    Log("buildIcons done");
}
static void __fastcall h_SetDims(void* self, int cols, int rows) {
    Log("uiCursor::setDims cursor=%p cols=%d rows=%d", self, cols, rows);
    o_SetDims(self, cols, rows);
}

template<typename T> static bool Hook(uintptr_t addr, void* detour, T** orig, const char* name) {
    MH_STATUS s = MH_CreateHook((void*)addr, detour, (void**)orig);
    if (s != MH_OK) { Log("hook %s at %llx failed: %d", name, (unsigned long long)addr, s); return false; }
    Log("hook %s at %llx ok", name, (unsigned long long)addr);
    return true;
}

static void Init() {
    char path[MAX_PATH]; GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* p = strrchr(path, (int)92); if (p) *(p + 1) = 0;
    char logPath[MAX_PATH]; snprintf(logPath, sizeof logPath, "%sUMvC3PageTurn.log", path);
    g_log = fopen(logPath, "w");
    Log("UMvC3 PageTurn diagnostic 0.0.2 loaded; exe base=%p", GetModuleHandleA(nullptr));

    if (strcmp((const char*)ADDR_GAME_NAME, "umvc3") != 0) {
        Log("version check FAILED: expected 'umvc3' at %llx", (unsigned long long)ADDR_GAME_NAME);
        MessageBoxA(nullptr, "UMvC3 PageTurn: unsupported game version. Mod disabled.", "UMvC3 PageTurn", MB_ICONWARNING);
        return;
    }
    Log("version check ok");
    o_Lookup = (tLookup)ADDR_GRID_LOOKUP;
    LogGridTable("at init");

    if (MH_Initialize() != MH_OK) { Log("MH_Initialize failed"); return; }
    Hook(ADDR_BGMAIN_SETCUR, (void*)h_SetCur, &o_SetCur, "BgMain::setCursorPos");
    Hook(ADDR_CURSOR_CTOR, (void*)h_CursorCtor, &o_CursorCtor, "uMenuChrSelCursor::ctor");
    Hook(ADDR_BGMAIN_ICONS, (void*)h_BuildIcons, &o_BuildIcons, "BgMain::buildIcons");
    Hook(ADDR_UICURSOR_SETDIM, (void*)h_SetDims, &o_SetDims, "uiCursor::setDims");
    MH_STATUS s = MH_EnableHook(MH_ALL_HOOKS);
    Log("MH_EnableHook -> %d", s);
}

extern "C" __declspec(dllexport) void InitializeASI() { Init(); }

BOOL WINAPI DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH && g_log) { Log("unloading"); fclose(g_log); }
    return TRUE;
}
