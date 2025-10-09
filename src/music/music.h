/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

#ifndef _MUSIC_H
#define _MUSIC_H

typedef struct {
    uint32_t pitch; /**< One of the PITCH_ constant (pitch given as the half period of the note in ticks), 0 for silences */
    float duration; /**< Duration, relative to 1 beat */
} Note;

void music_init(void);

/* \brief Enable the music generation
 *
 * You can use the GPIO for other purposes when not \p enabled,
 * and this call will re-assign the GPIO to the PWM when \p enabled.
 *
 * As noise generation and music share the same buzzer, they should not be both enabled.
 *
 * To enable, there must be queued notes. */
bool music_set_enabled(bool enabled);

/* \brief Replace the current melody and queue this one instead.
 *
 * Enables the melody if disabled.
 *
 * \param notes (borrowed) Must be ended by a 0-note which cancels the melody
 * \param beat In beats per minutes */
void music_set_melody(const Note *notes, float beat);

bool music_is_playing(void);


/* ------ DEFINITIONS OF THE NOTES ------ */
/* Wraps length are constrained to uint16, so adjust the PWM clock to a frequency that helps us cover all notes.
 * The 901120Hz gives 2048 for a 440Hz, 55108 for C0, and 114 for B8 */
#define TARGET_PWM_HZ (901120)

#define SILENCE    (0)

#define NOTE_C0    (TARGET_PWM_HZ/1.6352e+01) /**< C_0 16.35 Hz */
#define NOTE_CS0   (TARGET_PWM_HZ/1.7324e+01) /**< CS_0 sharp 17.32 Hz */
#define NOTE_D0    (TARGET_PWM_HZ/1.8354e+01) /**< D_0 18.35 Hz */
#define NOTE_DS0   (TARGET_PWM_HZ/1.9445e+01) /**< DS_0 sharp 19.45 Hz */
#define NOTE_E0    (TARGET_PWM_HZ/2.0602e+01) /**< E_0 20.60 Hz */
#define NOTE_F0    (TARGET_PWM_HZ/2.1827e+01) /**< F_0 21.83 Hz */
#define NOTE_FS0   (TARGET_PWM_HZ/2.3125e+01) /**< FS_0 sharp 23.12 Hz */
#define NOTE_G0    (TARGET_PWM_HZ/2.4500e+01) /**< G_0 24.50 Hz */
#define NOTE_GS0   (TARGET_PWM_HZ/2.5957e+01) /**< GS_0 sharp 25.96 Hz */
#define NOTE_A0    (TARGET_PWM_HZ/2.7500e+01) /**< A_0 27.50 Hz */
#define NOTE_AS0   (TARGET_PWM_HZ/2.9135e+01) /**< AS_0 sharp 29.14 Hz */
#define NOTE_B0    (TARGET_PWM_HZ/3.0868e+01) /**< B_0 30.87 Hz */

#define NOTE_C1    (TARGET_PWM_HZ/3.2703e+01) /**< C_1 32.70 Hz */
#define NOTE_CS1   (TARGET_PWM_HZ/3.4648e+01) /**< CS_1 sharp 34.65 Hz */
#define NOTE_D1    (TARGET_PWM_HZ/3.6708e+01) /**< D_1 36.71 Hz */
#define NOTE_DS1   (TARGET_PWM_HZ/3.8891e+01) /**< DS_1 sharp 38.89 Hz */
#define NOTE_E1    (TARGET_PWM_HZ/4.1203e+01) /**< E_1 41.20 Hz */
#define NOTE_F1    (TARGET_PWM_HZ/4.3654e+01) /**< F_1 43.65 Hz */
#define NOTE_FS1   (TARGET_PWM_HZ/4.6249e+01) /**< FS_1 sharp 46.25 Hz */
#define NOTE_G1    (TARGET_PWM_HZ/4.8999e+01) /**< G_1 49.00 Hz */
#define NOTE_GS1   (TARGET_PWM_HZ/5.1913e+01) /**< GS_1 sharp 51.91 Hz */
#define NOTE_A1    (TARGET_PWM_HZ/5.5000e+01) /**< A_1 55.00 Hz */
#define NOTE_AS1   (TARGET_PWM_HZ/5.8270e+01) /**< AS_1 sharp 58.27 Hz */
#define NOTE_B1    (TARGET_PWM_HZ/6.1735e+01) /**< B_1 61.74 Hz */

#define NOTE_C2    (TARGET_PWM_HZ/6.5406e+01) /**< C_2 65.41 Hz */
#define NOTE_CS2   (TARGET_PWM_HZ/6.9296e+01) /**< CS_2 sharp 69.30 Hz */
#define NOTE_D2    (TARGET_PWM_HZ/7.3416e+01) /**< D_2 73.42 Hz */
#define NOTE_DS2   (TARGET_PWM_HZ/7.7782e+01) /**< DS_2 sharp 77.78 Hz */
#define NOTE_E2    (TARGET_PWM_HZ/8.2407e+01) /**< E_2 82.41 Hz */
#define NOTE_F2    (TARGET_PWM_HZ/8.7307e+01) /**< F_2 87.31 Hz */
#define NOTE_FS2   (TARGET_PWM_HZ/9.2499e+01) /**< FS_2 sharp 92.50 Hz */
#define NOTE_G2    (TARGET_PWM_HZ/9.7999e+01) /**< G_2 98.00 Hz */
#define NOTE_GS2   (TARGET_PWM_HZ/1.0383e+02) /**< GS_2 sharp 103.83 Hz */
#define NOTE_A2    (TARGET_PWM_HZ/1.1000e+02) /**< A_2 110.00 Hz */
#define NOTE_AS2   (TARGET_PWM_HZ/1.1654e+02) /**< AS_2 sharp 116.54 Hz */
#define NOTE_B2    (TARGET_PWM_HZ/1.2347e+02) /**< B_2 123.47 Hz */

#define NOTE_C3    (TARGET_PWM_HZ/1.3081e+02) /**< C_3 130.81 Hz */
#define NOTE_CS3   (TARGET_PWM_HZ/1.3859e+02) /**< CS_3 sharp 138.59 Hz */
#define NOTE_D3    (TARGET_PWM_HZ/1.4683e+02) /**< D_3 146.83 Hz */
#define NOTE_DS3   (TARGET_PWM_HZ/1.5556e+02) /**< DS_3 sharp 155.56 Hz */
#define NOTE_E3    (TARGET_PWM_HZ/1.6481e+02) /**< E_3 164.81 Hz */
#define NOTE_F3    (TARGET_PWM_HZ/1.7461e+02) /**< F_3 174.61 Hz */
#define NOTE_FS3   (TARGET_PWM_HZ/1.8500e+02) /**< FS_3 sharp 185.00 Hz */
#define NOTE_G3    (TARGET_PWM_HZ/1.9600e+02) /**< G_3 196.00 Hz */
#define NOTE_GS3   (TARGET_PWM_HZ/2.0765e+02) /**< GS_3 sharp 207.65 Hz */
#define NOTE_A3    (TARGET_PWM_HZ/2.2000e+02) /**< A_3 220.00 Hz */
#define NOTE_AS3   (TARGET_PWM_HZ/2.3308e+02) /**< AS_3 sharp 233.08 Hz */
#define NOTE_B3    (TARGET_PWM_HZ/2.4694e+02) /**< B_3 246.94 Hz */

#define NOTE_C4    (TARGET_PWM_HZ/2.6163e+02) /**< C_4 261.63 Hz */
#define NOTE_CS4   (TARGET_PWM_HZ/2.7718e+02) /**< CS_4 sharp 277.18 Hz */
#define NOTE_D4    (TARGET_PWM_HZ/2.9366e+02) /**< D_4 293.66 Hz */
#define NOTE_DS4   (TARGET_PWM_HZ/3.1113e+02) /**< DS_4 sharp 311.13 Hz */
#define NOTE_E4    (TARGET_PWM_HZ/3.2963e+02) /**< E_4 329.63 Hz */
#define NOTE_F4    (TARGET_PWM_HZ/3.4923e+02) /**< F_4 349.23 Hz */
#define NOTE_FS4   (TARGET_PWM_HZ/3.6999e+02) /**< FS_4 sharp 369.99 Hz */
#define NOTE_G4    (TARGET_PWM_HZ/3.9200e+02) /**< G_4 392.00 Hz */
#define NOTE_GS4   (TARGET_PWM_HZ/4.1530e+02) /**< GS_4 sharp 415.30 Hz */
#define NOTE_A4    (TARGET_PWM_HZ/4.4000e+02) /**< A_4 440.00 Hz */
#define NOTE_AS4   (TARGET_PWM_HZ/4.6616e+02) /**< AS_4 sharp 466.16 Hz */
#define NOTE_B4    (TARGET_PWM_HZ/4.9388e+02) /**< B_4 493.88 Hz */

#define NOTE_C5    (TARGET_PWM_HZ/5.2325e+02) /**< C_5 523.25 Hz */
#define NOTE_CS5   (TARGET_PWM_HZ/5.5437e+02) /**< CS_5 sharp 554.37 Hz */
#define NOTE_D5    (TARGET_PWM_HZ/5.8733e+02) /**< D_5 587.33 Hz */
#define NOTE_DS5   (TARGET_PWM_HZ/6.2225e+02) /**< DS_5 sharp 622.25 Hz */
#define NOTE_E5    (TARGET_PWM_HZ/6.5926e+02) /**< E_5 659.26 Hz */
#define NOTE_F5    (TARGET_PWM_HZ/6.9846e+02) /**< F_5 698.46 Hz */
#define NOTE_FS5   (TARGET_PWM_HZ/7.3999e+02) /**< FS_5 sharp 739.99 Hz */
#define NOTE_G5    (TARGET_PWM_HZ/7.8399e+02) /**< G_5 783.99 Hz */
#define NOTE_GS5   (TARGET_PWM_HZ/8.3061e+02) /**< GS_5 sharp 830.61 Hz */
#define NOTE_A5    (TARGET_PWM_HZ/8.8000e+02) /**< A_5 880.00 Hz */
#define NOTE_AS5   (TARGET_PWM_HZ/9.3233e+02) /**< AS_5 sharp 932.33 Hz */
#define NOTE_B5    (TARGET_PWM_HZ/9.8777e+02) /**< B_5 987.77 Hz */

#define NOTE_C6    (TARGET_PWM_HZ/1.0465e+03) /**< C_6 1046.50 Hz */
#define NOTE_CS6   (TARGET_PWM_HZ/1.1087e+03) /**< CS_6 sharp 1108.73 Hz */
#define NOTE_D6    (TARGET_PWM_HZ/1.1747e+03) /**< D_6 1174.66 Hz */
#define NOTE_DS6   (TARGET_PWM_HZ/1.2445e+03) /**< DS_6 sharp 1244.51 Hz */
#define NOTE_E6    (TARGET_PWM_HZ/1.3185e+03) /**< E_6 1318.51 Hz */
#define NOTE_F6    (TARGET_PWM_HZ/1.3969e+03) /**< F_6 1396.91 Hz */
#define NOTE_FS6   (TARGET_PWM_HZ/1.4800e+03) /**< FS_6 sharp 1479.98 Hz */
#define NOTE_G6    (TARGET_PWM_HZ/1.5680e+03) /**< G_6 1567.98 Hz */
#define NOTE_GS6   (TARGET_PWM_HZ/1.6612e+03) /**< GS_6 sharp 1661.22 Hz */
#define NOTE_A6    (TARGET_PWM_HZ/1.7600e+03) /**< A_6 1760.00 Hz */
#define NOTE_AS6   (TARGET_PWM_HZ/1.8647e+03) /**< AS_6 sharp 1864.66 Hz */
#define NOTE_B6    (TARGET_PWM_HZ/1.9755e+03) /**< B_6 1975.53 Hz */

#define NOTE_C7    (TARGET_PWM_HZ/2.0930e+03) /**< C_7 2093.00 Hz */
#define NOTE_CS7   (TARGET_PWM_HZ/2.2175e+03) /**< CS_7 sharp 2217.46 Hz */
#define NOTE_D7    (TARGET_PWM_HZ/2.3493e+03) /**< D_7 2349.32 Hz */
#define NOTE_DS7   (TARGET_PWM_HZ/2.4890e+03) /**< DS_7 sharp 2489.02 Hz */
#define NOTE_E7    (TARGET_PWM_HZ/2.6370e+03) /**< E_7 2637.02 Hz */
#define NOTE_F7    (TARGET_PWM_HZ/2.7938e+03) /**< F_7 2793.83 Hz */
#define NOTE_FS7   (TARGET_PWM_HZ/2.9600e+03) /**< FS_7 sharp 2959.96 Hz */
#define NOTE_G7    (TARGET_PWM_HZ/3.1360e+03) /**< G_7 3135.96 Hz */
#define NOTE_GS7   (TARGET_PWM_HZ/3.3224e+03) /**< GS_7 sharp 3322.44 Hz */
#define NOTE_A7    (TARGET_PWM_HZ/3.5200e+03) /**< A_7 3520.00 Hz */
#define NOTE_AS7   (TARGET_PWM_HZ/3.7293e+03) /**< AS_7 sharp 3729.31 Hz */
#define NOTE_B7    (TARGET_PWM_HZ/3.9511e+03) /**< B_7 3951.07 Hz */

#define NOTE_C8    (TARGET_PWM_HZ/4.1860e+03) /**< C_8 4186.01 Hz */
#define NOTE_CS8   (TARGET_PWM_HZ/4.4349e+03) /**< CS_8 sharp 4434.92 Hz */
#define NOTE_D8    (TARGET_PWM_HZ/4.6986e+03) /**< D_8 4698.64 Hz */
#define NOTE_DS8   (TARGET_PWM_HZ/4.9780e+03) /**< DS_8 sharp 4978.03 Hz */
#define NOTE_E8    (TARGET_PWM_HZ/5.2740e+03) /**< E_8 5274.04 Hz */
#define NOTE_F8    (TARGET_PWM_HZ/5.5877e+03) /**< F_8 5587.65 Hz */
#define NOTE_FS8   (TARGET_PWM_HZ/5.9199e+03) /**< FS_8 sharp 5919.91 Hz */
#define NOTE_G8    (TARGET_PWM_HZ/6.2719e+03) /**< G_8 6271.93 Hz */
#define NOTE_GS8   (TARGET_PWM_HZ/6.6449e+03) /**< GS_8 sharp 6644.88 Hz */
#define NOTE_A8    (TARGET_PWM_HZ/7.0400e+03) /**< A_8 7040.00 Hz */
#define NOTE_AS8   (TARGET_PWM_HZ/7.4586e+03) /**< AS_8 sharp 7458.62 Hz */
#define NOTE_B8    (TARGET_PWM_HZ/7.9021e+03) /**< B_8 7902.13 Hz */

#endif /* _MUSIC_H */
