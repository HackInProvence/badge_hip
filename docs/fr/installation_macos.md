# Installer la chaîne de compilation sur macOS

Pas à pas pour compiler et flasher `badge_menu` sur un Mac (Apple Silicon ou Intel), et les pièges rencontrés en
le faisant, avec leurs solutions. Complète la section « Compiler et flasher » du
[guide développeur](guide_developpeur.md#2-compiler-et-flasher).

*English version: [macos_install.md](../en/macos_install.md).*

## 1. Les outils

| Outil | Rôle | Installation |
|---|---|---|
| CMake, Ninja | organisent la compilation | `brew install cmake ninja` |
| picotool (facultatif) | flasher sans bouton, inspecter un `.uf2` | `brew install picotool` |
| Compilateur Arm `arm-none-eabi-gcc` | compile pour le RP2040 | archive officielle d'Arm (§ 1.1) |
| Pico SDK **2.2 ou plus** | fonctions du RP2040 | `git clone` (§ 1.2) |
| Python 3 + Pillow (+ pyserial) | images converties à la compilation ; outils du dossier `tools/` | environnement dédié (§ 1.3) |

### 1.1 Le compilateur Arm officiel

Ne pas utiliser `brew install arm-none-eabi-gcc` : il n'a pas la bibliothèque C (`nosys.specs`), voir § 6. Le cask
`gcc-arm-embedded` convient mais son installeur demande le mot de passe administrateur. Le plus simple est l'archive
officielle d'Arm, décompressée dans un dossier :

```bash
cd ~/work
# Apple Silicon (M1, M2...) : darwin-arm64 ; Mac Intel : darwin-x86_64
curl -fL -o arm-toolchain.tar.xz \
  "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.tar.xz"
tar -xf arm-toolchain.tar.xz
```

### 1.2 Le Pico SDK

`src/badge_secsea.h` utilise `pico_board_cmake_set`, qui n'existe qu'à partir du SDK **2.2.0** :

```bash
cd ~/work
git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
git -C pico-sdk submodule update --init lib/tinyusb    # l'USB (port série du badge)
```

### 1.3 Python et Pillow

Le Python de macOS ou de Homebrew refuse souvent les `pip install` globaux. Un environnement dédié, hors du dépôt,
évite de toucher au système :

```bash
python3 -m venv ~/work/badge-venv
~/work/badge-venv/bin/pip install pillow pyserial numpy    # numpy : video2epaper.py et ses tests
```

## 2. Compiler

Depuis la racine du dépôt :

```bash
export PICO_SDK_PATH=~/work/pico-sdk
export PICO_TOOLCHAIN_PATH=~/work/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi
cmake -S . -B build -G Ninja -DPICO_BOARD=badge_secsea \
      -DPICO_TOOLCHAIN_PATH=$PICO_TOOLCHAIN_PATH \
      -DPython3_EXECUTABLE=$HOME/work/badge-venv/bin/python3
cmake --build build --target badge_menu    # build/src/menu/badge_menu.uf2
```

Le dossier `build` retient ces réglages : ensuite, `cmake --build build --target badge_menu` suffit, sans les
`export`.

## 3. Flasher

- **À la main** (le plus fiable sur macOS) : maintenir BOOTLOADER en branchant le badge, relâcher après 2 s ; le
  disque « RPI-RP2 » apparaît. Puis :
  ```bash
  /bin/cp -X build/src/menu/badge_menu.uf2 /Volumes/RPI-RP2/
  ```
  Le disque disparaît quand le badge redémarre : le flash a réussi.
- **Avec picotool**, badge allumé : `picotool load -f -x build/src/menu/badge_menu.uf2`. Sur macOS il échoue
  souvent après le redémarrage (§ 6) ; dans ce cas `picotool reboot -f -u` met le badge en mode flash sans toucher au
  bouton, puis copier le `.uf2` comme ci-dessus.

## 4. Le port série

Plusieurs appareils USB (clavier, Flipper, caméra...) créent aussi des `/dev/cu.usbmodem*`. Le badge est celui
dont l'identifiant USB est `2e8a` (Raspberry Pi) :

```bash
~/work/badge-venv/bin/python -c "from serial.tools import list_ports; \
print([p.device for p in list_ports.comports() if p.vid == 0x2e8a])"
screen /dev/cu.usbmodem83201 115200    # le port trouvé ci-dessus
```

Pour quitter `screen` : `Ctrl-A` puis `\` (puis `y`). Le préfixe est toujours `Ctrl-A`, relâché avant la touche
suivante.

Le badge n'écrit sur le port série que lorsqu'un terminal est ouvert et lève le signal DTR (`screen` le fait). Un
script Python doit le lever lui-même : `serial.Serial(port, 115200, dsrdtr=True)` puis `s.dtr = True`.

## 5. Les tests sur le PC

```bash
~/work/badge-venv/bin/python src/tests/host/run_tests.py --cc clang
```

`--cc clang` choisit le clang d'Apple : un `gcc` de Homebrew ou de Nix échoue souvent sur macOS (§ 6).

## 6. Dépannage

| Symptôme | Cause | Solution |
|---|---|---|
| `Error: bad instruction 'pico_board_cmake_set(...)'` | Pico SDK plus ancien que 2.2.0 | `git -C pico-sdk fetch --depth 1 origin tag 2.2.0 && git -C pico-sdk checkout 2.2.0`, puis `rm -rf build` et reconfigurer |
| `cannot read spec file 'nosys.specs'` | `arm-none-eabi-gcc` de Homebrew, sans bibliothèque C | le compilateur officiel d'Arm (§ 1.1) et `-DPICO_TOOLCHAIN_PATH` |
| `'/opt/homebrew/bin/ninja' '--version' failed` (ou un autre outil introuvable) | le dossier `build` a retenu le chemin d'un outil qui a bougé (mise à jour, passage de Homebrew à Nix...) | `rm -rf build`, puis reconfigurer (§ 2) |
| `No module named 'PIL'`, ou une conversion d'image qui échoue | Pillow absent du Python trouvé par CMake | § 1.3 et `-DPython3_EXECUTABLE` |
| `cp: could not copy extended attributes` en flashant | macOS essaie de copier des métadonnées que le disque du badge ne sait pas garder | sans gravité (le `.uf2` est copié) ; `-X` l'évite |
| `cp: invalid option -- 'X'` | le `cp` du `PATH` est celui de GNU (Nix, coreutils de Homebrew), sans `-X` | `/bin/cp -X` (le `cp` de macOS), ou `cp` sans option |
| `picotool load -f -x` : *no accessible RP-series devices in BOOTSEL mode* | macOS monte le disque RPI-RP2 avant que picotool ne reprenne la main | `picotool reboot -f -u`, puis copier le `.uf2` (§ 3) |
| `Resource busy` en ouvrant le port série | un autre programme le tient : `screen` oublié, onglet Chrome d'un flasheur web (WebSerial), qFlipper | `lsof /dev/cu.usbmodem83201` pour le trouver, puis le fermer |
| un script Python ne lit rien sur le port série | DTR non levé : le badge croit que personne n'écoute | § 4 |
| tests PC : `error: tool 'dsymutil' not found` | `gcc` (Homebrew, Nix) qui appelle le `dsymutil` d'Apple | `run_tests.py --cc clang` |
| l'éditeur souligne `pico/stdlib.h` en rouge alors que la compilation passe | il ne connaît pas les chemins du SDK | configurer avec `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` et lui donner `build/compile_commands.json` (clangd, extension C/C++ de VS Code) |
