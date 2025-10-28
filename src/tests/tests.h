/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/** \brief This file changes the definition of STATIC so that tests can test static things */

#ifndef _TESTS_H
#define _TESTS_H


/* Redefine static to not be static */
#ifdef STATIC
#undef STATIC
#endif

#define STATIC


#endif  /* _TESTS_H */
