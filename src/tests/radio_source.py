#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Send data to the radio source module to be aired.
"""


import argparse
import time

import serial
import serial.tools.list_ports


def wait_open(target):
    """Wait for /dev/ttyACM0 to appear then connects to it"""
    print('waiting for device on', target)
    while 'waiting':
        for p in serial.tools.list_ports.comports():
            if target == p.device:
                print('open', p.device)
                return serial.Serial(p.device, 115200, timeout=.1)
        time.sleep(.1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Pass command and payloads to the radio source module (see radio_source.c)')
    parser.add_argument('--tty', '-t', default='/dev/ttyACM0', help='path to the (maybe not yet existing) path of the tty to which to send data')
    args = parser.parse_args()

    # Just send and receive the hex version
    try:
        with wait_open(args.tty) as ser:
            i = 0
            while 'data':
                # This prints out the serial content but reset DTR so we can't show these data on another editor.
                # In the end, we want to keep stdout of the RaSo longer than this program runs (this program will open/close multiple times)
                #c = ser.read()
                c = b''
                time.sleep(.1)
                if c:
                    print(c.decode(), end='', flush=True)
                else:
                    if i%128 == 0:
                        print()
                    ser.write(bytes([i%256]))
                    i += 1
    except serial.SerialException:
        print('\nconnection lost')
    except KeyboardInterrupt:
        print('exit')
