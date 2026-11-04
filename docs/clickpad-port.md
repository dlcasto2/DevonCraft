# Original TI-Nspire CAS ClickPad port

## Hardware and build target

- Device: original monochrome TI-Nspire CAS ClickPad / TouchPad family (not CX)
- OS: 3.9.0.463
- Boot2 noted for the user's device: 3.0.1.131
- CPU: ARM926EJ-S
- LCD mode: 320 x 240, 4-bit grayscale
- Runtime: Ndless; build with the Ndless r2020 toolchain

The calculator target is compiled for ARM with `-mcpu=arm926ej-s`. The source keeps
classic and color-device paths separate at runtime. A CX-family Boot2 image or CX OS
image is not part of this application build and must not be substituted for classic
firmware.

## Source audit

### Timer

`platform_time.cpp` selects `is_classic` and uses the classic timer-0 offsets:

- `0x900C0000`: current counter
- `0x900C0004`: divider
- `0x900C0008`: control

It saves the prior counter/divider/control values, freezes the timer while setting it
to divider `1`, counter `0`, and control `0x0F`, accumulates and clears the counter
when sampling ticks, then restores the saved values when the game exits. This matches
the classic Ndless timer binding's register selection and 32768 Hz tick rate; the
CX SP804 timer and clock-gate setup remains in its own branch. Reference:
<https://docs.rs/ndless/latest/src/ndless/bindings/timer.rs.html>.

### Display

`nGL/gl.cpp` checks `lcd_type() == SCR_320x240_4`. In that mode it allocates a 16-bit
intermediate buffer, inverts the rendered words for the classic panel convention,
and calls `lcd_blit` with `SCR_320x240_16`. Ndless documents that format as RGB444,
not as a native 4-bit grayscale buffer. The nGL renderer stores `COLOR` as RGB565,
and the current monochrome display path only inverts the 16-bit words; it does not
perform an explicit RGB565-to-RGB444 pack. `main.cpp` and `terrain.cpp` also run the
loading texture and terrain atlas through the existing grayscale conversion, but
that is not a verified final-framebuffer conversion. This is the main unresolved
ClickPad graphics item: confirm how the classic LCD path interprets the blit words
and add a measured format conversion if needed. No physical display test has been
performed here. See the [Ndless LCD API](https://www.hackspire.org/Libndls/) and
[Classic LCD controller notes](https://www.hackspire.org/Memory-mapped_IO_ports_on_Classic/).

### Input

`task.cpp` reads the classic keypad matrix at `0x900E0000 + key.row` when
`is_touchpad` is false and treats a cleared bit as pressed. Ndless key definitions
store the row as an already-offset value (for example `0x10`, `0x12`, etc.), so the
address expression points into the keypad data window at `0x900E0010`–`0x900E002F`.
ClickPad navigation is mapped through the physical keys; the color Touchpad branch
remains separate.

### Memory and performance notes

The 320 x 240 renderer allocates a 16-bit color buffer, a same-sized inverted
monochrome blit buffer, and a 16-bit depth buffer: approximately 450 KiB for those
three core buffers alone, excluding textures, world/chunk data, stacks, and Ndless/OS
memory. This is an estimate from the declared buffer dimensions and element types,
not a measured peak-RAM result. The current build uses fixed-point rendering and
there is no device-measured FPS or memory high-water mark in this archive.

## Backward-compatible user files

New installs use `/documents/ndless/devoncraft.map.tns` and the optional external
texture path `/documents/ndless/devoncraft.ppm.tns`. If only the old
`crafti.map.tns` save exists, DevonCraft loads it and keeps that legacy path active
for subsequent saves. The file is not moved or renamed; later in-game saves update
it in the normal way. An old `crafti.ppm.tns` external atlas is also accepted as a
fallback. Explicit file-association arguments take priority. The branding change
does not rewrite a save on startup.

## Build and validation status

Host-side logic tests are run with `make -C tests test`. The calculator build requires
`nspire-gcc`, `nspire-g++`, `nspire-ld`, `genzehn`, and `make-prg` from the Ndless r2020
toolchain; those tools are not included in this source archive. Do not treat source
compatibility or host tests as proof of a working calculator executable. The
`devoncraft.tns` and `devoncraft-stress.tns` builds, emulator run, and physical ClickPad
test must still be performed with the actual toolchain/device.

The user's OS and Boot2 images are reference material only. This project does not
modify, flash, or replace calculator firmware.
