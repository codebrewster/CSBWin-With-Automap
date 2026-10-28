# Rendering

How the game draws. The engine thinks in the original machine's terms — a
320×200 8-bit palettised screen — and a thin machine layer scales and blits that
to the real window. The port keeps the original picture decoding (`LZW`) and the
3D viewport maths, and adds SDL2 plus Dear ImGui for the window and dialogs.

## The screen model

The engine draws into an 8-bit logical screen (the "ST screen", 320×200). The
machine layer owns the physical surface and the palette:

- `setscreen` (`Graphics.cpp:133`) and `SetDLogicalBase` (`Graphics.cpp:150`) set
  the logical/physical bases.
- `WinScreen.cpp` (Windows and shared blitting) has `UnpackScreen`
  (`WinScreen.cpp:314`), the `BLT1`/`BLT2`/`BLT3` routines (`WinScreen.cpp:348`…),
  the palette builders `createPalette32`/`createPalette16`
  (`WinScreen.cpp:255`, `267`), and the change detector `HasAreaChanged`
  (`WinScreen.cpp:163`).
- `MacScreen.cpp` is the classic-macOS equivalent (`STBlt`, `UpdateScreenArea`).
- The platform-neutral interface the engine calls is `UI.h` — `UI_ScreenPresent`,
  `UI_Invalidate`, `UI_SetDIBitsToDevice`, and friends.

Overlays are the translucent palette effects: `SetOverlayPalette`
(`Bitmaps.cpp:651`) and `SelectOverlay` (`Bitmaps.cpp:687`).

## Picture decoding (`graphics.dat`)

Graphics are stored compressed in `graphics.dat` and expanded on demand:

- `openGraphicsFile` (`Graphics.cpp:1821`) and `ReadGraphicsIndex`
  (`Graphics.cpp:1970`) open the file and read the index.
- `ReadGraphic` (`Graphics.cpp:1772`) and `ReadAndExpandGraphic`
  (`Graphics.cpp:2677`) decode one picture; `LZWExpand` (`Graphics.cpp:2784`) is
  the decoder.
- `GetBasicGraphicAddress` (`Graphics.cpp:2588`) caches decoded pictures.
- CSB uses a second file: `OpenCSBgraphicsFile` (`Graphics.cpp:1898`) and
  `ReadCSBgraphic` (`Graphics.cpp:3294`).

`Bitmaps.cpp` loads uncompressed BMP assets instead — portraits and icons via
`ImportBitmap` (`Bitmaps.cpp:88`, `246`). The automap decodes the same pictures
in `AutoMapSprites.cpp`; see [automap.md](automap.md).

## The 3D viewport

`Viewport.cpp` draws the first-person view. The entry point is `DrawViewport`
(`Viewport.cpp:6677`); `FloorAndCeilingOnly` (`Viewport.cpp:3335`) and
`FillViewportCanvas` (`Viewport.cpp:6601`) handle the background, and
`AffineTransform1`/`AffineTransform4` (`Viewport.cpp:252`, `198`) do the picture
transformations. A `GRAPHICT...` trace (`OpenGraphicTraceFile`,
`Viewport.cpp:16`) can dump each drawn graphic.

## Dear ImGui

The UI dialogs (options, file browser, message boxes) use Dear ImGui, vendored in
`src/imgui/`. The context and SDL renderer backend are set up in the platform
main — for Linux, `CSBlinux.cpp:1513-1514` — and the fonts are loaded at
`CSBlinux.cpp:1517-1523` (`Vera.ttf`, plus `diogenes.ttf` and `OldeEnglish.ttf`
for scrolls and wall writing). `LinCSBUI.cpp` deliberately keeps the ImGui
includes above `CSB.h` (`LinCSBUI.cpp:1`) because `CSB.h` turns on
`#pragma pack(1)`.

## Sound

`Sound.cpp` recreates the original sound engine: `StartSound`
(`Sound.cpp:614`), the per-source volume/priority filter `SoundFilter`
(`Sound.cpp:637`), `playSound` (`Sound.cpp:844`) and `PlayCustomSound`
(`Sound.cpp:1010`). `SoundMixer.cpp` provides the mixing channels
(`Mix_OpenAudio`, `SoundMixer.cpp:555`, and the rest of the `Mix_*` API), and the
vendored `src/sdl_sound/` decodes MP3/WAV. `UI_Initialize_sounds`
(`LinCSBUI.cpp:448`) wires the platform audio in; `UI_PlaySound`/`UI_StopSound`
in `UI.h` are the engine-facing calls.

Sound files are optional — see [../README.md](../README.md) and
[../BUILDING.md](../BUILDING.md).
