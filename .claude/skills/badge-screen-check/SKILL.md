---
name: badge-screen-check
description: Check that every choice of a SecSea badge screen (app page, settings list, admin page, menu) can be reached and does what it says, then test it on a badge. Use whenever a screen of src/menu is added or changed (new rows, new actions, a new app), and when the user reports that a choice of a screen cannot be selected, saved or reached.
---

# Every choice of a screen reachable, and tested

A screen is done only when **each** of its choices can be selected with the buttons and does what its text says,
checked on a badge. Example of what this prevents: Admin > Batterie (calibration) had a single "Enregistrer le
point" row and the firmware chose the slot itself; with the ADC almost flat, the 2nd point always replaced the 1st:
"Enregistrer le point 2" could never be reached. Fixed with one row per point, chosen by the user.

## Read the code of the screen (src/menu/<app>.c)
- List the choices: the rows (enum ROW_..., N_ROWS, a table), the actions of each wing on each row, the long presses.
- Navigation: the flanks (UI_BTN_X down, UI_BTN_Y up) go through **all** the rows, with wrap-around when the other
  lists do; no row skipped by a condition that is never true; a list longer than UI_VISIBLE_ROWS (7) scrolls.
- Each action has its own effect: no choice made by the firmware in the user's place when the user can tell
  (a slot, a point, a target), no choice silently replaced by another one, no state where an action does nothing
  without a message. When an action is refused, the footer or the status says why.
- Every way out: the left wing (G) goes back from each row (except the rows where the wings change a value: then a
  long press of G, as written in the footer).
- Drawing: every row above the footer (UI_FOOTER_Y 180) and below the title (UI_TITLE_H 28), the selected row
  visible, the footer text matching the row (`G : retour  D : valider`, `Ailes : -  +`...).
- An app_t: `.start`, `.buttons` and `.render` are called without a NULL check by main.c: always define them (a
  missing `.start` froze the badge with its USB port stuck).
- Each action writes a log line (`printf`) the tests can wait for (`battery: point 2 set ...`).

## Test it on a badge
Check first that the serial ports are free (the user uses the badges too), then flash
(`picotool load --ser <SER> -f -x build/src/menu/badge_menu.uf2`).
- Go through every row and run every action, waiting for its log line: add or update the group of
  `tools/badge_selftest.py` (`Tester.keys()`, `expect()`, `screenshot()`), run it with
  `python tools/badge_selftest.py --port COMx --only <group>`; a scratch script in the scratchpad for a one-off check.
- Screenshot the page (the selftest `screenshot()`, or `tools/badge_screens.py`) and look at it: rows, selected row,
  footer, no text cut; the `U` key logs the texts too wide (`uicheck:`). In the other languages too
  (`badge_screens.py --lang en`).
- Leave the badge as it was: values changed by the test put back (calibration points cleared only on a badge that
  was not calibrated), admin mode off, **mute mode back on** (the tests and the radio commands can unmute it).
- Report what was tested on which badge; what could not be tested, say it.
