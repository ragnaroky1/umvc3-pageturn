# UMvC3 Character Select Page-Turn Mod — Research Notes

Running log of every finding. Newest entries appended at the bottom of each section.

## Environment (2026-09-30)

- Game: Steam app 357190, install `C:\Program Files (x86)\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3`
- Steam buildid `11420782`, depot 357191 manifest `7549865494669531118`, fresh install today.
- `umvc3.exe` sha256 `b2f2fabe01949f1130f1cc42d31709a2a8cbed9c524330b5fdee71583bd21e7e`, 14,828,952 bytes,
  PE timestamp 2017-04-03 (matches the April 2017 patch that added menu mouse support).
  Capcom pushed a June 2026 "Win11 compat" update that broke things and reverted it; current exe is still the 2017 build.
- PE: x64, ImageBase 0x140000000, **no ASLR** (DllCharacteristics 0x8120), so static addresses like 0x1400xxxxx are usable directly.
  Sections: .text 0x1000 (0xa59200 raw), .rdata 0xa5b000, .data 0xc51000, .pdata 0xe25000, .rsrc 0xebb000, .bind 0xed3000.
- Imports: d3d9, d3dx9_43, DINPUT8, XINPUT1_3, steam_api64, winmm, ws2_32, etc. `DINPUT8.dll` is imported directly → Ultimate ASI Loader as `dinput8.dll` works (this is how Clone Engine and UMVC3Hook load).
- Clean install backup: `D:\umvc3_backup\ULTIMATE MARVEL VS. CAPCOM 3_clean_2026-09-30` (10,360 files, 3.523 GB). Restore: `scripts/restore_clean_game.ps1`.
- Toolchain on this PC: VS 2022 Community + VS 18 BuildTools (MSVC), Rust, Python 3.11. No Java before today.
- Tools installed (portable, no admin) in `C:\Tools`:
  - `ghidra_12.1.4_PUBLIC` (official GitHub release zip), `jdk-21.0.12.1+1` (Adoptium Temurin)
  - `x64dbg` (snapshot 2026-05-27, official GitHub release)
  - `asiloader\dinput8.dll` Ultimate ASI Loader v9.7.4 x64, sha256 `fa266e3513d02c08a1b808f28c10538a489eaffaa4b0707f7cc1066e71b5afd7`
  - Cheat Engine: NOT installed. Official installer (cheatengine.org) bundles offers; Will should install and decline bundles.
- winget: JDK/x64dbg installs hung waiting for UAC; killed. Portable zips used instead.

## Reference projects (cloned into `thirdparty/`, gitignored)

- `ermaccer/UMVC3Hook` — open-source ASI plugin for this exact exe (camera mod). Uses Ultimate ASI Loader, MinHook, ImGui on a D3D9 EndScene hook.
  - Exports `InitializeASI()` — the ASI loader calls this. DllMain does nothing.
  - Version check: string "umvc3" at `0x140B12D10`.
  - Hooks `0x14001A490` (camera ctor), patches `0x1406A9864+2` = 0x14 (DirectInput coop level → non-exclusive so ImGui gets input).
  - Calls into engine at 0x140015710, 0x14000BAA0, 0x14045F7D0, 0x14000BA60 (matrix/camera helpers).
- `ArcherOfLegend/MarvelCloneTool` (MIT) — Python; clones a base character into a Clone Engine slot. Contains a working **ARC reader/writer** (`mvcclone/arc.py`), used by our `scripts/` to unpack UI archives.
  - Documents Clone Engine's config: `Characters.ini` (root or `nativePCx64/`), blocks `[CharacterN]` with `CharacterID=`, `BaseCharacter=`, `SoundID=`, `NumColors=`, `Child1=`.
  - Base roster IDs 0001–0051 (Ryu..Galactus), children 0052–0059 (ZeroSh, MorriganSh, FeliciaF, FeliciaC, Zombie, Mayoi, RedArremerSh, DrStrangeSh). `CLONE_SLOT_BASE = 59`: clone N gets internal slot 59+N. BGM stream base 109, event base 138.
  - Clone UI textures: `nativePCx64/ui/chs/chs_b1p/chs_as_n/n_{name}_BM_HQ_NOMIP_typeB_other` and `ui/chs/chs_b1p/chs_body/b_{name}99_BM_HQ_NOMIP` (loose files, not in arc).
- `EternalYoshi/umvc3-tools` — TEX↔DDS/PNG converter (`mttexconv`), MRL converter, 010 binary templates for MT Framework formats.
- Ultimate ASI Loader: ASI files go in game root or `scripts/`, `plugins/`, `update/`.

## Community Edition / Clone Engine (from public sources; not yet installed here)

- Nexus mod 286 "UMvC3 Community Edition" by HAVENGERS + tabs twerk team, v1.2.2 (2026-08-24), 67 added fighters. Nexus blocks automated download (403) → Will must download manually.
- Clone Engine by Gneiss (Gneiss64): ships `dinput8.dll` + `CloneEngine.asi`. Adds 50+ slots; modded characters appear in rows *below* the visible grid, reached by scrolling down past the last row.
- CE is "not compatible with other modpacks"; modded chars don't work in Simple Mode.

## MT Framework file formats seen so far

- `.arc` v7, 64-byte path field, entries: path[64], ext_hash u32, csize u32 (low 29 bits), dsize u32, offset u32; zlib payloads. Ext hash = ~crc32(class name) (JAMCRC).
- Class hashes seen: sdl=0x4c0db839 (rScheduler), mrl=0x2749c8a8, mod=0x58a15856, lmt=0x76820d81, efl=0x6d5ae854, epv=0x4e397417, tex=rTexture; unknown: 0x7d37e18c (chs_assist/0000), 0x5b55f5b1 (msg text), 0x1db58b09 (font), 0x5a7e5d8a (sprite anm), 0x7359de6b (table), 0x501acd5c (msg).
- `SDL\0` = rScheduler (timeline/animation script). `MOD\0` v0xd3 = model. `TEX\0` = texture (header 0x10 bytes then packed fields).

## Character select screen assets (`nativePCx64/ui/mnchscmn.arc`, `mnchs.arc`)

- The grid ("meku") is NOT a 2D layout file. It is a 3D model `ui/chs/chs_meku/chs_meku.mod` (+`.mrl`, `.lmt` anims) driven by scheduler `chs_meku.sdl`.
- Per-slot drawing helpers: `chs_meku_face_a.mod` / `chs_meku_face_b.mod` (icon faces), `chs_chr_uv_offset.sdl` (per-character UV offsets into icon sheets), `chs_chr_color_type.sdl`, `chs_card1p.sdl`/`chs_card2p.sdl` (selection cards), `chs_meku_sel*/seld*` (cursor highlights), `meku_chs01..03_BM_NOMIP.tex` (grid backgrounds).
- Per-character icon textures: `b_<Name>99_BM_HQ_NOMIP.tex` (51 of them, one per base char, + `b_Random`), and `n_<Name>_BM_HQ_NOMIP_typeB` name plates. So the grid icon for a slot is a **per-character texture**, which is what Clone Engine supplies loose per clone.
- Implication for page turning: the exe must have a table mapping grid (row,col) → character id → icon texture. Paging = remapping that table per player page and forcing a redraw. Finding that table/function is Phase 1 target #1.

## Open questions

- How does the exe lay out the grid rows/cols (expected 3 rows × 18? Actually UMvC3 shows 50 chars in 3 rows; confirm from `chs_meku.mod` face count and the grid code).
- Where is the select-screen state struct (per-player cursor row/col, page, selected char IDs)?
- How does Clone Engine extend the grid: does it patch the row count, or a slot table, or hook the draw?

## SteamStub DRM (2026-09-30)

- `umvc3.exe` on disk is wrapped by Steam's SteamStub DRM: entry point RVA 0xed3310 lies in the `.bind` section, `.text` has entropy 8.00 (encrypted). Scanning the on-disk exe for code references finds nothing; Ghidra on the raw exe is useless.
- Consequences: (a) for static analysis we need an unpacked copy — made with Steamless (atom0s, GitHub v3.1.0.5) into `C:\Tools\ghidra_projects\` (never committed, never shipped); (b) at runtime the stub decrypts `.text` in place before jumping to the real entry, so hooks at static 0x140xxxxxx addresses work once the game is running. Ultimate ASI Loader already handles this (UMVC3Hook and Clone Engine rely on it).
- The string tables in `.rdata` are NOT encrypted, which is why the class/field names below were readable.

## Select-screen classes found in `.rdata` (MT Framework DTI names)

| VA | name |
|---|---|
| 0x140b3e758 | uMenuChrSel |
| 0x140b3ebd0 | uMenuChrSelAssist |
| 0x140b3f0c0 | uMenuChrSelBgMain — the grid. Fields: mTimeInfinite, mTimeStop, mPhaseStageSel, mVsTexPhase, mSelStgId, **mHideTbl**, mReserveSel, **mCursorPos**, **mCursorPosOld**, mCursorAnimeTime, mpColorTypeSdl, mpUvOffsetSdl, mpVsPrevTex, mpVsNextTex, mpChrNameTex, mpVjobCngColor |
| 0x140b3f550 | uMenuChrSelCardPlayer — per-player card. Fields: mOldChrSelPhase, mHide, mReserveUnitInit, mReserveUnitCansel, mIsLoadChr, mReqChrType, mReqChrBody, mReqChrVoice, mpBgMain, mpCursorCopy, mpTexChr, mpTexChrName, mpTexReserve[0..2] |
| 0x140b3f8f0 | uMenuChrSelCtrlMode (Normal/Simple mode picker) |
| 0x140b3fc18 | uMenuChrSelCursor |
| 0x140b3fd70 | uMenuChrSelHandicap |
| 0x140b3fe88 | uMenuChrSelMakeTex |
| 0x140b3ffd0 | uMenuChrSelNetInfoPlayer |
| 0x140b40250 | uMenuChrSelOption |
| 0x140b40800 | uMenuChrSelReserveUnit |
| 0x140b0a8d0.. | cNetChrSel (netplay select sync) |

- Format strings used by the grid: `chs_meku_face_%c` (0x140b3f250; face mesh a/b), `ui\chs\chs_face_a\chs_cs_f\f_%s%02d_BM_HQ_NOMIP` (0x140b02f40; grid icon per character name + costume index), `ui\chs\chs_meku\chs_card%dp`, `chs_card%dp_no%d[_tf|_tw]`, `Name_name%dp_no%d`, `ColorSelect%dp`.
- Grid icon textures live in `mnchs.arc` → `ui/chs/chs_face_a/chs_cs_f/f_<Name>00_BM_HQ_NOMIP.tex` (16,408 bytes each; 53 files incl. f_Random, f_Random_all, f_Hatena = "?" placeholder). Big side portraits are `chs_b1p/chs_body/b_<Name>99…` (262 KB), name plates `chs_b1p/chs_as_n/n_<Name>…typeB` (32 KB).
- `f_Hatena_BM_HQ_NOMIP_typeC.tex` is a ready-made "?" icon → candidate placeholder for missing modded portraits (Phase 3.4).

## Static analysis of unpacked exe (2026-09-30, capstone via `scripts/disasm.py`)

All addresses are for the Steam 2017-04 exe, image base 0x140000000, no ASLR.

### uMenuChrSelBgMain field offsets (from its DTI property-registration fn `0x14036cad0`)

| offset | field | type/notes |
|---|---|---|
| +0xb0 | mbHost | bool |
| +0xb1 | mTimeInfinite | bool |
| +0xb2 | mTimeStop | bool |
| +0xb4 | mPhaseStageSel | u32 |
| +0xb8 | mHideTbl | u8[2] (per player, hide grid?) |
| +0xba | mReserveSel | u8[2] |
| +0xbc | **mCursorPos** | s32[2] — **one int per player**, a slot index (not row/col) |
| +0xc4 | mCursorPosOld | s32[2] |
| +0xcc | mTimeLimit | float |
| +0xd0 | mCursorAnimeTime | float[2] |
| +0xd8 | mVsTexPhase | u32 |
| +0xdc | mSelStgId | u32 |
| +0xe8 | mpColorTypeSdl | ptr |
| +0xf0 | mpUvOffsetSdl | ptr |
| +0xf8 | mpChrNameTex | ptr[6] |
| +0x128 | mpVsPrevTex | ptr |
| +0x130 | mpVsNextTex | ptr |
| +0x138 | mpVjobCngColor | ptr[2] |
| +0x58 | (base class) pointer to the loaded model/scene root, used with `0x1402de580(root, "name")` to find nodes |

Property-registration helpers: `0x140010a20` = bool/u8 prop, `0x14000bb50` = s32 prop, `0x14000bb10` = u32 prop, `0x14000bb90` = float prop, `0x14000bbd0` = pointer prop; `0x14000bf60` appends to the property list. Args: rcx=out, rdx=obj, r8=name, r9=&field, [rsp+0x20]=array flag (0x20), [rsp+0x28]=count.

### Grid layout

- **Grid lookup `0x140361fd0(int row, int col) -> chrId`**: `chrId = dword [0x140b3e580 + (col*8 + row)*4]` — a **[7 cols][8 rows] int32 table at `0x140b3e580`** (224 bytes, .rdata). Special IDs: 0x34/0x37 → "?" (f_Hatena), 0x35 → Random (f_Random), 0x36 → Random-all. Non-special IDs are validated with `0x140226670(game, chrId)` (available?) and `0x140059520(chrId)`; if a byte at `[singleton+0x5b6c]` (from `0x140001ac0()`) is set, positions (row 0 or 5, col 0 or 5..6) are forced to 0 — probably DLC/lock handling.
- **Grid builder `0x14036dfbe`** (inside a larger BgMain init): for face `a` and `b` (`chs_meku_face_%c`), for slot 0..27: row = slot/7, col = slot%7; face a uses lookup(3 - row, col), face b uses lookup(4 + row, col). So face a = rows 3,2,1,0 and face b = rows 4..7 of the table; 28 icon nodes per face mesh (children via `0x140326b90(node, idx)`), and the icon texture `f_<Name>00` is assigned via vtable slot +0x50 on the node's material. Then 12 highlight meshes (`chs_meku_sel1_a`…`selr2_b`, string table at `0x140d04300`) × 28 nodes each.
- Character name table: `0x140058f90(chrId)` returns `char*` from a pointer array at `0x140c553b0` (.data).
- `0x1403716a0` = per-player card init (uMenuChrSelCardPlayer; vtable near `0x140b3f488`): builds body/name texture handles for chrIds 0..0x33 (52), loads `b_Random`, `n_Hatena`, `chs_card%dp`, `chs_chr_color_type`, and `f_%s%02d` per costume (3×3 loop) into `[this + (0xa2+…)*8]`.

Implication: the visible grid is fully described by a static 56-entry table plus per-player `mCursorPos`. Paging = presenting a different 56-entry table per page and re-running the icon assignment for the 2×28 face nodes. The cursor-movement code that reads/writes `mCursorPos` (+0xbc) is the next target.

### uMenuChrSelCursor (per-player grid cursor)

- Vtable region `0x140b3fa60..0x140b3fc10` (class name strings at 0x140b3fc18/0x140b3fc30). Derives from a generic grid-cursor class whose methods sit at `0x140323600..0x1403238e0`.
- Fields (inner grid cursor): **+0x4c = cursor slot index**, laid out **column-major: row = pos % rows, col = pos / rows** — identical to the grid table index `col*8+row`. **+0x54 = rows (8), +0x58 = cols (7), +0x5c = rows*cols (56)** (set by `0x140373280`). +0x48 = -1 idle state, +0x60 = player index (set by `0x140255150`), +0x7c = enabled, +0x64 = flags (bit 2/4 lock input), +0x74/+0x78 = repeat timers, +0x84 = player index, +0x88/+0x8c = confirm state, +0x110/+0x11c/+0x124 = anim state.
- `0x140372c50` (vtable override): "is the slot under the cursor selectable" → `chrId = 0x140361fd0(pos / cols, pos % cols)` then `0x14024d3f0(game, playerIdx, chrId, -1, -1)`.
- `0x140372d90` (override): `(this, row, col) -> bool` slot blocked (chrId==0 or not selectable).
- Base input handlers: `0x140323890` tests mask 0x10010 → calls vtable+0xc0; `0x140323700` mask 0x40040 → vtable+0xc8; `0x140323750` mask 0x80080 → vtable+0xd0; `0x140323840` mask 0x20020 → vtable+0xd8. The mask comes from vtable+0x100 (get input bits; low bits = press, bit<<16 = repeat). So **+0xc0/+0xc8/+0xd0/+0xd8 are the four directional move handlers** (mapping to up/down/left/right TBD). vtable+0xb0 = confirm-allowed check, +0xe0 = on-confirm, +0xe8 = on-confirm-blocked, +0xf0 = on-cancel.
- Pad/input helpers: `0x140001af0()` = input manager, `0x14025ac40(mgr, playerIdx)` = pad for player, `0x1402b3b80(pad)` = pressed bits, `0x1402b3950(pad)` = held bits, `0x140001b00()`/`0x140001a50`/`0x140001a70` = keyboard/UI-input helpers.

### uMenuChrSelCursor object layout (corrected, from ctor `0x140372900(this, playerIdx)`)

- Outer object vtable `0x140b3fb90`; base ctor `0x14050d5f0`. Outer fields: +0x110 ptr, +0x11c float anim, **+0x120 / +0x124 = player index**, +0x128 = 1, +0x12c = 0.
- **Embedded grid cursor at +0x78**, vtable **`0x140b3fa80`**, ctor `0x1403728c0` → base ctor `0x1403233a0` (generic menu cursor), zeroes +0x80,+0x88,+0x8c,+0x90.
- Right after construction: **`0x140373280(inner, rows=8, cols=7)`** sets the grid size, then `0x140255150(inner, playerIdx)`, then `0x140028060(inner, playerIdx ? 0x1d : 0x1a)`.
- Second ctor `0x1403729b0` (no player; also 8×7).
- Inner vtable (`0x140b3fa80` base): +0x00 dtor `0x140372b60`, +0x20 `0x140372e20` (DTI), +0x40 `0x140323920`, +0x60 `0x1407e4d40` (returns 0), +0x78 `0x1403740a0`, +0x88 `0x140323890`, +0x90 `0x140323700`, +0x98 `0x140323750`, +0xa0 `0x140323840`, +0xa8 `0x140372cd0` (confirm), **+0xb0 `0x140372c50` (isSelectable(slot))**, +0xb8 `0x140372bc0` (cancel), +0xc0..+0xf0 = `0x1407e1a10` (stub), +0xf8 `0x140372d90` (isBlocked(row,col)), +0x100 `0x140323600` (input bits), +0x108 = 0.
- All earlier "field offsets" for the cursor (+0x4c pos, +0x54 cols, +0x84 player) are relative to the **inner** object at outer+0x78.

## Decompiled select-screen logic (Ghidra 12.1.4 on the unpacked exe; dumps in `notes/ghidra_dump*.txt`)

### Coordinate system (confirmed)
- Grid is **8 columns (x) × 7 rows (y) = 56 slots**. `slot = x + 8*y` (x = slot & 7, y = slot >> 3).
- Left half x=0..3 is face mesh `chs_meku_face_a`, right half x=4..7 is `chs_meku_face_b` (mirrored). Node index within a face: `FUN_14036d670(slot)` = `(3 - x') * 7 + y` with `x' = x>3 ? 7-x : x`; `FUN_14036d6e0(slot)` = `x > 3` (which face).
- Layout table `0x140b3e580` is `int32[56]` indexed by `slot` directly (my earlier "[col][row]" reading == x + 8*y). `FUN_140361fd0(x, y)` = table lookup + availability; `FUN_140361fa0(slot)` = same by slot; `FUN_140361f50(chrId)` = reverse lookup (chrId → slot, 0 if absent).
- Table contents (x→ left to right, y↓): y0: Jill,·,·,Random,RandomAll,·,·,Shuma; y1: Nemesis,RedArremer,Hiryu,Naruhodo,Nova,GhostRider,HawkEye,DrStrange; … y6: Leilei,Haggar,CViper,Amaterasu,Phoenix,Magneto,SheHulk,TaskMaster. (`·` = 0 = empty.)

### Objects and key functions
- **uMenuChrSel** (owner; property fn `0x1403615e0`): +0x78 mChrSelArray, **+0xf0 mpChrSelBgMain**, +0xf8 mpChrSelOpt, **+0x120 mpCursor[2]**, +0x140 mpCardPlayer[2], +0x150 mpAssist[2], +0x160 mpReserveUnit[2], +0x178 chrSelData[6] {mType,mBody,mAssist} (3 per player), **+0x1c0 mChrTbl[60] u32, +0x2b0 mChrTblMax**, +0x2b4 mMode[2], +0x2c4 mRandAll[2], +0x2c8 mPhase, +0x2f4 mReserveId[2], +0x32b mbHost.
- **uMenuChrSelCursor** outer (`0x140372900` ctor): +0x78 inner uiCursor, **+0x110 = pointer to BgMain**, +0x11c anim time, +0x120/+0x124 player, +0x128 enabled.
  - `FUN_140372e50` = update: ticks inner via `FUN_140373120`, then `pos = FUN_1402e79d0(inner)` (= inner+0x4c), sets the cursor sprite animation frame to `pos * 60 + t` via `FUN_1401aabe0`, then **`FUN_14036e820(bgMain, player, pos)`**.
  - `FUN_140373120`: inner enable/player, calls inner vt+0x40 (uiCursor::update), then mouse: `slot = FUN_14025c3c0(mouse, 0)`; if `slot < 0x38` and not blocked (`vt+0xf8(inner, slot & 7, slot >> 3)`) → `FUN_140028060(inner, slot)` (set pos).
- **uiCursor** (generic, name string "uiCursor" at 0x140b212b8; vtable `0x140b211b0`): +0x48 state (-1 idle), **+0x4c pos**, +0x50 prevPos, **+0x54 = 8 (columns), +0x58 = 7 (rows), +0x5c = 56**, +0x60 player, +0x68 moveFlags, +0x74 repeat timer, +0x78 repeat limit 10.0, +0x7c enabled.
  - `FUN_140323920` = update: confirm (vt+0xa8) → callback(this,0); cancel (vt+0xb8) → callback(this,-2); x = pos % 8, y = pos / 8; vt+0xa0 (bit 0x20) → x++, vt+0x98 (bit 0x80) → x--, vt+0x90 (bit 0x40) → y++, vt+0x88 (bit 0x10) → y--; **wraps x in [0,8), y in [0,rows)**; pos = y*8 + x; then `FUN_140323b20` skips blocked slots (vt+0xf8) by continuing in the move direction.
  - `FUN_140373280(this, cols, rows)` sets +0x54/+0x58/+0x5c. `FUN_140028060(this, pos)` sets pos. `FUN_1402e79d0(this)` gets pos. `FUN_140255150(this, player)`. `FUN_140373270(this, enabled)`.
- **uMenuChrSelBgMain** (`0x14036e9c0` = load: loads `chs_meku`, color/uv SDLs, 2 highlight units at +0x138; initial mCursorPos = 0x1a (P1) / 0x1d (P2), or from saved teams via `FUN_140361f50`).
  - **`FUN_14036e820(this, player, slot)`: `if (player < 2 && slot < 0x38) mCursorPos[player] = slot;`** — slots ≥ 56 are silently ignored (this is why Clone Engine rows show no highlight).
  - `FUN_14036ce80` = update: per player derives highlight node from mCursorPos (+0xbc/+0xc0) and old (+0xc4/+0xc8), colors the 12 highlight meshes via `FUN_14036e840(this, meshIdx, uv, color)`, plays `sel_round_*` anims via `FUN_14036c930(this, player, state)`, and iterates all 56 slots marking taken/unavailable characters (`FUN_14024d3f0(game, player, chrId, -1, -1)` = "already on a team").
  - **`FUN_14036df90(this)` = build icons** (1589 bytes; vtable 0x140b3f058): for face a/b × 28 nodes calls `FUN_140361fd0(...)` and assigns `f_<Name>00` / `f_Hatena` / `f_Random` / `f_Random_all` textures to node materials (`node->vt+0x50(node, tex+8)`), then the 12 highlight meshes. Self-contained → **callable again from a mod to repaint the grid.**
- Netplay: `FUN_140368bf0(uMenuChrSel, player, teamSlot)` receives **character IDs** (clamped 0..0x33 by `FUN_140455640`), converts to slot with `FUN_140361f50`, and pushes it to the cursor (`FUN_140369090`) and BgMain. So online sync is by chrId, not by cursor movement — local cursor/page changes are not part of the synced state (to be verified live).

### What Clone Engine most likely does (inference; verify once CE is installed)
- Raises the row count passed to `FUN_140373280` (7 → 7+k) so the cursor can move into y ≥ 7, and hooks `FUN_140361fd0` / `FUN_140361fa0` (and the 0x33 clamps) to return clone chrIds for those slots. No icons/highlight exist for y ≥ 7 (`FUN_14036e820` drops them), hence "scrolling into empty space".

## DESIGN DECISION (Will, 2026-09-30): split-screen paging

- **Two-player select (versus, online):** each player owns one half of the grid. P1 = left face (`chs_meku_face_a`, x 0..3), P2 = right face (`chs_meku_face_b`, x 4..7). Each half is a **28-slot page** that the owning player pages independently. Page 1 = the vanilla left half (Capcom), page 2 = the vanilla right half (Marvel), pages 3+ = Clone Engine characters in CE's order. Cursor is confined to the owner's 4 columns (horizontal wrap within 4). Opponent's cursor is shown only on the opponent's side.
- **Solo select (arcade, training, mission, any mode with one picker):** P1 uses the **full width**, 56-slot pages, vanilla look on page 1; CE characters on pages 2+.
- Both cases: shoulder buttons flip pages; simple page indicator per side; `f_Hatena` ("?") as the placeholder icon when a clone has no face texture.
- Rejected for now: shrinking icons to fit more per page (needs new mesh + animation assets; ~3-4x the effort; can be a later extension by changing the page size).

## Live test 1 (vanilla + diagnostic ASI 0.0.1, 2026-09-30) — PASS

Log: `notes/test1_vanilla_diag.log`. All four hooks installed; version check passed; no crash.
- Cursor objects created per player with **cols=8 rows=7 total=56** (confirms `0x140373280` args and inner offsets +0x54/+0x58/+0x5c).
- `buildIcons` (0x14036df90) runs once at screen load, before the first cursor position is pushed.
- P1 starts at slot 26 (x=2,y=3) = Ryu (chrId 1). Horizontal wrap 31→24 and 24→31 observed; vertical wrap 49 (y=6) → 9 (y=1) because y=0 at x=1 is empty and the blocked-skip in `0x140323b20` continues past it; 7 (y=0) ↑ → 55 (y=6).
- Every position pushed to `BgMain::setCursorPos` was < 56 on vanilla, as expected.

## Community Edition 1.2.3 / Clone Engine — what it actually patches (2026-09-30)

Package: Nexus 286 v1.2.3 (zip 8.2 GB, kept at `D:\umvc3_ce\`, never in the repo). Ships: `dinput8.dll` (its own ASI loader build), `CloneEngine.asi` (2023-08-12, by "monol" = Gneiss; release, MSVC), `ColorExpansion.asi` (debug build → needs the shipped `msvcp140d/ucrtbased/vcruntime140d.dll`), `mag_patch.asi` (Rust, 2026-05, with pdb), `mod_enable_damage_counter.asi`, `Characters.ini` (106 `[CharacterN]` blocks: CharacterID, BaseCharacter, SoundID, NumColors, Child1..5), `nativePCx64/CloneEngine/{AssistDef.csa, AssistMsg.msd, BGM.stqr, EndingMsg.msd}`, replacement `ui/mnchs*.arc`, `mnchscmn_en.arc`, `game*.arc`, `mnmain*.arc`, loose `ui/chs/chs_face_a/chs_cs_f/*.tex` (1018 files), `chs_b1p/chs_body`, `chs_b1p/chs_as_n`, plus `chr/`, `sound/`.
Install per its readme = copy everything over the game folder, replace on conflict. Hidden slots are 8 wide, below the vanilla 7 rows (see `Character_Slot_Locations.png` in the zip).

CloneEngine.asi computes `gameBase + RVA` and writes inline patches (VirtualProtect + memcpy). Relevant ones (disassembled at CloneEngine 0x1800031c0..0x180003420):
- Allocates **0x3000 bytes at fixed address `0x1B0000000`**, copies the vanilla 56-entry grid table there, then appends one slot per clone: **clone chrId = 0x3c + index** (60+, matches MarvelCloneTool's base 59+N), skipping entries whose `[+0x94] < 0`.
- Patches the **disp32 of the `lea` at `0x140361fe8`** (inside `lookup(x,y)` 0x140361fd0) and at **`0x140361f55`** (inside `chr→slot` 0x140361f50) to point at that table. Function entries are untouched → entry hooks on both functions still work and see CE's table.
- **Rewrites the 12 bytes of `0x140373280` (uiCursor::setDims)** to `mov [rcx+0x54],edx; mov edx,ROWS; mov [rcx+0x58],edx; ret` with **ROWS = (totalSlots*4)/32 + 1** (56+106 slots → 21 rows). +0x5c is no longer written.
- Other inline patches (roster-size constants 0x33/0x34 etc.): 0x14036db26, 0x14036db65, 0x14036f856, 0x14036f9bc, 0x14036fbc9, 0x1403707cf, 0x140371755, 0x140360deb/e2d/e6f, 0x14036c061, 0x140058f96/fb6 (name table → CE's), 0x140059526, 0x1400592cd, 0x14022667c, 0x1402ab417/41a/4c2, 0x1402af5a2, 0x1402afa10, 0x1402afe80, 0x1402bc1ba/24c/9a2/9c2, 0x1402fab87, 0x14033ee73/ef8f/f0d7, 0x140384100, 0x14038c1e4/98f, 0x14042f928/fb09, 0x140448f0f, 0x1401cbac8, 0x14020c34c, 0x14017e9c0, 0x1400d158b/d358b/e840f/f1d8b, 0x140087983, 0x14007417a.
- ColorExpansion.asi touches 0x14036dab0, 0x1403704dc.., 0x1403741d9 (CardPlayer colour counts) and 0x1402b9ad0/0x1402bc1ad.., not our functions.
- **None of CE's patches hit the entries of the functions we plan to hook** (0x140361fd0, 0x140361fa0, 0x14036e820, 0x14036df90, 0x14036ce80, 0x140323920, 0x140372900, 0x140373120, 0x140372e50). Load order: the ASI loader loads `CloneEngine.asi` before `UMvC3PageTurn.asi` (alphabetical), so CE's inline patches are in place before our hooks go in.

## Crash with CE + our ASI (2026-09-30) — ROOT CAUSE + FIX

- Symptom: game dies ~6 s after launch, `ucrtbase.dll` fail-fast 0xc0000409 (data 5 = invalid parameter); CE's `Log.txt` stops at step `0`.
- Cause: CE step 0 (`0x180001e20`) does `VirtualAlloc(0x13FFF0000, 0x3000, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE)` — a **fixed-address code cave just below the exe**. MinHook's trampoline block search (`FindPrevFreeRegion` from the hook target downward) picked the first free 64 KB below the exe = `0x13FFF0000`. CE's fixed alloc then fails → `_errno=EINVAL; _invalid_parameter_noinfo()` → abort.
- CE fixed addresses (all 0x3000 bytes): code caves at `0x13FF40000, 0x13FF50000, 0x13FF70000, 0x13FF90000, 0x13FFA0000, 0x13FFB0000, 0x13FFE0000, 0x13FFF0000` (+ several more without a visible movabs, presumably the other 0x13FFx0000 pages); data tables at `0x150000000, 0x160000000, 0x190000000, 0x1A0000000, 0x1B0000000 (grid table), 0x200000000, 0x210000000, 0x220000000, 0x260000000`.
- Fix: vendored MinHook `buffer.c` now refuses to place trampoline blocks in `[0x13F000000, 0x140000000)` and `[0x140F00000, 0x300000000)` (`IsReservedForCloneEngine`). Verified: block now lands at `0x13EFF0000`, CE logs steps 0..18 + Done, game stays up.
- Also learned: hooking `0x140373280` (setDims) is unnecessary and CE rewrites that function body later; keep hands off it.
- Rule for the future: **any new hook library / code cave must avoid those ranges.**

## Live test 5 (CE 1.2.3 + diagnostic 0.0.5) — PASS. Log `notes/test5_ce_diag.log`

- CE patches are applied **after** our init but **before** the select screen: at cursor ctor the lookup table is at `0x1B0000000`, 141 entries (56 vanilla + 85 clones; children skipped, e.g. slot 63 → chrId 68), setDims body = `89 51 54 BA 12 00 00 00 89 51 58 C3` → **rows = 18** (0x12), cols = 8; inner +0x5c stays 1.
- Cursor freely reaches y = 7..17; every slot ≥ 56 pushed to BgMain::setCursorPos is dropped by the game (no highlight), as predicted. Hidden characters selectable (Will).
- Grid size for our paging: 8 × 18 → split mode 3 bands × 2 halves = **6 pages of 28**; solo mode **3 pages of 56**.

## Phase 2 status (2026-09-30 evening)

- **Test 12 (0.1.6) PASS**: split paging works in Versus — P1's left half pages through CE rows, cursor highlight follows, right half untouched, modded pick works. Screenshots in `notes/screenshots/`.
- Repaint lessons: (1) the game's `buildIcons` cannot be re-run at runtime for clone ids (no `f_` icons exist for clones → engine "Failed open file" fatal dialog); (2) at runtime the resource manager (`mgr->vt+0x60`, fn `0x1402B4CD0`) does NOT resolve arc-packed textures by name, it goes straight to disk. So we cache the texture objects the game loads during its own screen-load `buildIcons` (hooked loader) and reuse them; loose files (CE `b_<Name>255` bodies, our `ui/PageTurn/blank`) load fine at runtime.
- Clone grid icons: CE ships none; we show the CE body portrait `b_<Name>255_BM_HQ_NOMIP` (silhouette for many) squeezed into the icon. Proper face crops = later polish.
- Empty cells: our own 128x128 BC3 fully-transparent `assets/nativePCx64/ui/PageTurn/blank_BM_HQ_NOMIP.tex` (Hatena header + zero blocks).
- Open: Random / Random-all / Hatena not captured by the cache (paths logged in 0.1.7 to fix); page indicator; shoulder buttons; solo-mode test; online test.

## Clone icons (2026-09-30)

- CE 1.2.3 slot order (56+k) = `Characters.ini` block order **excluding entries named in any `ChildN=`** (85 entries; matches the 141-entry runtime table). Saved in `notes/ce_slot_order.txt`. Verified against `Character_Slot_Locations.png` row 1 (Rash, KennFist, Gui, Chl | Jeannix, WL3, Psylock, Cyclop).
- Icon convention for creators: `nativePCx64\ui\chs\chs_face_a\chs_cs_f\f_<CharacterID>00_BM_HQ_NOMIP.tex`, 128x128 BC3 (docs/ICONS_FOR_CREATORS.md). Converter: `scripts/make_icon.py` (own BC3 encoder, header from vanilla icon).
- For Will's install only: 85 icons cropped from CE's own `Character_Slot_Locations.png` (cells: rows y=730,811,891,971,1050,1130,1208,1286,1367,1446,1523; cols 0/79/158/237/316 and 385/463/542/620/699; drop 18 px name strip). Kept in `test_icons/ce_faces/` (gitignored: derived from CE art; ask the CE team before shipping).

## Icon format resolved (2026-09-30)

- Vanilla `f_*00_BM_HQ_NOMIP.tex` are format **0x2a (BM_HQ)** = DXT5 with a colour swizzle (luma in alpha, chroma in R/B, G = coverage; YCoCg-like, exact transform unknown). Plain-RGB data in a 0x2a file renders magenta/green.
- Format **0x17 (BM_XLU, plain DXT5)**, as used by CE's `b_*255` bodies, renders correctly on the grid nodes (test 16). `scripts/make_icon.py` writes 0x17 now. Own BC3 encoder verified bit-identical to DirectXTex texconv on a test block.
- Modded pages skip the 4 logo cells (x=1,2,5,6 on the top row): 26 cells per half-page, 52 per full page. CE slot count is scanned at cursor ctor (`g_ceCount`, 85 for CE 1.2.3).
- Will's navigation preference: only left/right flip pages; up/down wrap inside the page (0.2.1).

## Scope decisions (Will, 2026-09-30 evening)

- No page indicator, no shoulder-button paging, no page-turn animation/sound. Left/right-only flipping is final.
- CE-cropped face icons are NOT shipped (artists will make better ones). Converter + docs stay for creators. Fallback chain stays: f_<ID>00 -> b_<ID>255 body -> blank.
- Online cannot be tested (no second player available); ship as "untested online" with the technical reasoning (game syncs picks by character ID).
- Shipped version baseline: 0.2.1.

## Minimal Clone Engine install (2026-09-30) — what a trimmed install actually needs

`scripts/make_minimal_ce.ps1 -Count N`: clean vanilla + Clone Engine + first N pack characters (plus their children). Findings while trimming:
- `CloneEngine.asi` + `dinput8.dll` + `nativePCx64/CloneEngine/*` + `Characters.ini` + per-character files (`chr/archive/<ID>_*.arc`, `sound/se/chr/archive/<ID>.arc`, `sound/bgm/source/<ID>.sngw`, `sound/event/<SoundID>/`, `ui/**/*<ID>*`, `ui/ending/ending_<ID>.arc`) are not enough.
- **ColorExpansion.asi is required** (Arcade and Training load `b_<Name>99/255` portraits through it) together with `ColorExpansion.ini`, the debug CRT DLLs it links (`msvcp140d`, `vcruntime140d`, `ucrtbased`), its `nativePCx64/ColorExpand/*.msd`, and the pack's **whole `nativePCx64/ui` tree** (the 255 portraits for the base roster are inside the pack's replaced `mnchs*.arc`).
- Game mode ids (`[game+0x34c]`, `FUN_1400044b0`): **Versus = 0**, Arcade = 1, Training = 5 (from the 19:51 live log, 0.2.4). Both cursors are constructed and enabled in every mode, so "split vs solo" must come from the mode id, not from the cursor flags.

## Test 24 (0.2.4, Versus) — FAIL, fixed in 0.2.5

- Arcade and Training OK. Versus: once a player confirmed a character (assist not yet chosen), their cursor stopped ticking, so the 500 ms "both cursors ticking" heuristic flipped SplitMode() to solo and the other player could roam the full width (onto the opponent's half). Picking the assist re-enabled the cursor and split came back. Also a stretched P1 marker: P1's real pos chosen while in bogus solo mode mapped, once split returned, onto a top-row logo hole cell (`Screenshot_3.png`).
- 0.2.5: SplitMode() keys off the mode id read at cursor ctor (`g_modeId`): 0 = split, 1/5 = solo; the tick heuristic is only a fallback for mode ids not yet identified (Mission, Online, Heroes & Heralds still unknown; the `mode:` line in the log records them).

## Test 25 (0.2.5) — PASS. Mode ids

- Versus split mode stays split after one player confirms (the 0.2.4 bug). Arcade and Training solo OK.
- Mode ids seen: Versus = 0, Arcade = 1, Training = 5, Online lobby pick = 5, Mission = 5 (Mission has no regular select screen; out of scope). Real online two-player select untestable (no opponent); its id is unknown.
