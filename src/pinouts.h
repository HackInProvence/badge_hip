/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#ifndef _PINOUTS_H
#define _PINOUTS_H

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
//#define BADGE_GPIO21 21
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

/* We need defines that we use in the libraries, but they are unavailable on some platforms
 * TODO: disable the libs for these or better */
#if defined(BADGE_PROTO) || defined(BADGE_RADIO)
#define BADGE_UART0_RX 42
#define BADGE_BUZZER 42
#endif


#endif /* _PINOUTS_H */
