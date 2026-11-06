#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""Badge SecSea Studio: everything of the badge from the PC in one window, as qFlipper for the Flipper Zero.

- The badge: by its USB cable, or by the Wi-Fi of its ESP32-CAM module (Badge > Télécommande on the badge): the
  access point of the module (192.168.4.1) or a network, where the modules are found by themselves (UDP).
- The "Écran" tab: the remote control (badge_remote.py): its screen, its buttons, the text typed in its editors; by
  the USB also its log, the diagnostic, the admin mode, the bit rate of the radio, the commands of the serial port.
- The "Fichiers" tab: the SD card (sd_files.py): the files of the PC and of the card, send, fetch, new folder, rename,
  delete. By the USB only (the module does not carry files), on the same connection as the screen: both at once.

    python tools/badge_studio.py                          the first badge plugged in
    python tools/badge_studio.py --port COM9
    python tools/badge_studio.py --wifi 192.168.4.1       the access point of the module
    python tools/badge_studio.py --wifi cigale-ada.local --pin 123456   on a network: the PIN shown by the badge

Requires Python 3 with Tkinter and pyserial. badge_remote.py and sd_files.py still work alone (and their command
lines: --snapshot, ls, get, put...). Close them, and any terminal on the port, before using the USB here.

The Wi-Fi (esp32cam/src/main.cpp): GET /screen?since=<id> (204: no change, otherwise [kind][id lo][id hi] and the
planes, camlink.h CAMLINK_SCREEN_*), /key?k=<a A b B x X y Y>, /type?t=<text>; on a network ?pin=<the PIN of the
badge>. The modules of a network say "CIGALE1 <name>" by UDP broadcast on the port 4210.
"""

import argparse
import queue
import socket
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from badge_remote import HEIGHT, WIDTH, Badge, badge_ports, dpi_aware, find_port, remote_panel  # noqa: E402
from sd_files import files_panel  # noqa: E402

AP_HOST = '192.168.4.1'  # The access point of the module
PEER_PORT = 4210  # PEER_PORT of esp32cam/src/main.cpp
WEB_KEYS = 'aAbBxXyY'  # The buttons the module passes on (get_key() of main.cpp)
FRAME = WIDTH * HEIGHT // 8


def decode_screen(data):
    """The answer of /screen: (its id, rows of gray levels 0..3), None when it is not one"""
    if len(data) < 3:
        return None
    kind, sid = data[0], data[1] | data[2] << 8
    if kind in (2, 3):  # All white, all black
        return sid, [[3 if kind == 2 else 0] * WIDTH for _ in range(HEIGHT)]
    planes = 2 if kind == 1 else 1
    if kind > 1 or len(data) < 3 + planes * FRAME:
        return None
    lsb, msb = data[3:3 + FRAME], data[3 + FRAME:3 + 2 * FRAME] if kind == 1 else None
    rows = []
    for y in range(HEIGHT):
        row = []
        for x in range(WIDTH):
            i, bit = y * (WIDTH // 8) + x // 8, 0x80 >> (x % 8)
            low = 1 if lsb[i] & bit else 0
            row.append((2 if msb[i] & bit else 0) | low if msb else (3 if low else 0))
        rows.append(row)
    return sid, rows


class WifiBadge(threading.Thread):
    """The badge through the web server of its ESP32-CAM module: like badge_remote.Badge for remote_panel(), but only
    the screen, the buttons and the text (usb False: no log, no SD card, no other command of the serial port)."""

    usb = False

    def __init__(self, host, pin=''):
        super().__init__(daemon=True)
        self.host, self.pin = host, pin.strip()
        self.port_name = f'wifi:{host}'
        self.frames, self.logs, self.sd_lines = queue.Queue(), queue.Queue(), queue.Queue()
        self.error = ''
        self.running = True
        self.since = -1
        self.last_ok = 0.0
        self.keys = queue.Queue()  # Sent in order by their own thread (a slow network does not block the screen)
        self.said_usb_only = False
        threading.Thread(target=self._sender, daemon=True).start()

    def _url(self, path, **query):
        if self.pin:
            query['pin'] = self.pin
        return f'http://{self.host}{path}?' + urllib.parse.urlencode(query)

    def _get(self, path, **query):
        """(status, body); raises OSError (urllib.error.URLError, timeout...) when the module does not answer"""
        try:
            with urllib.request.urlopen(self._url(path, **query), timeout=4) as r:
                return r.status, r.read()
        except urllib.error.HTTPError as e:
            return e.code, b''

    def _denied(self):
        self.error = ('code PIN demandé : celui affiché par le badge (Télécommande > Se connecter)' if not self.pin
                      else 'code PIN refusé (celui affiché par le badge)')

    def connected(self):
        return time.time() - self.last_ok < 5

    def send(self, keys):
        i = 0
        while i < len(keys):
            c = keys[i]
            if c == '\x02' and i + 1 < len(keys):  # A character for the text editor of the badge
                ch = keys[i + 1]
                if ' ' <= ch < '\x7f' or ch in '\r\b':
                    self.keys.put(('/type', {'t': ch}))
                elif ch == '\x1b':
                    self.keys.put(('/key', {'k': 'a'}))  # Escape: cancel, as the button A
                else:
                    self._usb_only('les lettres accentuées')
                i += 2
                continue
            if c in WEB_KEYS:
                self.keys.put(('/key', {'k': c}))
            elif c == 's':
                self.since = -1  # The screen again
            elif c not in '[]!':
                self._usb_only(f'la commande {c!r}')
            i += 1

    def _usb_only(self, what):
        if not self.said_usb_only:
            self.logs.put(f'--- par le Wi-Fi : les boutons et le texte seulement ({what} : par l\'USB)')
            self.said_usb_only = True

    def write_raw(self, _data):
        return False  # The SD card: by the USB only

    def _sender(self):
        while self.running:
            try:
                path, query = self.keys.get(timeout=0.5)
            except queue.Empty:
                continue
            try:
                status, _ = self._get(path, **query)
                if status == 401:
                    self._denied()
            except OSError as e:
                self.error = f'module injoignable : {e}'

    def run(self):
        self.logs.put(f'--- Wi-Fi : http://{self.host}/')
        while self.running:
            try:
                status, body = self._get('/screen', since=self.since)
            except OSError as e:
                self.error = f'module injoignable ({self.host}) : {getattr(e, "reason", e)}'
                time.sleep(1)
                continue
            if status == 401:
                self._denied()
                time.sleep(1)
                continue
            if status in (200, 204):
                if not self.connected():
                    self.logs.put(f'--- connecté au module {self.host}')
                self.last_ok = time.time()
                self.error = ''
                screen = decode_screen(body) if status == 200 else None
                if screen:
                    self.since = screen[0]
                    self.frames.put(screen[1])
            time.sleep(0.15)

    def stop(self):
        self.running = False


class Link:
    """The badge of the window, USB or Wi-Fi, for both panels: they keep this object, it changes what is behind it
    (the queues stay the same)."""

    def __init__(self):
        self.frames, self.logs, self.sd_lines = queue.Queue(), queue.Queue(), queue.Queue()
        self.current = None
        self.generation = 0  # + 1 at each change of badge

    def _use(self, badge):
        if self.current:
            self.current.stop()
        badge.frames, badge.logs, badge.sd_lines = self.frames, self.logs, self.sd_lines
        self.current = badge
        self.generation += 1
        badge.start()

    def use_usb(self, port):
        self._use(Badge(port))

    def use_wifi(self, host, pin=''):
        self._use(WifiBadge(host, pin))

    # What remote_panel() and sd_files.LinkCard use
    @property
    def usb(self):
        return bool(self.current and self.current.usb)

    @property
    def port_name(self):
        return self.current.port_name if self.current else None

    @property
    def error(self):
        return self.current.error if self.current else 'aucun badge choisi'

    def connected(self):
        return bool(self.current and self.current.connected())

    def send(self, keys):
        if self.current:
            self.current.send(keys)

    def write_raw(self, data):
        return bool(self.current and self.current.write_raw(data))

    def stop(self):
        if self.current:
            self.current.stop()

    def __getattr__(self, name):  # ser, switch_to... of a Badge (the list of the ports of remote_panel: not used)
        return getattr(self.current, name, None) if self.__dict__.get('current') else None


class Finder(threading.Thread):
    """The modules of the network: their "CIGALE1 <name>" (UDP broadcast) -> {name: (ip, time)}"""

    def __init__(self):
        super().__init__(daemon=True)
        self.found = {}

    def run(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(('', PEER_PORT))
        except OSError:
            return  # The port is taken: the address can still be typed
        while True:
            try:
                data, (ip, _) = s.recvfrom(128)
            except OSError:
                time.sleep(1)
                continue
            text = data.decode('utf-8', 'replace')
            if text.startswith('CIGALE1 ') and len(text) > 8:
                self.found[text[8:].strip()] = (ip, time.time())

    def modules(self):
        now = time.time()
        return sorted((name, ip) for name, (ip, t) in list(self.found.items()) if now - t < 15)


def run(port=None, host=None, pin='', zoom=0):
    import tkinter as tk
    from tkinter import ttk

    dpi_aware()
    link = Link()
    finder = Finder()
    finder.start()

    root = tk.Tk()
    root.title('Badge SecSea Studio')
    root.configure(padx=8, pady=8)

    # --- The badge: USB or Wi-Fi
    bar = ttk.Frame(root)
    bar.pack(fill='x', pady=(0, 6))
    ttk.Label(bar, text='Badge :').pack(side='left')
    choice = tk.StringVar()
    box = ttk.Combobox(bar, textvariable=choice, state='readonly', width=44)
    box.pack(side='left', padx=4)
    host_var, pin_var = tk.StringVar(value=host or ''), tk.StringVar(value=pin)
    wifi_bar = ttk.Frame(bar)
    ttk.Label(wifi_bar, text='Adresse :').pack(side='left', padx=(8, 2))
    ttk.Entry(wifi_bar, textvariable=host_var, width=22).pack(side='left')
    ttk.Label(wifi_bar, text='PIN :').pack(side='left', padx=(8, 2))
    ttk.Entry(wifi_bar, textvariable=pin_var, width=8).pack(side='left')
    ttk.Button(wifi_bar, text='Connecter', command=lambda: connect_wifi()).pack(side='left', padx=6)
    state_var = tk.StringVar()
    ttk.Label(bar, textvariable=state_var).pack(side='right')
    entries = {}  # The label of the list -> ('usb', port) or ('wifi', host)
    AP_LABEL = f'Wi-Fi   point d\'accès du module ({AP_HOST})'

    def refresh():
        entries.clear()
        for device, label in badge_ports():
            entries['USB   ' + label] = ('usb', device)
        for name, ip in finder.modules():
            entries[f'Wi-Fi   {name}  ({ip})'] = ('wifi', ip)
        entries[AP_LABEL] = ('wifi', AP_HOST)
        current = link.port_name
        if current and current.startswith('wifi:') and current[5:] not in [w for _, w in entries.values()]:
            entries[f'Wi-Fi   {current[5:]}'] = ('wifi', current[5:])  # An address typed
        box['values'] = list(entries)
        for label, (kind, where) in entries.items():
            if (kind == 'usb' and where == current) or (kind == 'wifi' and current == f'wifi:{where}'):
                if choice.get() != label:
                    choice.set(label)
        root.after(2000, refresh)

    def show_wifi_bar(on):
        if on:
            wifi_bar.pack(side='left')
        else:
            wifi_bar.pack_forget()

    def connect_wifi():
        if host_var.get().strip():
            link.use_wifi(host_var.get().strip(), pin_var.get())
            badge_changed()

    def chosen(_event=None):
        kind, where = entries.get(choice.get(), (None, None))
        if kind == 'usb':
            show_wifi_bar(False)
            if link.port_name != where:
                link.use_usb(where)
                badge_changed()
        elif kind == 'wifi':
            show_wifi_bar(True)
            host_var.set(where)
            connect_wifi()
        root.focus_set()
    box.bind('<<ComboboxSelected>>', chosen)

    # --- The tabs
    tabs = ttk.Notebook(root)
    tabs.pack(fill='both', expand=True)
    screen_tab, files_tab = tk.Frame(tabs, padx=6, pady=6), tk.Frame(tabs)
    tabs.add(screen_tab, text='  Écran  ')
    tabs.add(files_tab, text='  Fichiers (carte SD)  ')
    remote_panel(screen_tab, link, zoom, choose_port=False)
    files = files_panel(files_tab, link=link)
    tried = [None]  # The badge (generation) whose card was asked: once, the button Actualiser of the tab again

    def badge_changed():
        tried[0] = None
        files.disconnect('La carte SD passe par l\'USB : choisir un badge branché en USB.' if not link.usb
                         else 'Connexion à la carte SD...')

    def files_follow():
        """The card is read when its tab is shown, by the USB, once per badge chosen"""
        try:
            on_files = tabs.select() == str(files_tab)
            if on_files and link.usb and link.connected() and tried[0] != link.generation:
                tried[0] = link.generation
                files.connect()
            usb = 'USB' if link.usb else 'Wi-Fi'
            state_var.set(f'{usb} : connecté' if link.connected() else f'{usb} : ' + (link.error or 'connexion...'))
        finally:
            root.after(500, files_follow)
    tabs.bind('<<NotebookTabChanged>>', lambda e: root.focus_set())

    root.report_callback_exception = lambda kind, value, tb: link.logs.put(f'--- error: {value!r}')

    def on_close():
        try:
            files.close()
            link.stop()
        finally:
            root.destroy()
    root.protocol('WM_DELETE_WINDOW', on_close)

    # The first badge: the one asked, otherwise the first one plugged in, otherwise the access point of a module
    if host:
        show_wifi_bar(True)
        connect_wifi()
    else:
        port = port or find_port()
        if port:
            link.use_usb(port)
        else:
            show_wifi_bar(True)
            host_var.set(AP_HOST)
            link.logs.put('--- aucun badge en USB : choisir dans la liste « Badge », ou le Wi-Fi de son module')
        badge_changed()
    refresh()
    files_follow()
    root.focus_set()
    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument('--port', '-p', help='serial port (default: the first badge plugged in)')
    parser.add_argument('--wifi', '-w', metavar='HOST', help=f'the module of the badge by the Wi-Fi ({AP_HOST}: its '
                                                             'access point)')
    parser.add_argument('--pin', default='', help='on a network: the PIN shown by the badge')
    parser.add_argument('--zoom', '-z', type=int, default=0, help='zoom of the screen (default: from the display)')
    a = parser.parse_args()
    run(a.port, a.wifi, a.pin, a.zoom)


if __name__ == '__main__':
    main()
