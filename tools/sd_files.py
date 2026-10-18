#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""File manager of the SD card of the badge, through its USB serial port (no need to take the card out).

    python tools/sd_files.py ls [DIR]                    list a folder (default: the root)
    python tools/sd_files.py tree [DIR]                  every file below a folder
    python tools/sd_files.py get CARD_PATH [LOCAL]       copy a file (or a folder, -r) to the PC
    python tools/sd_files.py put LOCAL [CARD_PATH]       copy a file (or a folder, recursively) to the card
    python tools/sd_files.py rm CARD_PATH                delete a file (or a folder and its content, -r)
    python tools/sd_files.py mkdir CARD_PATH             make a folder (and its parents)
    python tools/sd_files.py mv OLD NEW                  rename or move
    python tools/sd_files.py df                          free space
    python tools/sd_files.py shell                       interactive: the same commands, plus cd

Examples:
    python tools/sd_files.py put docs/sd/CONFERENCES CONFERENCES
    python tools/sd_files.py --port COM9 get IMAGES/chat.epi

Protocol (src/menu/sd_serial.c): the byte 0x05, then a command line; the badge answers "@SD OK ..." or
"@SD ERR <code> <text>"; the data goes in base64, 512 bytes per command. The other lines are the log of the badge.
Close the other applications that use the serial port (badge_remote.py, a terminal...) first.
"""

import argparse
import base64
import os
import posixpath
import shlex
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit('This program requires pyserial: pip install pyserial')

PICO_VID = 0x2E8A
CHUNK = 512  # SD_SERIAL_CHUNK of sd_serial.h


class SdError(Exception):
    pass


def find_port():
    for p in serial.tools.list_ports.comports():
        if p.vid == PICO_VID:
            return p.device
    return None


class Card:
    def __init__(self, port, verbose=False):
        self.ser = serial.Serial(port, 115200, timeout=0.2)
        self.verbose = verbose
        self.buf = b''
        time.sleep(0.2)
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def _line(self, deadline):
        while b'\n' not in self.buf:
            if time.time() > deadline:
                raise SdError('no answer from the badge (is the menu firmware running?)')
            self.buf += self.ser.read(self.ser.in_waiting or 1)
        line, self.buf = self.buf.split(b'\n', 1)
        return line.decode('utf-8', 'replace').rstrip('\r')

    def command(self, text, timeout=10.0):
        """Sends a command, returns the lines "@SD E ..." and the final "@SD OK ..." (without "@SD ")."""
        self.ser.write(b'\x05' + text.encode('utf-8') + b'\n')
        entries = []
        deadline = time.time() + timeout
        while True:
            line = self._line(deadline)
            if not line.startswith('@SD '):
                if self.verbose:
                    print('  badge: ' + line)
                continue
            body = line[4:]
            if body.startswith('E '):
                entries.append(body[2:])
            elif body.startswith('OK'):
                return entries, body[3:]
            elif body.startswith('ERR'):
                raise SdError(f'{text.split(" ")[0]} {text.split(" ", 1)[-1][-60:]}: {body[4:]}')

    # ------ The operations

    def ls(self, path):
        """[(name, is_dir, size)]"""
        entries, _ = self.command('L ' + path)
        out = []
        for e in entries:
            kind, size, name = e.split(' ', 2)
            out.append((name, kind == 'd', int(size)))
        return sorted(out, key=lambda x: (not x[1], x[0].lower()))

    def stat(self, path):
        """(is_dir, size), None when it does not exist"""
        try:
            _, r = self.command('S ' + path)
        except SdError as e:
            if 'no file' in str(e) or 'no path' in str(e):
                return None
            raise
        kind, size = r.split()
        return kind == 'd', int(size)

    def read(self, path, progress=None):
        is_dir, size = self.stat(path) or (None, None)
        if size is None or is_dir:
            raise SdError(f'{path}: not a file')
        data = bytearray()
        while len(data) < size:
            _, r = self.command(f'R {len(data)} {CHUNK} {path}')
            n, _, b64 = r.partition(' ')
            chunk = base64.b64decode(b64) if b64 else b''
            if int(n) != len(chunk) or not chunk:
                raise SdError(f'{path}: read error at {len(data)}')
            data += chunk
            if progress:
                progress(len(data), size)
        return bytes(data)

    def write(self, path, data, progress=None):
        offset = 0
        while True:
            chunk = data[offset:offset + CHUNK]
            _, r = self.command(f'W {offset} {base64.b64encode(chunk).decode()} {path}')
            if int(r) != len(chunk):
                raise SdError(f'{path}: write error at {offset}')
            offset += len(chunk)
            if progress:
                progress(offset, len(data))
            if offset >= len(data):
                break
        self.command('C')

    def mkdir(self, path):
        parts = [p for p in path.strip('/').split('/') if p]
        for i in range(len(parts)):
            sub = '/'.join(parts[:i + 1])
            st = self.stat(sub)
            if st is None:
                self.command('M ' + sub)
            elif not st[0]:
                raise SdError(f'{sub}: a file, not a folder')

    def rm(self, path, recursive=False):
        st = self.stat(path)
        if st is None:
            raise SdError(f'{path}: not found')
        if st[0] and recursive:
            for name, _, _ in self.ls(path):
                self.rm(posixpath.join(path, name), True)
        self.command('D ' + path)

    def mv(self, old, new):
        self.command(f'N {old}\t{new}')

    def df(self):
        _, r = self.command('F', timeout=60)  # Counting the free clusters of a big card takes a while
        free, total = (int(x) for x in r.split())
        return free, total


def human(n):
    for unit in ('B', 'KB', 'MB', 'GB'):
        if n < 1024 or unit == 'GB':
            return f'{n} {unit}' if unit == 'B' else f'{n:.1f} {unit}'
        n /= 1024


def progress_bar(name):
    start = time.time()

    last = [0.0]

    def show(done, total):
        if done < total and time.time() - last[0] < 0.5:
            return
        last[0] = time.time()
        rate = done / max(time.time() - start, 1e-3) / 1024
        print(f'\r  {name}: {done}/{total} bytes, {rate:.0f} KB/s   ', end='' if done < total else '\n', flush=True)
    return show


def norm(path, cwd='/'):
    path = posixpath.normpath(posixpath.join(cwd, path)) if path else cwd
    return path.lstrip('/')


# ------ The commands

def do_ls(card, args, cwd):
    path = norm(args[0] if args else '', cwd)
    for name, is_dir, size in card.ls(path):
        print(f'  {name}/' if is_dir else f'  {name:<40} {human(size):>10}')


def do_tree(card, args, cwd):
    def walk(path, indent):
        for name, is_dir, size in card.ls(path):
            print(f'{indent}{name}/' if is_dir else f'{indent}{name}  ({human(size)})')
            if is_dir:
                walk(posixpath.join(path, name), indent + '  ')
    walk(norm(args[0] if args else '', cwd), '  ')


def do_get(card, args, cwd):
    src = norm(args[0], cwd)
    dst = args[1] if len(args) > 1 else posixpath.basename(src) or '.'
    st = card.stat(src)
    if st is None:
        raise SdError(f'{src}: not found')
    if st[0]:
        os.makedirs(dst, exist_ok=True)
        for name, is_dir, _ in card.ls(src):
            do_get(card, [posixpath.join('/', src, name), os.path.join(dst, name)], '/')
        return
    if os.path.isdir(dst):
        dst = os.path.join(dst, posixpath.basename(src))
    data = card.read(src, progress_bar(src))
    with open(dst, 'wb') as f:
        f.write(data)


def do_put(card, args, cwd):
    src = args[0]
    dst = norm(args[1] if len(args) > 1 else os.path.basename(os.path.normpath(src)), cwd)
    if os.path.isdir(src):
        card.mkdir(dst)
        for name in sorted(os.listdir(src)):
            do_put(card, [os.path.join(src, name), '/' + posixpath.join(dst, name)], '/')
        return
    st = card.stat(dst)
    if st and st[0]:
        dst = posixpath.join(dst, os.path.basename(src))
    if posixpath.dirname(dst):
        card.mkdir(posixpath.dirname(dst))
    with open(src, 'rb') as f:
        card.write(dst, f.read(), progress_bar(dst))


def do_rm(card, args, cwd):
    recursive = '-r' in args
    for a in (a for a in args if a != '-r'):
        card.rm(norm(a, cwd), recursive)


def do_mkdir(card, args, cwd):
    for a in args:
        card.mkdir(norm(a, cwd))


def do_mv(card, args, cwd):
    card.mv(norm(args[0], cwd), norm(args[1], cwd))


def do_df(card, args, cwd):
    free, total = card.df()
    print(f'  {human(free * 1024)} free of {human(total * 1024)}')


COMMANDS = {'ls': do_ls, 'tree': do_tree, 'get': do_get, 'put': do_put, 'rm': do_rm, 'mkdir': do_mkdir,
            'mv': do_mv, 'df': do_df}


def shell(card):
    cwd = '/'
    print('SD card of the badge. Commands: ' + ', '.join(COMMANDS) + ', cd, pwd, exit')
    while True:
        try:
            line = input(f'sd:{cwd}> ')
        except (EOFError, KeyboardInterrupt):
            print()
            return
        try:
            words = shlex.split(line)
        except ValueError as e:
            print(f'  {e}')
            continue
        if not words:
            continue
        cmd, args = words[0], words[1:]
        try:
            if cmd in ('exit', 'quit'):
                return
            elif cmd == 'pwd':
                print('  ' + cwd)
            elif cmd == 'cd':
                path = '/' + norm(args[0] if args else '/', cwd)
                st = card.stat(path.lstrip('/'))
                if not st or not st[0]:
                    raise SdError(f'{path}: not a folder')
                cwd = path
            elif cmd in COMMANDS:
                COMMANDS[cmd](card, args, cwd)
            else:
                print(f'  unknown command {cmd}')
        except (SdError, OSError, IndexError) as e:
            print(f'  error: {e}' if not isinstance(e, IndexError) else '  missing argument')


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument('--port', help='serial port (default: the first Raspberry Pi Pico found)')
    parser.add_argument('-v', '--verbose', action='store_true', help='show the log lines of the badge')
    parser.add_argument('-r', '--recursive', action='store_true', help='rm: a folder and its content')
    parser.add_argument('command', choices=list(COMMANDS) + ['shell'])
    parser.add_argument('args', nargs='*')
    a = parser.parse_args()
    port = a.port or find_port()
    if not port:
        sys.exit('no badge found: is it connected and switched on?')
    try:
        card = Card(port, a.verbose)
    except serial.SerialException as e:
        sys.exit(f'cannot open {port}: {e} (used by another application?)')
    try:
        if a.command == 'shell':
            shell(card)
        else:
            COMMANDS[a.command](card, a.args + (['-r'] if a.recursive and a.command == 'rm' else []), '/')
    except SdError as e:
        sys.exit(f'error: {e}')
    finally:
        card.close()


if __name__ == '__main__':
    main()
