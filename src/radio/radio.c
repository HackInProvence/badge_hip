/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"

#include "radio.h"


void radio_init(void) {
    // Declare our GPIO usages
    bi_decl_if_func_used(bi_4pins_with_func(BADGE_SPI1_TX_MOSI_RADIO_SI, BADGE_SPI1_RX_MISO_RADIO_SO, BADGE_SPI1_SCK_RADIO, BADGE_SPI1_CSn_RADIO, GPIO_FUNC_SPI));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_RADIO_GDO0, "CC1101 GDO0"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_RADIO_GDO2, "CC1101 GDO2"));

    // Init SPI
    spi_init(spi1, 1000*1000);  /* Should go up to 10MHz */
    gpio_set_function(BADGE_SPI1_TX_MOSI_RADIO_SI, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI1_RX_MISO_RADIO_SO, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI1_SCK_RADIO, GPIO_FUNC_SPI);
    /* CSn with SPI module does not span multiple bytes, so we have to control it manually */
    gpio_set_function(BADGE_SPI1_CSn_RADIO, GPIO_FUNC_SIO);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Init other pins
    gpio_init(BADGE_SPI1_CSn_RADIO);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    gpio_set_dir(BADGE_SPI1_CSn_RADIO, GPIO_OUT);
    gpio_init(BADGE_RADIO_GDO0);
    gpio_init(BADGE_RADIO_GDO2);
}


/** \brief SPI read/write pulling CSn down for the whole transaction, \p response can be NULL
 *
 * We chose to block until the \p len bytes are written, as the communication is fast (~1MHz)
 *
 * TODO: static or not? */
void radio_send(const uint8_t *data, uint8_t *response, size_t len) {
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    if (response)
        spi_write_read_blocking(spi1, data, response, len);
    else
        spi_write_blocking(spi1, data, len);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}

/** \brief Helper to burst read registers
 * TODO: static or not ?*/
void radio_burst_read(uint8_t reg, uint8_t *response, size_t len) {
    uint8_t cmd = CC1101_BURST(CC1101_READ(reg));
    gpio_put(BADGE_SPI1_CSn_RADIO, 0);
    spi_write_blocking(spi1, &cmd, 1);
    spi_read_blocking(spi1, 0x00, response, len);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
}


void radio_set_frequency(uint32_t freq_hz) {
    /* setting = freq_hz * 2**16/fXOSC */
    uint64_t setting = freq_hz;
    setting <<= 16;
    setting /= CC1101_fXOSC;
    setting &= 0x003FFFFF;  /* Can only write the upper 22 bits, which gives 1.664GHz max */

    uint8_t cmd[4] = {
        CC1101_BURST(CC1101_FREQ2),
        (setting >> 16) & 0xFF, /* FREQ2 */
        (setting >> 8) & 0xFF,  /* FREQ1 */
         setting & 0xFF,        /* FREQ0 */
    };
    radio_send((uint8_t *)&cmd, NULL, 4);
}


