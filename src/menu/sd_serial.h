/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file sd_serial.h
 *
 * \brief The SD card through the USB serial port: list, read, write, delete... (protocol in sd_serial.c, PC side:
 * tools/sd_files.py). */

#ifndef _SD_SERIAL_H
#define _SD_SERIAL_H

#define SD_SERIAL_CHUNK 512  /* Bytes read or written by a command at most */

/** \brief After the byte 0x05 on the serial port: reads the command line, runs it, answers "@SD ..." lines. */
void sd_serial_command(void);

#endif /* _SD_SERIAL_H */
