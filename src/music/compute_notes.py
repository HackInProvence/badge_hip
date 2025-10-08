#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Just generate #defines to be used for notes in the melody library.
"""

if __name__ == '__main__':
    c0 = 440*2**(-4)*2**(-9/12)
    for octave in range(9):
        for semitone,note in enumerate('C CS D DS E F FS G GS A AS B'.split(' ')):
            name = note+str(octave)
            freq = c0*2**(octave+semitone/12)
            # The precision here will be rounded by the compiler, and we don't know about system's clock...
            formula = f'(TARGET_PWM_HZ/{freq:.4e})'
            formula += ' '*(22-len(formula))
            print(f'#define NOTE_{name:<5} {formula} /**< {note}_{octave}{" sharp" if len(note)>1 else ""} {freq:.02f} Hz */')
        print()

    #for octave in range(9):
    #    for note in 'C CS D DS E F FS G GS A AS B'.split(' '):
    #        name = note+str(octave)
    #        print(f'    {{NOTE_{name}, 1, 8}},')
