# UMvC3 PageTurn

Pages the Ultimate Marvel vs. Capcom 3 character select grid so Clone Engine characters
appear on extra pages of the normal grid instead of in hidden rows below it.

- **Versus:** each player pages their own half of the grid. P1 owns the left (Capcom) half,
  P2 the right (Marvel) half. Press left or right past the edge of your half to flip pages.
  Page 1 is the vanilla Capcom half, page 2 is the vanilla Marvel half, pages 3+ are the
  Clone Engine characters in `Characters.ini` order.
- **Arcade and Training:** the whole grid is one page. Page 1 is the vanilla roster, pages
  2+ are the Clone Engine characters.
- Up and down wrap inside the current page. Only left and right flip pages.
- Works with any roster size. The mod reads the Clone Engine roster at runtime.

## Requirements

- Ultimate Marvel vs. Capcom 3 on Steam (the April 2017 exe). On any other exe the mod shows
  a warning and disables itself.
- UMvC3 Community Edition with Clone Engine installed and working. Tested with Clone Engine
  1.2.3. Community Edition already installs the ASI loader (`dinput8.dll`) that loads this mod.

This download contains only the PageTurn mod. It does not include any Capcom, Community
Edition, Clone Engine, or ASI loader files.

## Install

1. Install Community Edition first and confirm it runs.
2. Copy `UMvC3PageTurn.asi` into the game folder, next to `CloneEngine.asi`:
   `...\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3\`
3. Copy the `nativePCx64` folder from this download into the game folder, merging with the
   existing one. It adds a single file: `nativePCx64\ui\PageTurn\blank_BM_HQ_NOMIP.tex`
   (the transparent icon used for empty cells).
4. Start the game. `UMvC3PageTurn.log` appears in the game folder when the mod loads.

## Uninstall

Delete `UMvC3PageTurn.asi`, `UMvC3PageTurn.log`, and the `nativePCx64\ui\PageTurn` folder
from the game folder. Leave `dinput8.dll` alone, Clone Engine needs it.

## Icons for modded characters

The game only has grid icons for the base roster. A Clone Engine character with no icon shows
its body portrait squeezed into the cell, which is a plain silhouette for many characters. If
the portrait is missing too, the cell is blank. Character creators can add a proper icon with
one file; see `docs/ICONS_FOR_CREATORS.md`.

## Known limitations

- **Online is untested.** No two-player online match has been run with the mod. The game
  sends character ids, not grid positions, so it is expected to work, but treat it as unverified.
- Mission mode does not use the regular character select screen and is not affected.
- Hidden cells under the CAPCOM and MARVEL logos on the top row are never used on modded pages,
  so each modded half-page holds 26 characters (52 per full page).
- No page number indicator, no page-turn animation. Flipping is left/right only.

## Troubleshooting

`UMvC3PageTurn.log` in the game folder records what the mod did. If the mod does not load,
check that `dinput8.dll` and `CloneEngine.asi` are present and that Community Edition works
without PageTurn. If you report a problem, include the log.

## Third-party code

PageTurn is built with MinHook (BSD 2-clause, Copyright (C) 2009-2017 Tsuda Kageyu). Its
license is in `LICENSE-MinHook.txt`.
