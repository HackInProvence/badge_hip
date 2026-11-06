# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""Test of tools/badge_studio.py without a badge: the screen of the Wi-Fi decoded, a fake module (its web server:
the screen, the buttons, the text, the PIN), the SD card through a shared connection (sd_files.LinkCard on a fake
badge that answers as src/menu/sd_serial.c)."""

import base64
import http.server
import os
import queue
import sys
import threading
import time
import urllib.parse

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'tools'))
import badge_studio  # noqa: E402
import sd_files  # noqa: E402

failures = 0
passes = 0


def check(cond, what):
    global failures, passes
    if cond:
        passes += 1
    else:
        failures += 1
        print('FAILED: ' + what)


# ------ The screen of the Wi-Fi

FRAME = 5000
plane = bytearray(FRAME)
plane[0] = 0x80  # (0, 0) white, the rest black
bw = bytes([0, 7, 1]) + plane
sid, rows = badge_studio.decode_screen(bw)
check(sid == 0x107 and rows[0][0] == 3 and rows[0][1] == 0 and rows[199][199] == 0, 'black and white')
msb = bytearray(FRAME)
msb[0] = 0xC0  # (0, 0): msb 1 + lsb 1 = 3, (1, 0): msb 1 = 2
sid, rows = badge_studio.decode_screen(bytes([1, 2, 0]) + plane + msb)
check(rows[0][0] == 3 and rows[0][1] == 2 and rows[0][2] == 0, '4 grays')
check(badge_studio.decode_screen(bytes([2, 0, 0]))[1][100][100] == 3, 'all white')
check(badge_studio.decode_screen(bytes([3, 0, 0]))[1][5][5] == 0, 'all black')
check(badge_studio.decode_screen(bytes([0, 0, 0]) + b'\0' * 10) is None, 'too short')

# ------ A fake module: its screen changes once, it records the buttons and the text

module = {'id': 5, 'keys': [], 'text': '', 'pin': ''}


class Module(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        q = dict(urllib.parse.parse_qsl(url.query))
        if module['pin'] and q.get('pin') != module['pin']:
            self.send_response(401)
            self.end_headers()
            return
        if url.path == '/screen':
            if q.get('since') == str(module['id']):
                self.send_response(204)
                self.end_headers()
                return
            body = bytes([0, module['id'] & 0xFF, module['id'] >> 8]) + plane
        elif url.path == '/key':
            module['keys'].append(q.get('k'))
            body = b'ok'
        elif url.path == '/type':
            module['text'] += q.get('t', '')
            body = b'ok'
        else:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Module)
threading.Thread(target=server.serve_forever, daemon=True).start()
host = f'127.0.0.1:{server.server_address[1]}'


def wait(cond, timeout=5):
    end = time.time() + timeout
    while time.time() < end:
        if cond():
            return True
        time.sleep(0.05)
    return False


link = badge_studio.Link()
link.use_wifi(host)
check(wait(link.connected), 'Wi-Fi: connected')
check(not link.usb, 'Wi-Fi: not the USB')
frame = link.frames.get(timeout=5)
check(frame[0][0] == 3 and frame[0][1] == 0, 'Wi-Fi: the screen')
link.send('b')
link.send('Y')
link.send('\x02h\x02i\x02\r')
link.send('!')  # The serial port only: not sent, said once in the log
check(wait(lambda: module['keys'] == ['b', 'Y'] and module['text'] == 'hi\r'), 'Wi-Fi: buttons and text')
check(not link.write_raw(b'\x05L \n'), 'Wi-Fi: no SD card')
module['id'] = 6
check(wait(lambda: not link.frames.empty()), 'Wi-Fi: the screen changed')
module['pin'] = '123456'
check(wait(lambda: 'PIN' in link.error), 'Wi-Fi: the PIN asked')
link.use_wifi(host, '123456')
check(wait(link.connected) and wait(lambda: not link.error), 'Wi-Fi: with the PIN')
link.stop()
server.shutdown()

# ------ The SD card through the connection of the badge (LinkCard)


class FakeBadge:
    """Answers the commands of the card as sd_serial.c, in its sd_lines; in between, other lines of the badge go
    elsewhere (the thread of badge_remote.Badge)"""

    usb = True

    def __init__(self):
        self.sd_lines = queue.Queue()
        self.files = {'README.TXT': b'hello', 'IMAGES/a.epi': b'x' * 1300}
        self.pending = b''
        self.connected_ = True

    def write_raw(self, data):
        if not self.connected_:
            return False
        assert data[:1] == b'\x05' and data.endswith(b'\n'), data
        line = data[1:-1].decode('utf-8')
        cmd, _, arg = line.partition(' ')
        out = self.sd_lines.put
        if cmd == 'T' or cmd == 'U' or cmd == 'C':
            out('@SD OK')
        elif cmd == 'L':
            prefix = arg.strip('/') + '/' if arg.strip('/') else ''
            seen = set()
            for name, data_ in self.files.items():
                if name.startswith(prefix):
                    rest = name[len(prefix):]
                    if '/' in rest:
                        d = rest.split('/')[0]
                        if d not in seen:
                            seen.add(d)
                            out(f'@SD E d 0 0 {d}')
                    else:
                        out(f'@SD E f {len(data_)} 0 {rest}')
            out('@SD OK')
        elif cmd == 'S':
            name = arg.strip('/')
            if name in self.files:
                out(f'@SD OK f {len(self.files[name])} 0')
            elif any(n.startswith(name + '/') for n in self.files):
                out('@SD OK d 0 0')
            else:
                out('@SD ERR 4 no file')
        elif cmd == 'R':
            off, n, name = arg.split(' ', 2)
            chunk = self.files[name][int(off):int(off) + int(n)]
            out(f'@SD OK {len(chunk)} {base64.b64encode(chunk).decode()}')
        elif cmd == 'W':
            off, b64, name = arg.split(' ', 2)
            chunk = base64.b64decode(b64)
            old = self.files.get(name, b'') if int(off) else b''
            self.files[name] = old[:int(off)] + chunk
            out(f'@SD OK {len(chunk)}')
        else:
            out('@SD ERR 1 unknown')
        return True


fake = FakeBadge()
card = sd_files.LinkCard(fake)
names = [(n, d) for n, d, _, _ in card.ls('')]
check(names == [('IMAGES', True), ('README.TXT', False)], f'LinkCard: ls {names}')
check(card.read('IMAGES/a.epi') == b'x' * 1300, 'LinkCard: read (3 pieces)')
card.write('NEW.BIN', bytes(range(256)) * 5)
check(fake.files['NEW.BIN'] == bytes(range(256)) * 5, 'LinkCard: write')
fake.connected_ = False
try:
    card.ls('')
    check(False, 'LinkCard: lost connection')
except sd_files.SdError:
    check(True, '')

print(f'{passes} passed, {failures} failed')
sys.exit(1 if failures else 0)
