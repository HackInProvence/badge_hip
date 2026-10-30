/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \file file_ops.h
 *
 * \brief Renaming and deleting a file of the SD card from the badge (long press on B in Médias: the image viewer,
 * the lists of files). A photo of the camera is two files: its .epi shown by the badge (IMAGES/PHOTOS/IMG_n.EPI)
 * and its JPEG (PHOTOS/IMG_n.JPG); both are renamed or deleted together.
 *
 * file_names.c: the names (no card, tested on a PC: src/tests/host/test_file_ops.c); file_ops.c: the card. */

#ifndef _FILE_OPS_H
#define _FILE_OPS_H

#include <stdbool.h>
#include <stddef.h>

/* The characters of a name typed on the badge: the ones a FAT name accepts (no \ / : * ? " < > |) */
extern const char FILE_NAME_CHARS[];

/** \brief Splits "DIR/NAME.EXT" into \p base "NAME" and \p ext ".EXT" ("" without an extension). */
void file_name_split(const char *path, char *base, size_t blen, char *ext, size_t elen);

/** \brief A name typed: removes the spaces and dots at both ends; false when it is empty or has a character not
 * in FILE_NAME_CHARS. */
bool file_name_clean(char *name);

/** \brief The other file of a photo of the camera: IMAGES/PHOTOS/X.EPI <-> PHOTOS/X.JPG (case insensitive).
 * \return false for any other file */
bool file_companion(const char *path, char *out, size_t len);

/** \brief "DIR/NAME.EXT" with another name: "DIR/<base>.EXT". */
void file_renamed_path(const char *path, const char *base, char *out, size_t len);

typedef enum { FILE_OK, FILE_BAD_NAME, FILE_EXISTS, FILE_ERROR } file_result_t;

/** \brief Renames \p path to \p base (its extension kept), and its companion; \p new_path receives the new path.
 * Nothing is renamed when a new name is taken. */
file_result_t file_rename(const char *path, const char *base, char *new_path, size_t len);

/** \brief Deletes \p path and its companion (when it is there). */
file_result_t file_delete(const char *path);

#endif /* _FILE_OPS_H */
