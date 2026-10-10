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
- start this script, which should connect to the pico with UART over USB,
- "In  1312.811018s ( 1312.804376s on pico), rate is  9.999867 kHz (9.999917 on pico), xosc is 25.999653 MHz ([25.999785, 25.999787] on pico)"
                                                                                                               ^^^^^^^^^--^^^^^^^^^ bounds using pico's clock
                                                                                               ^^^^^^^^^--------------------------- value using python's clock
                                                      ^^^^^^^^------^^^^^^^^------------------------------------------------------- diff btw pico and host clks
- wait long enough so that bounds converge

Conclusions:
- the RP2040's crystal is not necessarily more accurate than the CC1101's,

Notes on previous method to calibrate:
- when connecting to the serial to read the frequency, it slowed down, meaning that some interrupts were missed
  this may be caused by the UART that may have a higher priority interrupt than our GPIO counter
  -> we should use core 1 to count! -> THIS WORKS BETTER, but it still drifts low when we connect with UART...
  -> this may also be caused by USB CDC handler which sets up a task according to pico_stdio_usb...
  -> even with interrupt in RAM, we missed counts
  -> interrupt less is a better approach, so use PWM.
- the python value oscillates because it depends on the serial module,
  but it is a mean over the whole time the script runs, so it should stabilize -> 29_997_100 Hz

New elements:
- this is still not a very good calibration, needs more work,
- the actual frequency is even higher than the one given by the pico,
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
                # Skip logs
                l = l.strip()
                if not l or b'[INFO]' in l:
                    continue
                ts = time.time()  # Our ts
                data.append([ts]+list(map(int,l.decode().split(','))))
                if len(data)>1:
                    first,*_,last = data
                    dt = last[0]-first[0]  # Using our ts
                    dtp = (last[1]-first[1])/1e6  # Using Pico's ts
                    rate = (last[2]-first[2])/dt  # IRQ rate, with our ts
                    #ratep = sum(d[3] for d in data)/len(data)  # Pico's mean rate
                    ratep = (last[2]-first[2])/dtp  # Pico's mean rate
                    xosc = 2600 * rate  # With current config, the clock is 26k * the IRQ on the pico
                    #xoscp = sum(d[4] for d in data)/len(data)  # Pico's mean xosc
                    xoscp = 2600 * ratep  # Pico's mean xosc
                    xoscp_ = 2600 * (last[2]-first[2]+1)/dtp  # Pico's mean xosc upper bound
                    print(f'\x1b[2K\rIn {dt:12.6f}s ({dtp:12.6f}s on pico), rate is {rate/1e3:9.6f} kHz ({ratep/1e3:.6f} on pico), ' +
                          f'xosc is {xosc/1e6:.6f} MHz ([{xoscp/1e6:.6f}, {xoscp_/1e6:.6f}] on pico)',
                          end='', flush=True)
    except serial.SerialException:
        print('\nconnection lost')
    except KeyboardInterrupt:
        print()
        print(data[-1])
