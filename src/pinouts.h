/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#ifndef _PINOUTS_H
#define _PINOUTS_H

#if ! defined(BADGE_SECSEA) && ! defined(BADGE_PROTO) && ! defined(BADGE_RADIO)
#error "Not a board of the badge: configure with cmake -DPICO_BOARD=badge_secsea (or delete the CMakeCache.txt)"
#endif

// Our board and prototypes pinouts
#define BADGE_BUTTON_Y 0
#define BADGE_BUTTON_A 1
#define BADGE_I2C1_SDA 2
#define BADGE_I2C1_SCL 3
#define BADGE_SPI0_RX_MISO 4
#define BADGE_SPI0_CSn 5
#define BADGE_SPI0_SCK_SCREEN 6
#define BADGE_SPI0_TX_MOSI_SCREEN 7
#define BADGE_SCREEN_DC 8
#define BADGE_SCREEN_BUSY 9
#define BADGE_SCREEN_RST 10
#define BADGE_LED 11
#define BADGE_UART0_TX 12
#if defined(BADGE_SECSEA)
#define BADGE_UART0_RX 13
// Redefine pins for prototypes because some pins are not exposed on Pico/Pico W (23 for GD0 and 24,25 for SPI1)
#elif defined(BADGE_PROTO) || defined(BADGE_RADIO)
#define BADGE_SPI1_CSn_RADIO 13
#endif
#define BADGE_BUTTON_B 14
#define BADGE_BUTTON_X 15
#define BADGE_I2C0_SDA 16
#define BADGE_I2C0_SCL 17
#define BADGE_SPI0_CS_A0 18
#define BADGE_SPI0_CS_A1 19
//#define BADGE_GPIO20 20
#if defined(BADGE_SECSEA)
#define BADGE_SD_DETECT 21  /* Micro SD card detect (board V1.1, not connected on V1.0) */
#endif
#if defined(BADGE_PROTO) || defined(BADGE_RADIO)
#define BADGE_RADIO_GDO2 21
#endif
#define BADGE_RADIO_GDO0 22
#if defined(BADGE_SECSEA)
#define BADGE_RADIO_GDO2 23
#define BADGE_SPI1_RX_MISO_RADIO_SO 24
#define BADGE_SPI1_CSn_RADIO 25
#endif
#define BADGE_SPI1_SCK_RADIO 26
#define BADGE_SPI1_TX_MOSI_RADIO_SI 27
#if defined(BADGE_SECSEA)
#define BADGE_BUZZER 28
#elif defined(BADGE_PROTO) || defined(BADGE_RADIO)
#define BADGE_SPI1_RX_MISO_RADIO_SO 28
#endif
#define BADGE_VBAT 29

/* Extension modules (see docs/idees_reseau_extensions_ctf.md), badge seen from the front:
 * - right port J2: infrared receiver (38kHz, e.g. VS1838B/TSOP38238, active low output) and emitter (IR LED + transistor),
 * - left port J3: OLED screen SSD1306 128x64 on I2C1 (address 0x3C or 0x3D). */
#define BADGE_IR_RX 20  /* J2 pin 2 */
#define BADGE_IR_TX BADGE_I2C0_SCL  /* J2 pin 3 (GPIO17) */
#define BADGE_OLED_I2C 1
#define BADGE_OLED_SDA BADGE_I2C1_SDA  /* J3 pin 4 (GPIO2) */
#define BADGE_OLED_SCL BADGE_I2C1_SCL  /* J3 pin 3 (GPIO3) */

/* SPI0 devices, selected by the 74HC139 decoder: address = A1*2 + A0, enabled by SPI0_CSn low */
#define BADGE_SPI0_ADDR_SCREEN 0
#define BADGE_SPI0_ADDR_SD 1  /* Micro SD card (board V1.1) */

/* We need defines that we use in the libraries, but they are unavailable on some platforms
 * TODO: disable the libs for these or better */
#if defined(BADGE_PROTO) || defined(BADGE_RADIO)
#define BADGE_UART0_RX 42
#define BADGE_BUZZER 42
#endif


#endif /* _PINOUTS_H */
