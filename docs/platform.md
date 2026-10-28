# Platform layer

One front end per target owns `main`, the event loop and the machine primitives.
Everything above that — the engine and the UI dispatcher — is shared.

## The shared contract

The engine never calls SDL or a platform API directly. It sends `CSB_UI_MESSAGE`
values (`UI.h:97`, codes `UIM_*` at `UI.h:41`) through the single entry point
`CSBUI()` (`CSBUI.cpp:735`). `CSBUI.cpp` is cross-platform: it is a large `switch`
over the message types that either handles the message or drives the ported state
machine through `Dispatch()`. The platform files supply only the primitives
declared in `UI.h` — `UI_MessageBox`, `UI_ScreenPresent`, `UI_PlaySound`,
`UI_Initialize_sounds`, `UI_Invalidate`, and so on. For example the Linux
`UI_MessageBox` is `LinCSBUI.cpp:416`.

This split is why the same engine builds everywhere:
`LinCSBUI.cpp` (Unix/Linux), plus per-target screen and audio files, implement the
`UI.h` contract; `CSBUI.cpp` and everything under it are shared.

## The platform mains

| Target | Entry point | Notes |
|--------|-------------|-------|
| Linux | `main`, `CSBlinux.cpp:1286` | SDL2 + Dear ImGui; the reference implementation |
| macOS | `main`, `CSBMac.cpp:163` | Older DrawSprocket path (`MacScreen.cpp`); bundle support in `MacAppSupport.cpp` |
| Raspberry Pi | `main`, `CSBraspian.cpp:394` | SDL 1.2 build (`target = Raspberry` in `src/makefile`) |
| Windows | `WinMain`, `CSBwin.cpp:150` | Win32 front end; `wWinMain` at `CSBwin.cpp:139` |
| Pocket PC | `WinMain`, `CSBWinCE2002.cpp:111` | Windows CE 2002 build |

The build selects between them in `src/makefile`: the `target` variable
(`SDL20` by default) picks the compiler flags and libraries, and the `WIN` /
`MACOS` conditions add the platform objects. Cross-compiling to Windows from
Linux uses the mingw targets (`x86_64-w64-mingw32`, `i686-w64-mingw32`).

## What the Linux front end does

`CSBlinux.cpp` is the clearest example of the shape every platform follows:

1. **Start-up.** Set the root folder, parse options (`UI_ProcessOption`,
   `CSBlinux.cpp:1310`), read `csb.cfg` (`read_conf`, `CSBlinux.cpp:1888`).
2. **Window and backends.** Create the SDL window/renderer, the Dear ImGui
   context, load fonts (`CSBlinux.cpp:1501-1523`).
3. **Engine init.** Send `UIM_INITIALIZE` through `CSBUI` (`CSBlinux.cpp:1565`).
4. **Clock.** Start the SDL timer that fires `UIM_TIMER`
   (`SDL_AddTimer`, `CSBlinux.cpp:1572`; callback `__Timer_Callback`,
   `CSBlinux.cpp:142`).
5. **Event loop.** `while (true)` at `CSBlinux.cpp:1585`: wait for an SDL event,
   let ImGui process it, then route it to `Process_SDL_*`
   (`CSBlinux.cpp:1651-1660`). The automap window gets first refusal on pointer
   and key events (`CSBlinux.cpp:1603-1608`).

See [architecture.md](architecture.md) for the wider data flow.

## Platform differences worth knowing

- **Raspberry Pi** uses SDL 1.2, so it does not have the SDL2/ImGui stack; its
  `makefile` target sets different flags (`-D SDL12`).
- **macOS** carries the newer SDL path in `CSBlinux.cpp`/`MacScreen.cpp` and the
  older classic-Mac code guarded by `CSB_CLASSIC_MAC`
  (`CSBTypes.h`); the current build is the little-endian SDL one.
- **macOS bundles** resolve their data folder at start-up with
  `UseApplicationSupportFolderWhenBundled` (`MacAppSupport.cpp:107`, called from
  `CSBlinux.cpp:1303`), which is why a built app keeps saves under
  `~/Library/Application Support/CSB/`.
- **Windows / Pocket PC** still carry resource files (`resource.h`,
  `CSBWinCE2002.rc`) and the Visual Studio project files in `src/`.
