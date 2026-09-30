#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""
Remote control of the badge (firmware badge_menu) from a PC, over its USB serial port:
- the screen of the badge, shown big (and updated each time the badge draws),
- the buttons (on screen, or with the keyboard), including the long presses,
- the log of the badge, and screenshots in PNG.

Requires Python 3 with Tkinter (included in the Windows and macOS installers, "python3-tk" on Debian/Ubuntu) and pyserial.

Keyboard (the window must have the focus):
    Up / y          left flank (up in the lists)          Shift: long press
    Down / x        right flank (down in the lists)       Shift: long press
    Left / a        left wing (back, cancel)              Shift: long press
    Right / Enter / b   right wing (OK)                   Shift: long press
    F5              ask the screen again
    F12             screenshot (PNG)
Keyboard mode (check box): the characters typed go to the text editor of the badge (name, contact card, answers),
    Enter = done, Escape = cancel, Backspace = erase; the arrows are still the buttons.
Several badges plugged in: choose one in the "Badge" list (or --port).

Protocol (see src/menu/main.c): the keys a, b, x, y simulate the buttons (A, B, X, Y: long presses),
'[' / ']' start / stop sending the screen, 's' sends it once, 0x02 + a character types it in a text editor. The badge answers with lines
"@FB <BW|4G|WHITE|BLACK> [<base64 lsb plane> [<base64 msb plane>]]", other lines are its log.

Without window: python badge_remote.py --snapshot screen.png   (saves the current screen and exits)
"""

import argparse
import base64
import datetime
import os
import queue
import struct
import sys
import threading
import time
import zlib

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit('This program requires pyserial: pip install pyserial')

WIDTH = HEIGHT = 200
PICO_VID = 0x2E8A
# Colors of the 4 gray levels, 0 = black to 3 = white, with a paper tone like the e-Paper
GRAYS = [(0x20, 0x20, 0x20), (0x6E, 0x6E, 0x6A), (0xB4, 0xB4, 0xAE), (0xEC, 0xEC, 0xE4)]


# ------ Badge connection ------

def badge_ports():
    """The badges plugged in: [(port, label)], label = port and serial number of the RP2040."""
    return sorted((p.device, f'{p.device}  ({p.serial_number or "?"})')
                  for p in serial.tools.list_ports.comports() if p.vid == PICO_VID)


def find_port():
    ports = badge_ports()
    return ports[0][0] if ports else None


def decode_frame(line):
    """'@FB kind [lsb [msb]]' -> list of rows of gray levels (0..3), or None."""
    parts = line.split()
    if len(parts) < 2 or parts[0] != '@FB':
        return None
    kind = parts[1]
    if kind in ('WHITE', 'BLACK'):
        v = 3 if kind == 'WHITE' else 0
        return [[v] * WIDTH for _ in range(HEIGHT)]
    try:
        lsb = base64.b64decode(parts[2])
        msb = base64.b64decode(parts[3]) if kind == '4G' else None
    except (IndexError, ValueError):
        return None
    if len(lsb) != WIDTH * HEIGHT // 8 or (msb is not None and len(msb) != len(lsb)):
        return None
    rows = []
    for y in range(HEIGHT):
        row = []
        for x in range(WIDTH):
            i, bit = y * (WIDTH // 8) + x // 8, 0x80 >> (x % 8)
            l = 1 if lsb[i] & bit else 0
            if msb is None:
                row.append(3 if l else 0)  # Black and white
            else:
                row.append((2 if msb[i] & bit else 0) | l)
        rows.append(row)
    return rows


class Badge(threading.Thread):
    """Reads the serial port in the background: frames and log lines go to queues, reconnects when needed."""

    def __init__(self, port=None):
        super().__init__(daemon=True)
        self.port_name = port
        self.ser = None
        self.frames = queue.Queue()
        self.logs = queue.Queue()
        self.running = True
        self.lock = threading.Lock()

    def send(self, keys):
        with self.lock:
            if self.ser:
                try:
                    self.ser.write(keys.encode())
                except serial.SerialException:
                    pass

    def connected(self):
        return self.ser is not None

    def switch(self, port):
        """Connects to another badge (several badges plugged in)."""
        with self.lock:
            self.port_name = port
            if self.ser:
                try:
                    self.ser.write(b']')
                    self.ser.close()
                except serial.SerialException:
                    pass
                self.ser = None

    def run(self):
        busy_reported = None
        while self.running:
            if not self.ser:
                port = self.port_name or find_port()
                if not port:
                    time.sleep(0.5)
                    continue
                try:
                    ser = serial.Serial(port, 115200, timeout=0.2)
                except serial.SerialException as e:
                    if busy_reported != port:
                        self.logs.put(f'--- cannot open {port} (used by another program?): {e}')
                        busy_reported = port
                    time.sleep(0.5)
                    continue
                busy_reported = None
                with self.lock:
                    if port != (self.port_name or port):
                        ser.close()  # Switched meanwhile
                        continue
                    self.ser = ser
                self.logs.put(f'--- connected to {port}')
                self.send('[')  # Stream the screen (a badge without screen only sends its log)
            ser = self.ser
            try:
                line = ser.readline() if ser else b''
            except (serial.SerialException, AttributeError, TypeError, OSError):
                if not self.running:
                    break  # Closed by stop()
                with self.lock:
                    if self.ser is ser:
                        self.ser = None
                        self.logs.put('--- disconnected')
                continue
            if not line:
                continue
            line = line.decode('utf-8', errors='replace').rstrip()
            if line.startswith('@FB'):
                frame = decode_frame(line)
                if frame:
                    self.frames.put(frame)
            else:
                self.logs.put(line)

    def stop(self):
        self.send(']')
        self.running = False
        with self.lock:
            if self.ser:
                self.ser.close()
                self.ser = None


# ------ PNG (no dependency) ------

def save_png(path, rows, zoom=1):
    raw = bytearray()
    for row in rows:
        line = bytearray()
        for v in row:
            line += bytes(GRAYS[v]) * zoom
        for _ in range(zoom):
            raw += b'\0' + line
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)
    w, h = len(rows[0]) * zoom, len(rows) * zoom
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


def snapshot(port, path, timeout=5):
    """Saves the current screen of the badge and returns (without window)."""
    badge = Badge(port)
    badge.start()
    end = time.time() + timeout
    while time.time() < end and not badge.connected():
        time.sleep(0.1)
    if not badge.connected():
        sys.exit('badge not found')
    badge.send('s')
    try:
        rows = badge.frames.get(timeout=max(0.5, end - time.time()))
    except queue.Empty:
        badge.stop()
        sys.exit('no screen received (is the firmware badge_menu flashed?)')
    badge.stop()
    save_png(path, rows)
    print(f'screen saved in {path}')


# ------ Window ------

def run_window(port, zoom, on_ready=None):
    """on_ready(root, badge): called when the window is created (for the tests)."""
    import tkinter as tk
    from tkinter import ttk

    if sys.platform == 'win32':
        # Sharp pixels on scaled displays (otherwise Windows stretches the window and blurs the screen of the badge)
        try:
            import ctypes
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except (AttributeError, OSError):
            pass

    badge = Badge(port)
    badge.start()

    root = tk.Tk()
    root.title('Badge SecSea - télécommande')
    if not zoom:
        # As big as possible: the window must also fit the buttons and the log
        zoom = max(2, min(4, (root.winfo_screenheight() - 380) // HEIGHT))
    root.configure(padx=10, pady=10)

    size = WIDTH * zoom
    image = tk.PhotoImage(width=WIDTH, height=HEIGHT)
    image.put('#%02x%02x%02x' % GRAYS[3], to=(0, 0, WIDTH, HEIGHT))
    zoomed = [image.zoom(zoom)]
    canvas = tk.Canvas(root, width=size, height=size, highlightthickness=4, highlightbackground='#333333')
    canvas.grid(row=0, column=0, columnspan=4)
    canvas_image = canvas.create_image(0, 0, anchor='nw', image=zoomed[0])
    last = {'rows': None, 'count': 0, 't0': time.time()}

    def show(rows):
        colors = ['#%02x%02x%02x' % g for g in GRAYS]
        image.put(' '.join('{' + ' '.join(colors[v] for v in row) + '}' for row in rows))
        zoomed[0] = image.zoom(zoom)
        canvas.itemconfigure(canvas_image, image=zoomed[0])
        last['rows'] = rows
        last['count'] += 1

    # Buttons of the badge, placed like on the badge (flanks outside, wings inside)
    def press(key):
        badge.send(key)
        root.focus_set()

    buttons = [
        ('Flanc gauche\n▲', 'y', 'Y'),
        ('Aile gauche\nretour', 'a', 'A'),
        ('Aile droite\nOK', 'b', 'B'),
        ('Flanc droit\n▼', 'x', 'X'),
    ]
    for col, (label, key, long_key) in enumerate(buttons):
        frame = tk.Frame(root)
        frame.grid(row=1, column=col, pady=(10, 0), sticky='n')
        tk.Button(frame, text=label, width=12, command=lambda k=key: press(k)).pack()
        if long_key:
            tk.Button(frame, text='appui long', width=12, command=lambda k=long_key: press(k)).pack(pady=(2, 0))

    status = tk.StringVar(value='recherche du badge...')
    tk.Label(root, textvariable=status, anchor='w').grid(row=2, column=0, columnspan=4, sticky='we', pady=(8, 0))

    tools = tk.Frame(root)
    tools.grid(row=3, column=0, columnspan=4, sticky='we', pady=(4, 0))

    def screenshot():
        if not last['rows']:
            return
        name = datetime.datetime.now().strftime('badge_%Y%m%d_%H%M%S.png')
        save_png(name, last['rows'], zoom)
        badge.logs.put(f'--- screenshot saved: {os.path.abspath(name)}')

    log_visible = tk.BooleanVar(value=True)
    keyboard = tk.BooleanVar(value=False)
    ttk.Button(tools, text='Capture (F12)', command=screenshot).pack(side='left')
    ttk.Button(tools, text='Rafraîchir (F5)', command=lambda: badge.send('s')).pack(side='left', padx=4)
    ttk.Button(tools, text='Diagnostic', command=lambda: badge.send('!')).pack(side='left')
    ttk.Checkbutton(tools, text='Journal', variable=log_visible,
                    command=lambda: log.grid() if log_visible.get() else log.grid_remove()).pack(side='right')
    ttk.Checkbutton(tools, text='Mode clavier (saisie de texte)', variable=keyboard,
                    command=lambda: root.focus_set()).pack(side='right', padx=8)

    # Several badges plugged in: choose the one to drive
    ports_bar = tk.Frame(root)
    ports_bar.grid(row=5, column=0, columnspan=4, sticky='we', pady=(6, 0))
    tk.Label(ports_bar, text='Badge :').pack(side='left')
    port_choice = tk.StringVar()
    port_box = ttk.Combobox(ports_bar, textvariable=port_choice, state='readonly', width=40)
    port_box.pack(side='left', padx=4)
    port_labels = {}

    def refresh_ports():
        ports = badge_ports()
        port_labels.clear()
        port_labels.update({label: device for device, label in ports})
        port_box['values'] = [label for _, label in ports]
        current = badge.port_name or (badge.ser.port if badge.ser else None)
        for device, label in ports:
            if device == current:
                port_choice.set(label)
        root.after(2000, refresh_ports)

    def on_port(_event):
        device = port_labels.get(port_choice.get())
        if device and device != badge.port_name:
            badge.switch(device)
            badge.logs.put(f'--- switching to {device}')
        root.focus_set()
    port_box.bind('<<ComboboxSelected>>', on_port)
    refresh_ports()

    log = tk.Text(root, height=10, width=80, font=('Consolas', 9), bg='#111111', fg='#dddddd')
    log.grid(row=4, column=0, columnspan=4, sticky='we', pady=(6, 0))

    # Keyboard
    keys = {'Up': ('y', 'Y'), 'Down': ('x', 'X'), 'Left': ('a', 'A'), 'Right': ('b', 'B'), 'Return': ('b', 'B'),
            'y': ('y', 'Y'), 'x': ('x', 'X'), 'a': ('a', 'A'), 'b': ('b', 'B'),
            'Y': ('Y', 'Y'), 'X': ('X', 'X'), 'A': ('A', 'A'), 'B': ('B', 'B')}

    # Keyboard mode: the characters go to the text editor of the badge (0x02 + character), the arrows stay buttons
    text_keys = {'Return': '\r', 'KP_Enter': '\r', 'BackSpace': '\b', 'Escape': '\x1b'}

    def on_key(event):
        if keyboard.get() and event.keysym not in ('Up', 'Down', 'Left', 'Right', 'F5', 'F12'):
            c = text_keys.get(event.keysym, event.char)
            if c and len(c) == 1 and (c in '\r\b\x1b' or ' ' <= c < '\x7f'):
                badge.send('\x02' + c)
            return 'break'
        if event.keysym in keys:
            short, long_ = keys[event.keysym]
            badge.send(long_ if event.state & 0x0001 else short)  # Shift
        elif event.keysym == 'F5':
            badge.send('s')
        elif event.keysym == 'F12':
            screenshot()
    root.bind('<Key>', on_key)

    def poll():
        # Only show the last frame received (the badge may draw faster than we display)
        rows = None
        while not badge.frames.empty():
            rows = badge.frames.get_nowait()
        if rows:
            show(rows)
        while not badge.logs.empty():
            log.insert('end', badge.logs.get_nowait() + '\n')
            if int(log.index('end-1c').split('.')[0]) > 500:
                log.delete('1.0', '100.0')
            log.see('end')
        fps = last['count'] / max(1e-3, time.time() - last['t0'])
        if time.time() - last['t0'] > 3:
            last['count'], last['t0'] = 0, time.time()
        help_ = ('mode clavier : tapez le texte, Entrée = valider, Échap = annuler' if keyboard.get()
                 else '↑↓ flancs, ← retour, → OK, Maj = appui long')
        status.set(('connecté' if badge.connected() else 'recherche du badge...') + f'   —   {fps:.1f} images/s'
                   + '   —   ' + help_)
        root.after(30, poll)

    def on_close():
        badge.stop()
        root.destroy()
    root.protocol('WM_DELETE_WINDOW', on_close)
    root.focus_set()
    poll()
    if on_ready:
        root.after(0, on_ready, root, badge)
    root.mainloop()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Remote control of the badge (firmware badge_menu)')
    parser.add_argument('--port', '-p', default=None, help='serial port (default: the first Raspberry Pi RP2040 found)')
    parser.add_argument('--zoom', '-z', type=int, default=0, help='zoom of the screen (default: from the size of the display, 2 to 4)')
    parser.add_argument('--snapshot', '-s', default=None, help='save the current screen in this PNG file and exit')
    args = parser.parse_args()
    if args.snapshot:
        snapshot(args.port, args.snapshot)
    else:
        run_window(args.port, args.zoom)
