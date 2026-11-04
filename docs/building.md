# Building DevonCraft

## Calculator build with GitHub Actions

Push this repository to your own DevonCraft fork. The **Calculator build** workflow
builds the Ndless r2020 toolchain (the first run can take a while, then it is cached)
and uploads an artifact named `devoncraft-calculator` containing `devoncraft.tns`
and `devoncraft-stress.tns`.

## Calculator build on Linux or WSL

```bash
sudo apt-get install -y libgmp-dev libmpfr-dev libmpc-dev libboost-program-options-dev
git clone --recursive --branch r2020 https://github.com/ndless-nspire/Ndless.git
(cd Ndless/ndless-sdk/toolchain && ./build_toolchain.sh)
export PATH="$PWD/Ndless/ndless-sdk/toolchain/install/bin:$PWD/Ndless/ndless-sdk/bin:$PATH"
make -C Ndless -j"$(nproc)"
make -j"$(nproc)"                              # devoncraft.tns
make clean-objs && make STRESS=1 -j"$(nproc)"  # devoncraft-stress.tns
```

The top-level `Makefile` targets ARM with `-mcpu=arm926ej-s`, as required by the
original ClickPad. The runtime selects the classic ClickPad or color CX timer/display
path. The cross-toolchain is not bundled in this source archive.

## PC build and tests

```bash
sudo apt-get install -y g++ make libsdl1.2-dev zlib1g-dev
make -C tests test
make -f Makefile.pc -j"$(nproc)"               # devoncraft.elf
SDL_VIDEODRIVER=dummy DEVONCRAFT_MAX_FRAMES=200 ./devoncraft.elf /tmp/smoke.map.tns
```
