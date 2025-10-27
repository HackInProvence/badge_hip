/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */


#include "pico/binary_info.h"

#include "template.h"


void template_init(void) {
    /* GPIO usages */
    bi_decl_if_func_used(bi_1pin_with_name(BADGE_template, "template"));
}
