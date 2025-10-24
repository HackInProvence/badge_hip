#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Try to calibrate the CC1101 crystal using the serial port.
The radio_calibrate program must be flashed on the device.
Requires pyserial.

Conclusions:
- the pico time is accurate (at least it does not deviate from my laptop clock),
- when connecting to the serial to read the frequency, it slows down;
  this may be caused by the UART that may have a higher priority interrupt than our GPIO counter
  -> we should use core 1 to count!
- we can read the most precise value by flashing the radio_calibrate.uf2, rebooting the pico,
  waiting (> 10s ?), then connecting with UART (once connected, even if disconnected, the value drifts).
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
                if b'GDO0' in l:
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
