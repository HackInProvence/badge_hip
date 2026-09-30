#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Exports the contact cards received by the badge (Social > Contacts) to a vCard file (.vcf), to import them in a phone
or an address book.

    python tools/contacts_export.py [--port COM9] [-o contacts.vcf]

The badge must run the menu application and its serial port must be free (close badge_remote.py first).
"""

import argparse
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit('This script requires pyserial (pip install pyserial)')


def find_port():
    for p in serial.tools.list_ports.comports():
        if p.vid == 0x2E8A:  # Raspberry Pi
            return p.device
    return None


def main():
    parser = argparse.ArgumentParser(description='Export the contacts of the badge as vCards')
    parser.add_argument('--port', help='serial port (default: the first Raspberry Pi Pico found)')
    parser.add_argument('-o', '--output', default='contacts.vcf')
    args = parser.parse_args()
    port = args.port or find_port()
    if not port:
        sys.exit('no badge found')
    with serial.Serial(port, 115200, timeout=0.2) as s:
        time.sleep(0.3)
        s.read(100000)
        s.write(b'k')
        text = ''
        end = time.time() + 3
        while time.time() < end:
            text += s.read(100000).decode('utf-8', 'replace')
    cards, current = [], None
    for line in text.splitlines():
        if line.startswith('BEGIN:VCARD'):
            current = [line]
        elif current is not None:
            current.append(line)
            if line.startswith('END:VCARD'):
                cards.append('\r\n'.join(current))
                current = None
    with open(args.output, 'w', encoding='utf-8', newline='') as f:
        f.write('\r\n'.join(cards) + ('\r\n' if cards else ''))
    print(f'{len(cards)} contact(s) written to {args.output}')


if __name__ == '__main__':
    main()
