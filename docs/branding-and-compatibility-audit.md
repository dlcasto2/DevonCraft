# DevonCraft rename and ClickPad audit

## Branding applied

- Runtime UI title and help credit use **DevonCraft**.
- Makefile targets and Zehn application metadata produce `devoncraft.tns` and
  `devoncraft-stress.tns`; PC build targets use the matching DevonCraft basename.
- GitHub Actions checks out into a `devoncraft/` work directory and uploads a
  `devoncraft-calculator` artifact.
- README, build guide, publishing notes, test target, generator headers, and
  screenshot labels were refreshed.
- The menu wordmark is generated from `tools/gen_gui_art.py`; the checked-in menu
  and help screenshots were refreshed from that same generated title/font data.

## Names deliberately retained

Not every occurrence of “Crafti” is a current product label. These are preserved
on purpose:

- Fabian Vogt's original authorship and upstream project credit, and all license
  notices.
- `nGL/README.md`, which belongs to the separate upstream nGL dependency.
- The original Crafti v1.3 physics baseline and save-format fixture generator.
- Legacy `crafti.map.tns` and `crafti.ppm.tns` paths, so existing user worlds and
  external atlases remain readable. Old saves stay at their original path; subsequent
  in-game saves write back to that same legacy file, as before.
- Cleanup rules for old `crafti*.tns` build outputs.
- The renamed Git bundle contains the unchanged historical upstream commit objects;
  changing embedded history would not be a branding-only rename.

## ClickPad source review

The code has separate classic and CX timer/display branches. The classic timer
register selection and 32768-Hz time base match the [Ndless timer binding](https://docs.rs/ndless/latest/src/ndless/bindings/timer.rs.html); the classic
memory map documents the timer control register's freeze and count-direction bits
([Hackspire classic memory map](https://www.hackspire.org/Memory-mapped_IO_ports_on_Classic/)).

The keypad code reads `0x900E0000 + key.row`. Ndless key definitions store `row` as
an already-offset value such as `0x10` or `0x12`, so this reaches the keypad data
register window beginning at `0x900E0010`; a cleared key bit is treated as pressed.

The biggest unverified area is display format. nGL's `COLOR` is RGB565, while the
classic path sends inverted 16-bit words as `SCR_320x240_16`; Ndless documents that
format as RGB444. The current code does not explicitly convert each rendered pixel
from RGB565 to RGB444. The classic mode detection, grayscale texture conversion,
and blit call compile at source level, but tonal correctness is not established.
This must be resolved with API-level verification and a real ClickPad display test
before calling the port complete.

The three core 320x240 16-bit buffers (render target, monochrome intermediate,
and depth buffer) use about 450 KiB combined, excluding texture atlases, world
chunks, stacks, and system memory. This is a size calculation, not a measured peak
memory figure.

## Validation performed for this source snapshot

- Host logic test suite: **178 test cases, 628,563 assertions, 0 failures**.
- `platform_time.cpp` and `main.cpp`: host compiler syntax checks passed with a
  temporary Ndless API stub. This checks C++ syntax only, not ABI, linking, or
  calculator behavior.
- GitHub Actions workflow YAML files parsed successfully.
- Menu/help screenshot regeneration script ran successfully.
- Ndless cross-compiler tools (`nspire-g++`, `nspire-ld`, `genzehn`, `make-prg`)
  were not available in this environment. No `.tns` calculator executable was built.
- No emulator or physical original ClickPad test was performed.

The user-provided OS and Boot2 images are reference material only; this project does
not modify, flash, or replace calculator firmware.
