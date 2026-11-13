# OpenTyrian2000 for classic Mac OS (Retro68 + SDL3)

Runs on System 7.x through Mac OS 9, as a 68k application or a PowerPC (CFM/PEF)
application. Uses the SDL3 classic Mac OS port (video through QuickDraw/CopyBits,
audio through the Sound Manager) installed into Retro68.

## Requirements

- Retro68 in `/opt/retro68` (`RETRO68=...` to change)
- SDL3 built and installed for each CPU, in
  `/opt/retro68/m68k-apple-macos` and `/opt/retro68/powerpc-apple-macos`
- the Tyrian 2000 data files in `data/`

**68k SDL3 must be built without precompiled headers.** GCC's precompiled
headers drop Retro68's `#pragma parameter`, so every register-based Toolbox
trap in SDL (File Manager `PB*` calls, `NewPtr`, `DisposePtr`, ...) is called
with its arguments on the stack and the application crashes (error type 3)
at the first one, e.g. in `SDL_GetBasePath()`. PowerPC is not affected. Build
the 68k library with:

```sh
cmake -S SDL3 -B build-m68k \
    -DCMAKE_TOOLCHAIN_FILE=/opt/retro68/m68k-apple-macos/cmake/retro68.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
cmake --build build-m68k && cmake --install build-m68k
```

## Building

```sh
make -f Makefile.retro68 ARCH=m68k            # build-retro68/m68k/
make -f Makefile.retro68 ARCH=ppc             # build-retro68/ppc/
make -f Makefile.retro68 all-archs            # both
make -f Makefile.retro68 ARCH=m68k disk       # + opentyrian2000-data.dsk
```

Options: `M68K_CPU=68020|68040` (FPU build, own build directory),
`SDL3_PREFIX=<dir>` (SDL3 not in the Retro68 sysroot), `MACCLASSIC_LOG=1`
(stdout/stderr go to `OpenTyrian2000 log` next to the application),
`PREFERRED_SIZE_KB` / `MINIMUM_SIZE_KB` (memory partition, default 16 MB / 8 MB).

Outputs: `opentyrian2000.bin` (MacBinary), `opentyrian2000.dsk` (HFS image
with only the application), and with `make disk` `opentyrian2000-data.dsk`
(HFS image with the application and a `data` folder, for Basilisk II,
SheepShaver, QEMU or a real Mac).

## Files

- the game looks for its data in `<application folder>:data:`, then in the
  application folder itself;
- settings and saves go to `System Folder:Preferences:OpenTyrian2000:`.

## Notes

- 68k builds generate audio at 22.05 kHz instead of 44.1 kHz to save CPU time.
- SDL3 on classic Mac OS has no threads: audio is mixed whenever the game pumps
  events or delays, which it does every frame.
- No joystick, network or MIDI support.
