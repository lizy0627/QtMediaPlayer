# Legacy Inventory

## Danmaku

The following root-level danmaku files are legacy compatibility code and are no
longer used by the new `danmaku/` module or listed in `QtMediaPlayer.pro`:

- `danmakumanager.h`
- `danmakumanager.cpp`
- `danmakuinput.h`
- `danmakudisplay.h`
- `danmakuwidget.h`

Deletion conditions:

- No remaining production include references to `DanmakuManager`, `DanmakuInput`,
  `DanmakuDisplay`, or `DanmakuWidget`.
- Manual playback verification confirms the new `DanmakuController`,
  `DanmakuInputBar`, `DanmakuOverlay`, `DanmakuPanel`, and `DanmakuRepository`
  cover sending, overlay display, panel refresh, login checks, and "my danmaku"
  record lookup.
- Any historical notes or migration examples that still mention the old classes
  have been updated or moved under `tools/legacy/`.
