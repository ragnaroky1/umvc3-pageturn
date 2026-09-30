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
