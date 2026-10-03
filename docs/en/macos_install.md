# Installing the build toolchain on macOS

Step by step to build and flash `badge_menu` on a Mac (Apple Silicon or Intel), and the pitfalls met while doing
it, with their fixes. Complements the "Building and flashing" section of the
[developer guide](developer_guide.md#2-building-and-flashing).

*Version française : [installation_macos.md](../fr/installation_macos.md).*

## 1. The tools

| Tool | Role | Installation |
|---|---|---|
| CMake, Ninja | drive the build | `brew install cmake ninja` |
| picotool (optional) | flash without the button, inspect a `.uf2` | `brew install picotool` |
| Arm compiler `arm-none-eabi-gcc` | compiles for the RP2040 | official Arm archive (§ 1.1) |
| Pico SDK **2.2 or later** | RP2040 functions | `git clone` (§ 1.2) |
| Python 3 + Pillow (+ pyserial) | images converted at build time; the tools of `tools/` | dedicated environment (§ 1.3) |

### 1.1 The official Arm compiler

Do not use `brew install arm-none-eabi-gcc`: it lacks the C library (`nosys.specs`), see § 6. The
`gcc-arm-embedded` cask works but its installer asks for the administrator password. The simplest is Arm's official
archive, unpacked in a folder:

```bash
cd ~/work
# Apple Silicon (M1, M2...): darwin-arm64; Intel Mac: darwin-x86_64
curl -fL -o arm-toolchain.tar.xz \
  "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.tar.xz"
tar -xf arm-toolchain.tar.xz
```

### 1.2 The Pico SDK

`src/badge_secsea.h` uses `pico_board_cmake_set`, which only exists from SDK **2.2.0** on:

```bash
cd ~/work
git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
git -C pico-sdk submodule update --init lib/tinyusb    # USB (the serial port of the badge)
```

### 1.3 Python and Pillow

The Python of macOS or Homebrew often refuses global `pip install`s. A dedicated environment, outside the
repository, leaves the system untouched:

```bash
python3 -m venv ~/work/badge-venv
~/work/badge-venv/bin/pip install pillow pyserial numpy    # numpy: video2epaper.py and its tests
```

## 2. Building

From the root of the repository:

```bash
export PICO_SDK_PATH=~/work/pico-sdk
export PICO_TOOLCHAIN_PATH=~/work/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi
cmake -S . -B build -G Ninja -DPICO_BOARD=badge_secsea \
      -DPICO_TOOLCHAIN_PATH=$PICO_TOOLCHAIN_PATH \
      -DPython3_EXECUTABLE=$HOME/work/badge-venv/bin/python3
cmake --build build --target badge_menu    # build/src/menu/badge_menu.uf2
```

The `build` folder remembers these settings: afterwards, `cmake --build build --target badge_menu` is enough,
without the `export`s.

## 3. Flashing

- **By hand** (the most reliable on macOS): hold BOOTLOADER while plugging in the badge, release after 2 s; the
  "RPI-RP2" drive shows up. Then:
  ```bash
  /bin/cp -X build/src/menu/badge_menu.uf2 /Volumes/RPI-RP2/
  ```
  The drive disappears when the badge reboots: the flash worked.
- **With picotool**, badge switched on: `picotool load -f -x build/src/menu/badge_menu.uf2`. On macOS it often
  fails after the reboot (§ 6); then `picotool reboot -f -u` puts the badge in flash mode without touching the
  button, and copy the `.uf2` as above.

## 4. The serial port

Several USB devices (keyboard, Flipper, camera...) also create `/dev/cu.usbmodem*` ports. The badge is the one with
the USB vendor id `2e8a` (Raspberry Pi):

```bash
~/work/badge-venv/bin/python -c "from serial.tools import list_ports; \
print([p.device for p in list_ports.comports() if p.vid == 0x2e8a])"
screen /dev/cu.usbmodem83201 115200    # the port found above
```

To quit `screen`: `Ctrl-A` then `\` (then `y`). The prefix is always `Ctrl-A`, released before the next key.

The badge only writes on the serial port when a terminal is open and raises the DTR signal (`screen` does). A
Python script must raise it itself: `serial.Serial(port, 115200, dsrdtr=True)` then `s.dtr = True`.

## 5. The tests on the PC

```bash
~/work/badge-venv/bin/python src/tests/host/run_tests.py --cc clang
```

`--cc clang` picks Apple's clang: a Homebrew or Nix `gcc` often fails on macOS (§ 6).

## 6. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `Error: bad instruction 'pico_board_cmake_set(...)'` | Pico SDK older than 2.2.0 | `git -C pico-sdk fetch --depth 1 origin tag 2.2.0 && git -C pico-sdk checkout 2.2.0`, then `rm -rf build` and configure again |
| `cannot read spec file 'nosys.specs'` | Homebrew's `arm-none-eabi-gcc`, without the C library | Arm's official compiler (§ 1.1) and `-DPICO_TOOLCHAIN_PATH` |
| `'/opt/homebrew/bin/ninja' '--version' failed` (or another tool not found) | the `build` folder remembered the path of a tool that moved (upgrade, switch from Homebrew to Nix...) | `rm -rf build`, then configure again (§ 2) |
| `No module named 'PIL'`, or an image conversion that fails | Pillow missing from the Python CMake found | § 1.3 and `-DPython3_EXECUTABLE` |
| `cp: could not copy extended attributes` when flashing | macOS tries to copy metadata the badge's drive cannot keep | harmless (the `.uf2` is copied); `-X` avoids it |
| `cp: invalid option -- 'X'` | the `cp` of the `PATH` is GNU's (Nix, Homebrew coreutils), without `-X` | `/bin/cp -X` (macOS's `cp`), or `cp` without option |
| `picotool load -f -x`: *no accessible RP-series devices in BOOTSEL mode* | macOS mounts the RPI-RP2 drive before picotool gets the device back | `picotool reboot -f -u`, then copy the `.uf2` (§ 3) |
| `Resource busy` when opening the serial port | another program holds it: a forgotten `screen`, a Chrome tab of a web flasher (WebSerial), qFlipper | `lsof /dev/cu.usbmodem83201` to find it, then close it |
| a Python script reads nothing on the serial port | DTR not raised: the badge thinks nobody listens | § 4 |
| PC tests: `error: tool 'dsymutil' not found` | `gcc` (Homebrew, Nix) calling Apple's `dsymutil` | `run_tests.py --cc clang` |
| the editor underlines `pico/stdlib.h` in red while the build works | it does not know the SDK paths | configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and give it `build/compile_commands.json` (clangd, VS Code C/C++ extension) |
