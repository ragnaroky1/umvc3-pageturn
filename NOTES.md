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
- Fields: **+0x4c = cursor slot index** (linear, row-major: row = pos / cols, col = pos % cols), **+0x54 = column count (7)**, +0x50 = probably row count (8), +0x64 = flags (bit 2/4 lock input), +0x74/+0x78 = repeat timers, +0x84 = player index, +0x88/+0x8c = confirm state, +0x110/+0x11c/+0x124 = anim state.
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
