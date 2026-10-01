# Character select icons for modded characters

UMvC3 PageTurn shows Clone Engine characters on the character select grid. The game only has
grid icons for the base roster, so modded characters need one more picture. If a character
has no icon, PageTurn falls back to the character's body portrait (`b_<CharacterID>255`), and if
that is missing too, to a blank cell.

## What to make

| | |
|---|---|
| File name | `f_<CharacterID>00_BM_HQ_NOMIP.tex` |
| `<CharacterID>` | exactly the `CharacterID=` value from your `[CharacterN]` block in `Characters.ini` (case matters) |
| Folder | `<game>\nativePCx64\ui\chs\chs_face_a\chs_cs_f\` (loose file, not inside an .arc) |
| Size | 128 x 128 pixels |
| Format | MT Framework TEX, plain BC3 / DXT5 (texture format 0x17), no mipmaps - the same format as the Community Edition body portraits (`b_<CharacterID>255`). Do not copy the vanilla icons' header: they use a special colour encoding (format 0x2a) |
| Content | head / upper body shot, like the vanilla icons |

Example: `CharacterID=Rash` -> `nativePCx64\ui\chs\chs_face_a\chs_cs_f\f_Rash00_BM_HQ_NOMIP.tex`

## Easiest way: the converter script

`make_icon.py` is in the `docs` folder of the download (`scripts` in the repo). Any PNG or JPG works. It is center-cropped to a square and resized to 128 x 128.

```
python make_icon.py my_picture.png Rash
```

That writes `icons_out\f_Rash00_BM_HQ_NOMIP.tex`. Copy it into
`nativePCx64\ui\chs\chs_face_a\chs_cs_f\`. Needs Python 3 with Pillow (`pip install pillow`).

To get a placeholder with just the name on it:

```
python make_icon.py --label Rash
```

Other tools that write MT Framework `.tex` files (for example EternalYoshi's texture converter
from umvc3-tools) also work, as long as the result is 128 x 128 BC3 with the file name above.
