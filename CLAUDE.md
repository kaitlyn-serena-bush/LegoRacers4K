# racers-portable — agent notes

Cross-platform port (SDL3) of the [LEGO Racers decompilation](https://github.com/isledecomp/racers), modeled on [isle-portable](https://github.com/isledecomp/isle-portable). Windows/Linux/macOS today; emscripten deferred.

## Ground rules

- **The decompilation is the source of truth.** `LEGORacers/`, `GolDP/`, `common/` and `util/` come from the upstream decomp (git remote `racers`) and must stay mergeable: keep reccmp annotations (`// FUNCTION:`, `// GLOBAL:`, …) intact and do not rename or reorganize decomp code here.
- **Upstream-first policy.** Any change the original 32-bit binary could still byte-match (type modeling, renames, restructuring) is made in the decomp repo first — verified there with a full reccmp report diff — and arrives here by merge/rebase. Only genuine portability work is made directly in this repo.
- **Keep the touched-file surface tiny.** Rewritten app boundary: `LEGORacers/src/app/main.cpp`, `win32golapp.cpp`, `LEGORacers/src/video/` (player, AVI demuxer). Everything else in decomp directories should compile unmodified against the `miniwin/` headers; pointer-width fixes are a few lines each and tagged `// 64-bit compatibility:`. Platform hooks in decomp-adjacent code are tagged `// [library:xxx]` (window, filesystem, synchronization, input, audio, 2d, 3d, video, config).
- All platform work lives in `miniwin/` (DirectDraw/D3D6-IM, DirectSound, DirectInput 5 and a small Win32 surface over SDL3 + miniaudio) and `3rdparty/` (vendored FFmpeg n4.4.4 Indeo 5 decoder behind a minimal shim; `Indeo5_Open/Decode/Close`).
- `clang-format` everything you touch (CI-enforced; the vendored FFmpeg sources under `3rdparty/ffmpeg-indeo5/` except the wrapper are exempt and must stay byte-unmodified).

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

`DOWNLOAD_DEPENDENCIES=ON` (default) fetches SDL3 at a pinned release; with it OFF, a system SDL3 is found via `find_package`. A game installation (LEGO.JAM, `*.TUN`, Indeo 5 `*.AVI`) is required to run: `./build/LEGORacers --path <gamedir>`.

## Testing aids (environment variables)

- `RACERS_AUTOKEY="ms:scancode[:holdMs],..."` — scripted key injection (SDL scancodes; Alt/Ctrl/Shift chord state is tracked).
- `RACERS_TOUCH=1` — mouse-as-finger testing: SDL mouse→touch synthesis feeds the touch layer so drag-steering/buttons/taps work without a touchscreen (touch-synthesized menu clicks are suppressed for these fingers to avoid doubling the real mouse).
- `RACERS_AUTOTOUCH="ms:d|m|u:nx:ny[:finger],..."` — scripted touch injection (normalized window coords; hold taps ≥50 ms so the per-frame edge polling sees them; `finger` defaults to 1, so a concurrent second finger needs an explicit `:2`).
- `RACERS_DUMP_EVERY=ms:prefix` / `RACERS_DUMP_FRAME=N:path` — periodic / single-frame screenshots.
- `RACERS_WATCHDOG=<ms>` — reports game-thread stalls with the miniwin phase they occurred in; slow operations (>30 ms) log themselves.
- `RACERS_GL_STATS=1` — per-100-frame draw stats and frame-time min/avg/max (run with `SDL_LOGGING="*=info"`).
- `RACERS_VIRTUAL_PADS=N` attaches N SDL virtual gamepads; `RACERS_AUTOPAD="ms:pad:button[:holdMs],..."` presses SDL gamepad button numbers on them (pad 1-4). `RACERS_CONTROLS=<file>` overrides the controls.ini path.
- `RACERS_VSYNC=1`, `RACERS_WINDOW_TITLE=...` — vsync A/B lever; distinct window title for scripted test instances.

## Local multiplayer (LegoRacers4K)

Versus supports 2-4 players (split screen; 3-4 use quadrants). Flow: main menu VERSUS -> `VersusSetupScreen` (menu 0x04, reuses options.mib: race type Single/Grand Prix, players, laps) -> track or circuit pick (RaceModeSetupScreen) -> `CoopPickScreen` (menu 0x1e, reuses pickrcr.mib): every player joins at once by pressing accelerate on a pad or keyboard set, picks with their own left/right, locks in with accelerate; the joined preset overrides `[players]` for that session (`CoopControls_SetSessionPreset`). The screen sets `g_menuRawInput` (menu focus navigation off) and raw pad button mapping while open. Versus controls come from `controls.ini` next to the exe (`LEGORacers/src/app/coopcontrols.cpp`; default: player N = gamepad N if plugged in, else keyboard set N). miniwin exposes four fixed DirectInput joysticks ("Gamepad 1-4") bound to SDL gamepads with hot-plug (`miniwin/src/dinput/dinput.cpp`); RT/LT also press A/B; Start pauses.

## Player-facing options

`--path <dir>`, `--language <index>`, `--scale letterbox|stretch`, `--resolution native|original`, `--widescreen on|off`, plus the original arguments (`-window`, `-novideo`, …). No `-window` means fullscreen. Alt+Enter toggles fullscreen.

Widescreen (default on, letterbox mode): the backend canvas grows to the window aspect (capped at 21:9) and game x coordinates are shifted by `MiniwinGetWideExtra()`; menu backdrops, fonts and images may draw into the side areas (clip widened in `GolImage::DrawStretched`/`GolFontBase::ComputeClipRect`), full-screen race HUD rects stretch to the real edges (`RaceHud::SetLayout`), and race cameras touching a screen edge extend their viewport into the sides (Hor+, `racesession.cpp`).

Options are one page (opponents, music/sound volume, language, controls, credits); video options and the Pick Language screen are gone. QUIT exits without confirming; language changes live (the options screen reopens via saveall to reload strings); volume bars are narrower with one-line titles. In races, M cycles none/map/speedometer/rotating map/speedometer+map; the speedometer shows studs per second (1 world unit = 1 stud, `RaceHud::DrawSpeedReadout`). The port's old 50 ms per loading step throttle is gone. all circuits and tracks are unlocked (`gamestate.cpp`).

Touch (touchscreens/web): always-on additive controls — left-half drag steers (thumb held = throttle), drawn buttons for power-up/slide/brake + pause/look-back, tap-to-click menus, pause-dialog ▲✓▼ cluster, OSK name entry. Single local player only: Versus split-screen sessions stand the whole layer down (no overlay, fingers ignored; menus unaffected). Layer lives in `miniwin/src/dinput/touch.cpp` + `miniwin/src/d3d/backends/overlay.cpp`; game-side hooks are tagged `// [library:input]` (playercontrols.cpp, racesession.cpp, menutextfield.cpp, driverlicensescreen.cpp).
