/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

// -----------------------------------------------------
// Should not be included manually but using -DPICO_BOARD=badge_secsea while configuring cmake
// Inspired from pico-sdk/src/boards/include/boards/pico_w.h
// Defines pins for our board, as well as mem size and so on...
// -----------------------------------------------------

#ifndef _BOARD_BADGE_H
#define _BOARD_BADGE_H

#include "badge_pinout.h"

pico_board_cmake_set(PICO_PLATFORM, rp2040)

// For board detection
//#define RASPBERRYPI_PICO_W

// --- UART ---
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN BADGE_UART0_TX
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN BADGE_UART0_RX
#endif
//#define PICO_DEFAULT_UART_BAUD_RATE xxx

// --- LED ---
// no PICO_DEFAULT_LED_PIN - no on/off LED
// no PICO_COLORED_STATUS_LED_USES_WRGB - we don't have WRGB led, only RGB

// The following enables status_led API from pico/status_led.h
//  (see status_led_init, colored_status_led_set_on_with_color, status_led_set_state)
#define PICO_DEFAULT_WS2812_PIN BADGE_LED

// --- I2C ---
#define I2
#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C 0
#endif
#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN BADGE_I2C0_SDA
#endif
#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN BADGE_I2C1_SDL
#endif

// --- SPI ---
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 1
#endif
#ifndef PICO_DEFAULT_SPI_SCK_PIN
#define PICO_DEFAULT_SPI_SCK_PIN BADGE_SPI1_SCK_RADIO
#endif
#ifndef PICO_DEFAULT_SPI_TX_PIN
#define PICO_DEFAULT_SPI_TX_PIN BADGE_SPI1_TX_MOSI_RADIO_SI
#endif
#ifndef PICO_DEFAULT_SPI_RX_PIN
#define PICO_DEFAULT_SPI_RX_PIN BADGE_SPI1_RX_MISO_RADIO_SO
#endif
#ifndef PICO_DEFAULT_SPI_CSN_PIN
#define PICO_DEFAULT_SPI_CSN_PIN BADGE_SPI1_CSn_RADIO
#endif

// --- FLASH ---

// TODO: Is this compatible with our W25Q128JVSIQ? The boot loader seems compatible with W25QxxJV chips
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
//#set(PICO_BOOT_STAGE2_FILE ${PICO_SDK_PATH}/src/rp2040/boot_stage2/boot2_generic_03h.S)

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (16 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif
// note the SMSP mode pin is on WL_GPIO1
// #define PICO_SMPS_MODE_PIN

// --- Platform ---
// TODO: Which revision do we have?
#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 0
#endif

#ifndef PICO_RP2040_B1_SUPPORTED
#define PICO_RP2040_B1_SUPPORTED 0
#endif

// The GPIO Pin used to monitor VSYS. Typically you would use this with ADC.
// There is an example in adc/read_vsys in pico-examples.
//#ifndef PICO_VSYS_PIN
//#define PICO_VSYS_PIN 29
//#endif

#endif /* _BOARD_BADGE_H */
