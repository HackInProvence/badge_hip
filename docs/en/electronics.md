# The electronics of the badge

*Version française : [electronique.md](../fr/electronique.md).*

This document describes the board of the badge (V1.1, KiCad project in [hardware/kicad](../../hardware/kicad), PDF
schematic of the V1.0 in [hardware](../../hardware)) **by function**: what each component is for. The references
(R24, U6...) are those of the schematic; the connections were read from the printed circuit file. The GPIOs are those
of [src/pinouts.h](../../src/pinouts.h).

```
 USB-C ──► power (TP4056 charger, USB / battery sharing, 3.3 V buck-boost) ──► the whole badge
                 │
 Battery ───────►┘──► measurement (divider + LM321) ──► ADC
                                     ┌─► e-Paper screen (SSD1681) ─┐
 RP2040 + 16 MB flash ── SPI0 ──► 74HC139 ┼─► micro-SD card            ├─ ports J2 / J3
                     ── SPI1 ──► CC1101 ─► filter ─► 433 MHz antenna
                     ── GPIO ──► 4 buttons, 2 WS2812B LEDs, buzzer, IR (J2), I2C (J3)
```


## 1. Power

### USB-C input (J1)

- **J1**: USB-C receptacle (USB 2.0): the 5 V supply (VBUS) and the data (the serial port of the badge, flashing).
- **R5, R6** (5.1 kΩ): pull CC1 and CC2 to ground: the badge shows itself as a powered device ("sink"), the charger
  gives it 5 V.
- **R1, R2** (27.4 Ω): series resistors of D+ and D− to the RP2040 (matching of the USB line, value of the RP2040
  hardware design guide).

### Battery charging (U5 TP4056)

- **H1**: connector of the Li-ion / LiPo battery (JST PH 2.0 mm, 2 pins).
- **U5 TP4056**: linear Li-ion charger (4.2 V). Powered by VBUS through **R20** (0.1 Ω), decoupled by **C37**
  (10 µF); its BAT output goes to the battery (**C38**, 10 µF).
- **R21** (1.2 kΩ) on PROG: sets the **charge current to ~1 A** (I = 1200 / R_PROG in kΩ, TP4056 datasheet). The
  battery must accept it (1,000 mAh or more advised).
- **D6** (red) with **R19** (2.4 kΩ) on CHRG: **charging**; **D7** (green) with **R27** on STDBY: **charged**.

### USB / battery sharing (D2, Q3)

The badge runs on USB or on battery, without stopping the charge:
- **D2** (MBR0530 Schottky): with USB, the 5 V powers the badge (the "system" line, VSYS) through it.
- **Q3** (SI2301, P MOSFET) between the battery and VSYS, its gate on VBUS: **without USB**, **R26** (100 kΩ) pulls the
  gate to ground, Q3 conducts and the battery powers the badge; **with USB**, the gate is at 5 V, Q3 is off: the
  battery only charges.
- **C39** (47 µF): reservoir of VSYS.

### 3.3 V regulator (U6 TPS63000) and switch

- **U6 TPS63000**: buck-boost converter: a stable 3.3 V whether the battery is at 4.2 V or 3 V (up to ~1 A at
  3.3 V). Inductor **L7** (2.2 µH), outputs **C40, C41** (47 µF), **C28** (4.7 µF).
- **R28** (1 MΩ) / **R29** (178 kΩ): feedback divider setting the output to 0.5 × (1 + 1000 / 178) ≈ **3.31 V**.
- **R30** (100 Ω) and **C43**: the PS/SYNC pin tied (filtered) to the input: according to the TPS63000 datasheet,
  the **power save mode is disabled** (fixed frequency: less ripple, but a bit more idle current; a lead for the
  battery life of a future board).
- **SW7** (slide switch) and **R31** (100 kΩ): R31 pulls EN to VSYS (on); on OFF, SW7 grounds EN: **the regulator
  is off, the badge is off, even plugged in USB**; charging goes on (the TP4056 is before the regulator).

### Battery measurement (U7 LM321)

- **R24** (100 kΩ) / **R25** (200 kΩ): divider: 2/3 of the battery voltage.
- **U7 LM321**: amplifier wired as a follower (its output on its − input), powered by the battery, to **ADC_VBAT**
  (GPIO29, ADC3 of the RP2040). Test points **TP8** (ADC_VBAT), **TP9** (3.3 V), **TP10** (ground).
- **Known limit**: the LM321 is not rail-to-rail: its output stays below ~V_bat − 1.5 V while its input is
  2/3 V_bat: it saturates, hence the software calibration (developer guide § 6.9). For a next board: a rail-to-rail
  amplifier (MCP6001...) or the divider straight on the ADC.


## 2. The microcontroller (U1 RP2040)

- **U1 RP2040**: two Cortex-M0+ cores at 133 MHz, 264 KB of RAM. Decoupling: **C1–C7, C12, C14, C15, C19, C20, C42,
  C44** (100 nF / 1 µF) near each power pin.
- **Internal 1.1 V regulator** of the core: **C13** (1 µF), **C8, C9** (100 nF) on VREG_VOUT.
- **X1** (12 MHz crystal, ABM8-272-T3), **C10, C11** (15 pF), **R16** (1 kΩ, limits the current in the crystal): the
  clock of the RP2040 and of the USB.
- **U3 W25Q128JV**: the 16 MB flash memory (QSPI): the firmware, the settings, the contacts, the announcements.
  **R4** (10 kΩ) pulls QSPI_SS to 3.3 V.
- **SW6 BOOTSEL** (with **R3**, 1 kΩ): grounds QSPI_SS at boot: the RP2040 starts in flashing mode ("RPI-RP2" drive).
- **SW1 RESET**: grounds RUN (restart); **R7** (100 kΩ) pulls RUN to 3.3 V.
- **SWD debugging**: test points **TP2** (SWDIO), **TP3** (SWCLK), pulled by **R22, R23** (10 kΩ); **TP1** (3.3 V),
  **TP4** (ground), **TP5** (RUN).


## 3. The shared SPI0 bus: screen, SD card, extensions (U2 74HC139)

The screen, the SD card and two slots of the extensions share the SPI0 bus (SCK GPIO6, MOSI GPIO7, MISO GPIO4).
- **U2 74HC139** (one of its two 2-to-4 decoders): its enable input is the hardware chip select of SPI0 (GPIO5); its
  two address inputs come from **GPIO18** and **GPIO19**. It selects one of four: **Y0 the screen**, **Y1 the SD
  card**, **Y2 and Y3 the port J2** (CS2, CS3). Three devices with one select pin.
- **R32, R33, R34** (4.7 kΩ): pull-ups of MOSI, SCK and MISO (the SD card needs them in SPI mode).


## 4. The e-Paper screen (J4)

- **J4**: 24-pin FPC connector of the **1.54" 200 × 200 e-Paper** screen (**SSD1681** controller), on SPI0 (CS by Y0
  of the decoder), with **DC** (GPIO8), **BUSY** (GPIO9), **RST** (GPIO10).
- **The power circuit of the screen** (the reference circuit of these screens): the e-Paper needs drive voltages of
  about ±20 V, made from the 3.3 V by a small boost converter that the SSD1681 drives itself:
  - **L6** (10 µH) and **Q2** (SI1308, N MOSFET) driven by the GDR pin of the screen;
  - **R18** (3 Ω): current sense of Q2 (RESE pin); **R17** (10 kΩ): keeps the gate of Q2 low at rest;
  - **D3, D4, D5** (MBR0530 Schottky) and **C27** (4.7 µF): rectifier and charge pump giving **PREVGH** (positive)
    and **PREVGL** (negative), stored in **C29** and **C35** (1 µF);
  - **C30, C32, C33, C34, C36** (1 µF): decoupling of the other internal voltages of the screen (VSH, VSL, VCOM,
    internal regulators).


## 5. The micro-SD card (J5)

- **J5**: micro-SD socket with card detection, on SPI0 (CS by Y1 of the decoder).
- **SD_DETECT** (GPIO21), pulled to 3.3 V by **R35** (100 kΩ): the card is there (missing on the V1.0).


## 6. The 433 MHz radio (U4 CC1101)

- **U4 CC1101**: sub-GHz transceiver (300–928 MHz, here 433 MHz) on **SPI1** (SCK GPIO26, MOSI GPIO27, MISO GPIO24,
  CS GPIO25), with its status outputs **GDO0** (GPIO22) and **GDO2** (GPIO23).
- **X2** (26 MHz crystal) and **C17, C18** (27 pF): the clock of the radio (the firmware measures the crystal at boot:
  some badges had a 27 MHz one).
- **R15** (56 kΩ) on RBIAS: the internal current reference (value set by TI); **C16** (100 nF) on DCOUPL: the internal
  regulator of the radio.
- **The balun and the filter** (values of the TI reference design for 433 MHz): the radio output is differential
  (RF_P, RF_N), the antenna single-ended:
  - **L1, L2** (27 nH), **C22, C23** (3.9 pF): the balun, from differential to single-ended, and the impedance
    matching;
  - **C21** (220 pF): decoupling of the middle point;
  - **L3** (22 nH), **C24** (8.2 pF), **L4** (27 nH), **C25** (5.6 pF): low-pass filter (removes the harmonics of
    the transmission);
  - **C26** (220 pF): coupling capacitor to the antenna.
- **L5 ACAG1204-433**: 12 × 4 mm SMD ceramic antenna for 433 MHz. Its range depends a lot on the ground plane and on
  the clearance around it: ~7 m measured between two V1.2 badges at 10 kbit/s (less on the V1.0, whose antenna is
  placed less well).


## 7. The buttons

Four push buttons between 3.3 V and a GPIO, **active high**, each pulled to ground by a 10 kΩ resistor:

| Button | Place | Component | GPIO | Resistor |
|---|---|---|---|---|
| **Y** | left side | SW3 (K2-1112SW, side) | 0 | R11 |
| **A** | left wing | SW4 (XDTD-6650) | 1 | R12 |
| **B** | right wing | SW5 (XDTD-6650) | 14 | R13 |
| **X** | right side | SW2 (K2-1112SW, side) | 15 | R10 |

From left to right on the board: **Y A B X**. SW1 (RESET) and SW6 (BOOTSEL) are in § 2.


## 8. The LEDs (LED1, LED2 WS2812B)

- **LED1, LED2 WS2812B-2020**: two addressable RGB LEDs (2 × 2 mm) in a chain: GPIO11 → **R14** (330 Ω, protects the
  data input) → LED1 → LED2. Driven by the PIO of the RP2040. Powered by 3.3 V.
- **TP6** (data output of LED2) and **TP7** (ground): to extend the chain with more LEDs.


## 9. The buzzer (BUZZER1)

- **BUZZER1 MLT-8530**: magnetic transducer (no oscillator): it plays the frequency it is given (the cicada, music,
  ringtones, PWM sampled sounds).
- **Q1** (2N7002, N MOSFET): low-side switch, driven by the PWM of **GPIO28** (PWM6A) through **R8** (1 kΩ); **R9**
  (100 kΩ) keeps Q1 off at rest.
- **D1** (1N4148): flyback diode across the buzzer (absorbs the surge of the coil at each cut).


## 10. The extension ports (J2, J3)

Two female 2 × 6 headers at 2.54 mm pitch.

| J3 (left) | | J2 (right) | |
|---|---|---|---|
| 1: +3.3 V | 2: +3.3 V | 1: +3.3 V | 2: GPIO20 (IR receive) |
| 3: I2C1 SCL (GPIO3) | 4: I2C1 SDA (GPIO2) | 3: I2C0 SCL (GPIO17, IR send) | 4: not connected |
| 5: SPI0 SCK | 6: SPI0 MISO | 5: I2C0 SDA (GPIO16) | 6: not connected |
| 7: screen RST | 8: screen BUSY | 7: UART0 RX (GPIO13) | 8: SPI0 CS2 (Y2 of the decoder) |
| 9: SPI0 MOSI | 10: screen DC | 9: UART0 TX (GPIO12) | 10: SPI0 CS3 (Y3 of the decoder) |
| 11: ground | 12: ground | 11: ground | 12: ground |

- **J3** carries the signals of the screen and I2C1: an I2C OLED screen (Badge > Écran OLED), another SPI screen.
- **J2**: the infrared board (GPIO20, GPIO17), UART0 (free: the console is on USB; planned for an ESP32 module, see
  [etude_module_esp32.md](../etude_module_esp32.md), in French), two SPI chip selects.


## 11. Leads for a next board (V1.3)

- Battery measurement: rail-to-rail amplifier or the divider straight on the ADC (§ 1).
- Antenna: ground plane and clearance around L5, or a spring antenna / a u.FL connector (§ 6).
- Regulator: try the power save mode of the TPS63000 (PS/SYNC to ground) for the battery life (§ 1).
- A GPIO to switch off the power of an extension module (ESP32), a SAO connector.
