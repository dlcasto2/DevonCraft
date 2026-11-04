# Original TI-Nspire CAS ClickPad compatibility

## Target

- Calculator family: original monochrome TI-Nspire ClickPad / TouchPad
- OS target: 3.9.0.463 (the 3.9.0 release for the ClickPad family)
- Development API: Ndless
- CPU target: ARM926EJ-S
- Display: 320 x 240, native 4-bit grayscale

Ndless's supported-OS table lists OS 3.9.0.463 for ClickPad/TouchPad models. OS 3.9.1 is the separate CX/CX CAS line. See <https://github.com/ndless-nspire/Ndless>.

## Port work in this source tree

### Classic timer backend

`platform_time.cpp` now detects `is_classic` and uses the classic timer-0 register layout instead of the CX-only SP804 clock-source/gating setup:

- counter: `0x900C0000`
- divider: `0x900C0004`
- control: `0x900C0008`
- setup: divider `1`, control `0x0F`, counter `0`
- sampling: accumulate the counter value and clear it after each sample

This follows the classic-device timer strategy used by the Ndless timer implementation. It restores the timer registers captured at startup when the program exits. The CX path remains separate and unchanged.

Reference: <https://docs.rs/ndless/latest/src/ndless/bindings/timer.rs.html>

### Display and input paths already present in the supplied source

- nGL detects `SCR_320x240_4` and uses the classic monochrome LCD mode.
- The classic display path in nGL inverts the frame before blitting, matching the classic LCD's inverted grayscale convention.
- `terrain.cpp` converts the terrain atlas and derived terrain textures for the monochrome display.
- `task.cpp` reads the Nspire keypad matrix at `0x900E0000`; ClickPad keys are active-low and do not require the Touchpad path.

These were present in the supplied source/library; they were not newly invented as part of this patch.

## Build

The repository expects the Ndless r2020 toolchain. On Linux or WSL, follow `docs/building.md`:

```sh
sudo apt-get install -y libgmp-dev libmpfr-dev libmpc-dev libboost-program-options-dev
git clone --recursive --branch r2020 https://github.com/ndless-nspire/Ndless.git
(cd Ndless/ndless-sdk/toolchain && ./build_toolchain.sh)
export PATH="$PWD/Ndless/ndless-sdk/toolchain/install/bin:$PWD/Ndless/ndless-sdk/bin:$PATH"
make -C Ndless -j"$(nproc)"
make -j"$(nproc)"
```

The `nGL/` source directory must be populated with the matching nGL submodule source. If building from this archive, that dependency is included.

## Validation and limits

- Host logic tests: 178 test cases and 628,563 assertions passed in the supplied environment.
- The ClickPad timer source path passed a C++ syntax check against a minimal `libndls` declaration.
- A calculator `.tns` executable was **not** built here because the Ndless cross-toolchain (`nspire-gcc`, `nspire-g++`, `nspire-ld`, `genzehn`, and `make-prg`) is not installed, and network access to fetch it was unavailable.
- No physical ClickPad/emulator test was performed. Treat this as a source compatibility patch, not a hardware-certified release.
- Keep the original OS and Boot2 files unchanged. This project only builds an Ndless application; it does not modify calculator firmware.
