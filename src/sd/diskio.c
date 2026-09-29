/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* Glue between FatFs (read-only configuration, see fatfs/ffconf.h) and the SD card driver */

#include "ff.h"
#include "diskio.h"

#include "sd.h"


DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0)
        return STA_NOINIT;
    return sd_is_ready() ? 0 : STA_NOINIT;
}


DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0)
        return STA_NOINIT;
    return sd_init() ? 0 : STA_NOINIT;
}


DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || count == 0)
        return RES_PARERR;
    if (! sd_is_ready())
        return RES_NOTRDY;
    return sd_read_blocks(sector, buff, count) ? RES_OK : RES_ERROR;
}


DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    (void)buff;
    if (pdrv != 0)
        return RES_PARERR;
    /* Read-only: nothing to flush, and the sector count is only needed by f_mkfs */
    return cmd == CTRL_SYNC ? RES_OK : RES_PARERR;
}
