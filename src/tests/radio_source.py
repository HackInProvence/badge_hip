#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Send data to the radio source module to be aired.
"""


import argparse
import sys
import time

import serial
import serial.tools.list_ports


PRESETS = {
    # NOTE: DON'T PUSH 0x00..0x03
    # NOTE: use either VARIABLE or INFINITE packet length (see CC1101_PKTCTRL0.LENGTH_CONFIG)
    'gfsk': bytes([
        0x03, 0x47,  # CC1101_FIFOTHR: ADC retention, no RX attenuation, 33/32 TX/RX FIFO thresholds
        0x04, 0x46,  # CC1101_SYNC1: Sync word MSB
        0x05, 0x4C,  # CC1101_SYNC0: Sync work LSB
        #0x06, 0x00,  # CC1101_PKTLEN: The doc says that the value must be different from 0...
        0x08, 0x05,  # CC1101_PKTCTRL0: no whitening, use FIFOs, with CRC, variable packet length (first byte after sync word)
        0x09, 0x00,  # CC1101_ADDR: no packet filtration
        0x0B, 0x06,  # CC1101_FSCTRL1: IF frequency
        0x10, 0xC0,  # CC1101_MDMCFG4: Channel bandwidth: 203kHz
        #0x10, 0xC8,  # CC1101_MDMCFG4: Channel bandwidth: 203kHz
        #0x11, 0x93,  # CC1101_MDMCFG3: Data rate: 9.992kbps
        0x12, 0x12,  # CC1101_MDMCFG2: Modulation: GSK, no manchester, 16/16 sync word bits
        0x15, 0x34,  # CC1101_DEVIATN: Deviation = 19.04kHz
        0x18, 0x18,  # CC1101_MCSM0: Autocalibration on RX or TX, 64 ripples, no pin radio control
        0x19, 0x16,  # CC1101_FOCCFG: FOC: 3K, K/2 after sync word, limited to BW_chan/4
        0x1B, 0x43,  # CC1101_AGCCTRL2
        0x1C, 0x40,  # CC1101_AGCCTRL1: Relative carrier sense disabled, but absolute carrier sense
        0x1D, 0x91,  # CC1101_AGCCTRL0
        0x20, 0xFB,  # CC1101_WORCTRL: WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h
    ]),
    'fsk': bytes([
        0x03, 0x47,  # CC1101_FIFOTHR: ADC retention, no RX attenuation, 33/32 TX/RX FIFO thresholds
        0x04, 0x46,  # CC1101_SYNC1: Sync word MSB
        0x05, 0x4C,  # CC1101_SYNC0: Sync work LSB
        #0x06, 0x00,  # CC1101_PKTLEN: The doc says that the value must be different from 0...
        0x08, 0x05,  # CC1101_PKTCTRL0: no whitening, use FIFOs, with CRC, variable packet length (first byte after sync word)
        0x09, 0x00,  # CC1101_ADDR: no packet filtration
        0x0B, 0x06,  # CC1101_FSCTRL1: IF frequency
        0x10, 0xC0,  # CC1101_MDMCFG4: Channel bandwidth: 203kHz
        0x12, 0x02,  # CC1101_MDMCFG2: Modulation: 2-FSK, no manchester, 16/16 sync word bits
        0x13, 0x72,  # CC1101_MDMCFG1: 24 preamble bytes
        0x15, 0x34,  # CC1101_DEVIATN: Deviation = 19.04kHz
        0x18, 0x18,  # CC1101_MCSM0: Autocalibration on RX or TX, 64 ripples, no pin radio control
        0x19, 0x16,  # CC1101_FOCCFG: FOC: 3K, K/2 after sync word, limited to BW_chan/4
        0x1B, 0x43,  # CC1101_AGCCTRL2
        0x1C, 0x40,  # CC1101_AGCCTRL1: Relative carrier sense disabled, but absolute carrier sense
        0x1D, 0x91,  # CC1101_AGCCTRL0
        0x20, 0xFB,  # CC1101_WORCTRL: WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h
    ]),
    # Sends 24 preamble bytes + FF66 as sync then your packet (< 255)
    'fskrawFF66': bytes([
        0x03, 0x47,  # CC1101_FIFOTHR: ADC retention, no RX attenuation, 33/32 TX/RX FIFO thresholds
        0x04, 0xFF,  # CC1101_SYNC1: Sync word MSB
        0x05, 0x66,  # CC1101_SYNC0: Sync work LSB
        #0x06, 0x00,  # CC1101_PKTLEN: The doc says that the value must be different from 0...
        0x08, 0x00,  # CC1101_PKTCTRL0: no whitening, use FIFOs, no CRC, fixed packet length
        0x09, 0x00,  # CC1101_ADDR: no packet filtration
        0x0B, 0x06,  # CC1101_FSCTRL1: IF frequency
        0x10, 0xC0,  # CC1101_MDMCFG4: Channel bandwidth: 203kHz
        0x12, 0x02,  # CC1101_MDMCFG2: Modulation: 2-FSK, no manchester, 16/16 sync word bits
        0x13, 0x72,  # CC1101_MDMCFG1: 24 preamble bytes
        0x15, 0x34,  # CC1101_DEVIATN: Deviation = 19.04kHz
        0x18, 0x18,  # CC1101_MCSM0: Autocalibration on RX or TX, 64 ripples, no pin radio control
        0x19, 0x16,  # CC1101_FOCCFG: FOC: 3K, K/2 after sync word, limited to BW_chan/4
        0x1B, 0x43,  # CC1101_AGCCTRL2
        0x1C, 0x40,  # CC1101_AGCCTRL1: Relative carrier sense disabled, but absolute carrier sense
        0x1D, 0x91,  # CC1101_AGCCTRL0
        0x20, 0xFB,  # CC1101_WORCTRL: WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h
    ]),
    # Does not send preamble nor sync but you can do it yourself (length up to 64k)
    'fskrawer': bytes([
        0x03, 0x47,  # CC1101_FIFOTHR: ADC retention, no RX attenuation, 33/32 TX/RX FIFO thresholds
        #0x06, 0x00,  # CC1101_PKTLEN: The doc says that the value must be different from 0...
        0x08, 0x02,  # CC1101_PKTCTRL0: no whitening, use FIFOs, no CRC, infinite packet length
        0x09, 0x00,  # CC1101_ADDR: no packet filtration
        0x0B, 0x06,  # CC1101_FSCTRL1: IF frequency
        0x10, 0xC0,  # CC1101_MDMCFG4: Channel bandwidth: 203kHz
        0x12, 0x00,  # CC1101_MDMCFG2: Modulation: 2-FSK, no manchester, no preamble/sync
        #0x13, 0x02,  # CC1101_MDMCFG1: 0 preamble bytes
        0x15, 0x34,  # CC1101_DEVIATN: Deviation = 19.04kHz
        0x18, 0x18,  # CC1101_MCSM0: Autocalibration on RX or TX, 64 ripples, no pin radio control
        0x19, 0x16,  # CC1101_FOCCFG: FOC: 3K, K/2 after sync word, limited to BW_chan/4
        0x1B, 0x43,  # CC1101_AGCCTRL2
        0x1C, 0x40,  # CC1101_AGCCTRL1: Relative carrier sense disabled, but absolute carrier sense
        0x1D, 0x91,  # CC1101_AGCCTRL0
        0x20, 0xFB,  # CC1101_WORCTRL: WakeOnRadio: power down RC, 48 cycles for Event 1 (43ms), calibrate RC, maximum Event 0 timeout: 17h
    ]),
}


def wait_open(target):
    """Wait for /dev/ttyACM0 to appear then connects to it"""
    print('waiting for device on', target)
    while 'waiting':
        for p in serial.tools.list_ports.comports():
            if target == p.device:
                print('open', p.device)
                return serial.Serial(p.device, 115200, timeout=.1, write_timeout=1.)
        time.sleep(.1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Pass command and payloads to the radio source module (see radio_source.c). '+
                                                 'When possible, use -f and -b to set up baud rates, '+
                                                 'as they take into account the calibrated crystal frequency of the CC1101')
    parser.add_argument('--tty', '-t', default='/dev/ttyACM0', help='path to the (maybe not yet existing) path of the tty to which to send data')
    parser.add_argument('--preset', '-p ', default=None, choices=PRESETS.keys(), help='push a configuration beforehand')
    parser.add_argument('--frequency', '-f', default=None, type=int, help='push a frequency beforehand')
    parser.add_argument('--baud-rate', '-b', default=None, type=int, help='push a baud rate beforehand')
    parser.add_argument('--packet-size', '-s', default=-1, type=int,
                        help='split the input in payloads of this size (e.g. 60 max for the Flipper SubGHz Enhanced Chat App), '+
                             '-1 means "infinite length mode"')
    parser.add_argument('--variable-length', '-l', action='store_true',
                        help='you are using a preset that use variable length mode, so we push a placeholder byte to the radio to set the length')
    args = parser.parse_args()

    with wait_open(args.tty) as ser:
        print('waiting', ser.in_waiting, 'bytes to be read')
        if args.preset is not None:
            preset = PRESETS[args.preset]
            ser.write((len(preset)+1).to_bytes(2, 'little') +b'\xD0'+preset)
        if args.frequency is not None:
            ser.write(bytes([5, 0, 0xD1])+args.frequency.to_bytes(4, 'little'))
        if args.baud_rate is not None:
            ser.write(bytes([5, 0, 0xD3])+args.baud_rate.to_bytes(4, 'little'))

        buf = bytearray()
        max_size = 255 if args.variable_length else 65535-1
        while 'data':
            c = sys.stdin.buffer.read(1)  # Maybe we could read more at a time
            buf += c
            assert len(buf) <= max_size, 'cannot send packets bigger than 255 bytes in variable length mode and 64k-2 otherwise'
            if (not c and buf) or len(buf) == args.packet_size:  # Divide by packets of xx max
                # Handle variable packet length mode: the length is the first byte
                if args.variable_length:
                    payload = (
                        (len(buf)+2).to_bytes(2, 'little') +  # Length of payload: our buffer + 1 command + 1 prefix for variable length mode
                        b'\xD2' +  # Command
                        b'\xff'  # Placeholder, overwritten by RaSo, length of the packet
                    )
                else:
                    payload = (len(buf)+1).to_bytes(2, 'little') + b'\xD2'
                payload += bytes(buf)
                print('Sending', payload.hex())
                ser.write(payload)
                buf.clear()
            if not c:  # We reached EOF
                break
