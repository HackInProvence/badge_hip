#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Try to calibrate the CC1101 crystal using the serial port.
The radio_calibrate program must be flashed on the device.
Requires pyserial.

Manual:
- flash the radio_calibrate.uf2 image on the pico/badge,
- reboot it but while connected in USB (ground 3V3_EN on the pico),
- wait for a reasonably long time (10s, 100s),
- start this script, which should connect to the pico with UART over USB,
- Ctrl-C as soon as the first status line appears
- "In 0.60050011s (0.60000200s on pico), rate is 25.996640818 MHz (25.997476167 on pico)"
-                                                                  ^^^^^^^^^^^^--- best estimate (by the pico)
- either change it in src/radio/radio.h:CC1101_fXOSC and recompile,
- or use the test_radio.dis image to locate radio_set_frequency:
  - the constant @<radio_set_frequency+0x30> is the CC1101_fXOSC,
  - this gives you the offset of the constant (26MHz == 0x018cba80, or 25.997640 == 0x018cb148) -> 0x10006824 for instance,
  - subtract 0x10000000 and you have the index in the .bin file, but not in the UF2 file,
  - using your favorite hexeditor, search for it in the uf2 file, it should be 4-bytes aligned and in little endian,
    and the function around it should be the same as in the .bin or in the .dis.
  - xxd src/tests/test_radio.uf2 | sed 's/80ba 8c01/48b1 8c01/' | xxd -r - src/tests/test_radio.uf2
                                                    ^^^^^^^^^---new value
                                          ^^^^^^^^^-------------old value

Conclusions:
- the pico time is accurate (at least it does not deviate from my laptop clock),
- when connecting to the serial to read the frequency, it slows down;
  this may be caused by the UART that may have a higher priority interrupt than our GPIO counter
  -> we should use core 1 to count! -> THIS WORKS BETTER, but it still drifts low when we connect with UART...
- without multi-core, we can read the most precise value by flashing the radio_calibrate.uf2, rebooting the pico,
  waiting (> 10s ?), then connecting with UART (once connected, even if disconnected, the value drifts) -> 29_997_640 Hz
- the python value oscillates because it depends on the serial module,
  but it is a mean over the whole time the script runs, so it should stabilize -> 29_997_100 Hz
"""

import time

import serial
import serial.tools.list_ports


def wait_open():
    """Wait for /dev/ttyACM0 to appear then connects to it"""
    print('waiting for device')
    while 'waiting':
        for p in serial.tools.list_ports.comports():
            if 'ttyACM0' in p.device:
                print('open', p.device)
                return serial.Serial(p.device, 115200, timeout=1)
        time.sleep(.1)


if __name__ == '__main__':
    # We could only keep the first and last data point, but this could be used to plot the evolution...
    data = []  # [[ts_host, ts_pico, ticks, rate_pico], ...]
    try:
        with wait_open() as ser:
            while 'data':
                # Receives ts,ticks,rate
                l = ser.readline()
                if not l:
                    print('nothing received, ended?')
                    break
                if b'GDO0' in l or b'started' in l:
                    continue
                ts = time.time()  # Our ts
                data.append([ts]+list(map(int,l.decode().split(','))))
                if len(data)>1:
                    first,*_,last = data
                    dt = last[0]-first[0]  # Out ts
                    dtp = (last[1]-first[1])/1e6  # Pico's ts
                    rate = (last[2]-first[2])/dt  *96  # 96 because we measured fXOSC/96
                    ratep = sum(d[3] for d in data)/len(data)
                    print(f'\x1b[2K\rIn {dt:.8f}s ({dtp:.8f}s on pico), rate is {rate/1e6:.9f} MHz ({ratep/1e6:.9f} on pico)', end='', flush=True)
    except serial.SerialException:
        print('\nconnection lost')
    except KeyboardInterrupt:
        print()
        print(data[-1])
