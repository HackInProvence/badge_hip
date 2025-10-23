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
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_RADIO_GDO0, "CC1101"));
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_RADIO_GDO2, "CC1101"));

    // Init SPI
    spi_init(spi1, 1000*1000);  /* Should go up to 10MHz */
    gpio_set_function(BADGE_SPI1_TX_MOSI_RADIO_SI, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI1_RX_MISO_RADIO_SO, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI1_SCK_RADIO, GPIO_FUNC_SPI);
    gpio_set_function(BADGE_SPI1_CSn_RADIO, GPIO_FUNC_SIO); /* CSn with SPI module does not span multiple bytes */
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Init other pins
    gpio_init(BADGE_SPI1_CSn_RADIO);
    gpio_put(BADGE_SPI1_CSn_RADIO, 1);
    gpio_set_dir(BADGE_SPI1_CSn_RADIO, GPIO_OUT);
    gpio_init(BADGE_RADIO_GDO0);
    gpio_init(BADGE_RADIO_GDO2);
}


