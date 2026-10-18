#!/usr/bin/env python3

# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/

"""File manager of the SD card of the badge, through its USB serial port (no need to take the card out).

Without a command: a window with the files of the PC on the left and the ones of the card on the right (send,
fetch, new folder, rename, delete; several files or folders at once).

The same from the command line:
    python tools/sd_files.py ls [DIR]                    list a folder (default: the root)
    python tools/sd_files.py tree [DIR]                  every file below a folder
    python tools/sd_files.py get CARD_PATH [LOCAL]       copy a file or a folder to the PC
    python tools/sd_files.py put LOCAL [CARD_PATH]       copy a file or a folder to the card
    python tools/sd_files.py rm [-r] CARD_PATH           delete a file (-r: a folder and its content)
    python tools/sd_files.py mkdir CARD_PATH             make a folder (and its parents)
    python tools/sd_files.py mv OLD NEW                  rename or move
    python tools/sd_files.py df                          free space
    python tools/sd_files.py shell                       interactive: the same commands, plus cd

Examples:
    python tools/sd_files.py put docs/sd/CONFERENCES CONFERENCES
    python tools/sd_files.py --port COM9 get IMAGES/chat.epi

The dates: on connection the badge gets the time of the PC (the files it creates are dated with it), and a file
copied keeps the date of its original, both ways.

Protocol (src/menu/sd_serial.c): the byte 0x05, then a command line; the badge answers "@SD OK ..." or
"@SD ERR <code> <text>"; the data goes in base64, 512 bytes per command; a date is the local time in seconds since
1970 counted as if UTC (FAT stores local times). The other lines are the log of the badge.
Close the other applications that use the serial port (badge_remote.py, a terminal...) first.
"""

import argparse
import base64
import calendar
import os
import posixpath
import queue
import shlex
import sys
import threading
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


def badge_ports():
    """The badges plugged in: [(port, label)]"""
    return sorted((p.device, f'{p.device}  ({p.serial_number or "?"})')
                  for p in serial.tools.list_ports.comports() if p.vid == PICO_VID)


def find_port():
    ports = badge_ports()
    return ports[0][0] if ports else None


# ------ Dates: the card keeps local times (FAT), the PC real times (seconds since 1970 UTC)

def to_card_time(epoch):
    return calendar.timegm(time.localtime(epoch))


def from_card_time(local):
    return time.mktime(time.gmtime(local)[:8] + (-1,))


def card_time_text(local):
    return time.strftime('%Y-%m-%d %H:%M', time.gmtime(local)) if local else ''


class Card:
    def __init__(self, port, verbose=False):
        self.ser = serial.Serial(port, 115200, timeout=0.2)
        self.verbose = verbose
        self.buf = b''
        time.sleep(0.2)
        self.ser.reset_input_buffer()
        self.command(f'T {to_card_time(time.time())}')  # The clock of the badge: the date of the files it writes

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
        """[(name, is_dir, size, date)], folders first"""
        entries, _ = self.command('L ' + path)
        out = []
        for e in entries:
            kind, size, date, name = e.split(' ', 3)
            out.append((name, kind == 'd', int(size), int(date)))
        return sorted(out, key=lambda x: (not x[1], x[0].lower()))

    def stat(self, path):
        """(is_dir, size, date), None when it does not exist"""
        try:
            _, r = self.command('S ' + path)
        except SdError as e:
            if 'no file' in str(e) or 'no path' in str(e):
                return None
            raise
        kind, size, date = r.split()
        return kind == 'd', int(size), int(date)

    def read(self, path, progress=None):
        st = self.stat(path)
        if st is None or st[0]:
            raise SdError(f'{path}: not a file')
        size = st[1]
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

    def write(self, path, data, progress=None, date=None):
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
        if date is not None:
            self.set_date(path, date)

    def set_date(self, path, epoch):
        self.command(f'U {to_card_time(epoch)} {path}')

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
            for name, _, _, _ in self.ls(path):
                self.rm(posixpath.join(path, name), True)
        self.command('D ' + path)

    def mv(self, old, new):
        self.command(f'N {old}\t{new}')

    def df(self):
        _, r = self.command('F', timeout=60)  # Counting the free clusters of a big card takes a while
        free, total = (int(x) for x in r.split())
        return free, total

    # ------ Copies of whole trees, keeping the dates

    def put(self, src, dst, progress=None, log=print):
        """Local file or folder -> card path (created)"""
        if os.path.isdir(src):
            self.mkdir(dst)
            for name in sorted(os.listdir(src)):
                self.put(os.path.join(src, name), posixpath.join(dst, name), progress, log)
            self.set_date(dst, os.path.getmtime(src))
            return
        if posixpath.dirname(dst):
            self.mkdir(posixpath.dirname(dst))
        log(f'{src} -> {dst}')
        with open(src, 'rb') as f:
            self.write(dst, f.read(), progress and (lambda d, t: progress(dst, d, t)), os.path.getmtime(src))

    def get(self, src, dst, progress=None, log=print):
        """Card file or folder -> local path (created)"""
        st = self.stat(src)
        if st is None:
            raise SdError(f'{src}: not found')
        if st[0]:
            os.makedirs(dst, exist_ok=True)
            for name, _, _, _ in self.ls(src):
                self.get(posixpath.join(src, name), os.path.join(dst, name), progress, log)
        else:
            log(f'{src} -> {dst}')
            data = self.read(src, progress and (lambda d, t: progress(src, d, t)))
            with open(dst, 'wb') as f:
                f.write(data)
        if st[2]:
            t = from_card_time(st[2])
            os.utime(dst, (t, t))


def human(n):
    for unit in ('B', 'KB', 'MB', 'GB'):
        if n < 1024 or unit == 'GB':
            return f'{n} {unit}' if unit == 'B' else f'{n:.1f} {unit}'
        n /= 1024


def progress_bar():
    start, last = time.time(), [0.0]

    def show(name, done, total):
        if done < total and time.time() - last[0] < 0.5:
            return
        last[0] = time.time()
        rate = done / max(time.time() - start, 1e-3) / 1024
        print(f'\r  {name}: {done}/{total} bytes, {rate:.0f} KB/s   ', end='' if done < total else '\n', flush=True)
    return show


def norm(path, cwd='/'):
    path = posixpath.normpath(posixpath.join(cwd, path or '.'))
    return '' if path in ('/', '.') else path.lstrip('/')


# ------ The command line

def do_ls(card, args, cwd):
    for name, is_dir, size, date in card.ls(norm(args[0] if args else '', cwd)):
        print(f'  {name + "/":<40} {"":>10}  {card_time_text(date)}' if is_dir else
              f'  {name:<40} {human(size):>10}  {card_time_text(date)}')


def do_tree(card, args, cwd):
    def walk(path, indent):
        for name, is_dir, size, _ in card.ls(path):
            print(f'{indent}{name}/' if is_dir else f'{indent}{name}  ({human(size)})')
            if is_dir:
                walk(posixpath.join(path, name), indent + '  ')
    walk(norm(args[0] if args else '', cwd), '  ')


def do_get(card, args, cwd):
    src = norm(args[0], cwd)
    dst = args[1] if len(args) > 1 else posixpath.basename(src) or '.'
    if os.path.isdir(dst):
        dst = os.path.join(dst, posixpath.basename(src))
    card.get(src, dst, progress_bar(), lambda s: None)


def do_put(card, args, cwd):
    src = args[0]
    dst = norm(args[1] if len(args) > 1 else os.path.basename(os.path.normpath(src)), cwd)
    st = card.stat(dst)
    if st and st[0] and not os.path.isdir(src):
        dst = posixpath.join(dst, os.path.basename(src))
    card.put(src, dst, progress_bar(), lambda s: None)


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


# ------ The window

def gui(port_arg):
    import tkinter as tk
    from tkinter import filedialog, messagebox, simpledialog, ttk

    root = tk.Tk()
    root.title('Carte SD du badge')
    root.geometry('1000x620')
    root.minsize(700, 400)

    jobs = queue.Queue()  # Work for the thread of the card: functions taking the card
    events = queue.Queue()  # Its results for the window: functions to call
    state = {'card': None, 'busy': False, 'pc': os.path.expanduser('~'), 'sd': ''}

    def worker():
        while True:
            job = jobs.get()
            try:
                job()
            except (SdError, OSError, serial.SerialException) as e:
                events.put(lambda e=e: (status.set(f'Erreur : {e}'), messagebox.showerror('Carte SD', str(e))))
            events.put(lambda: done())
    threading.Thread(target=worker, daemon=True).start()

    def run(job, text):
        if state['busy']:
            return
        state['busy'] = True
        status.set(text)
        root.config(cursor='watch')
        jobs.put(job)

    def done():
        state['busy'] = False
        root.config(cursor='')

    def poll():
        while not events.empty():
            events.get()()
        root.after(50, poll)

    def progress(name, d, t):
        events.put(lambda: (bar.config(maximum=max(t, 1), value=d),
                            status.set(f'{name} : {human(d)} / {human(t)}')))

    def log(text):
        events.put(lambda: status.set(text))

    # --- The bar of the badge
    top = ttk.Frame(root, padding=6)
    top.pack(fill='x')
    ttk.Label(top, text='Badge :').pack(side='left')
    port_var = tk.StringVar()
    ports = ttk.Combobox(top, textvariable=port_var, width=32, state='readonly')
    ports.pack(side='left', padx=4)
    free_var = tk.StringVar()

    def refresh_ports():
        found = badge_ports()
        ports['values'] = [label for _, label in found]
        if found and not port_var.get():
            wanted = [label for p, label in found if p == port_arg]
            port_var.set(wanted[0] if wanted else found[0][1])

    def connect():
        label = port_var.get()
        if not label:
            return
        port = label.split()[0]

        def job():
            if state['card']:
                state['card'].close()
                state['card'] = None
            card = Card(port)
            state['card'] = card
            free, total = card.df()
            entries = card.ls(state['sd'])
            events.put(lambda: (free_var.set(f'{human(free * 1024)} libres sur {human(total * 1024)}'),
                                show_sd(entries), status.set(f'Connecté à {port}, heure du PC envoyée')))
        run(job, f'Connexion à {port}...')

    ttk.Button(top, text='Rechercher', command=refresh_ports).pack(side='left')
    ttk.Button(top, text='Connecter', command=connect).pack(side='left', padx=4)
    ttk.Label(top, textvariable=free_var).pack(side='right')

    # --- The two lists
    panes = ttk.PanedWindow(root, orient='horizontal')
    panes.pack(fill='both', expand=True, padx=6)

    def make_list(title):
        frame = ttk.Frame(panes)
        head = ttk.Frame(frame)
        head.pack(fill='x')
        ttk.Label(head, text=title, font=('', 10, 'bold')).pack(side='left')
        path_var = tk.StringVar()
        ttk.Label(head, textvariable=path_var).pack(side='left', padx=8)
        tree = ttk.Treeview(frame, columns=('size', 'date'), selectmode='extended')
        tree.heading('#0', text='Nom')
        tree.heading('size', text='Taille')
        tree.heading('date', text='Date')
        tree.column('#0', width=240)
        tree.column('size', width=80, anchor='e')
        tree.column('date', width=120)
        scroll = ttk.Scrollbar(frame, orient='vertical', command=tree.yview)
        tree.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right', fill='y')
        tree.pack(fill='both', expand=True)
        panes.add(frame, weight=1)
        return tree, path_var

    pc_tree, pc_path = make_list('PC')
    sd_tree, sd_path = make_list('Carte SD')

    def fill(tree, rows, up):
        tree.delete(*tree.get_children())
        if up:
            tree.insert('', 'end', iid='..', text='📁 ..', values=('', ''))
        for name, is_dir, size, date in rows:
            tree.insert('', 'end', iid=name, text=('📁 ' if is_dir else '') + name,
                        values=('' if is_dir else human(size), date), tags=('dir',) if is_dir else ())

    def show_pc():
        path = state['pc']
        rows = []
        try:
            for name in os.listdir(path):
                full = os.path.join(path, name)
                try:
                    st = os.stat(full)
                except OSError:
                    continue
                rows.append((name, os.path.isdir(full), st.st_size,
                             time.strftime('%Y-%m-%d %H:%M', time.localtime(st.st_mtime))))
        except OSError as e:
            status.set(f'Erreur : {e}')
        rows.sort(key=lambda r: (not r[1], r[0].lower()))
        fill(pc_tree, rows, os.path.dirname(path) != path)
        pc_path.set(path)

    def show_sd(entries):
        fill(sd_tree, [(n, d, s, card_time_text(t)) for n, d, s, t in entries], bool(state['sd']))
        sd_path.set('/' + state['sd'])

    def reload_sd(then_text='Prêt'):
        def job():
            entries = state['card'].ls(state['sd'])
            free, total = state['card'].df()
            events.put(lambda: (show_sd(entries), status.set(then_text),
                                free_var.set(f'{human(free * 1024)} libres sur {human(total * 1024)}')))
        if state['card']:
            run(job, 'Lecture de la carte...')

    def pc_open(_event=None):
        sel = pc_tree.focus()
        if not sel:
            return
        path = os.path.dirname(state['pc']) if sel == '..' else os.path.join(state['pc'], sel)
        if os.path.isdir(path):
            state['pc'] = path
            show_pc()

    def sd_open(_event=None):
        sel = sd_tree.focus()
        if not sel or not state['card']:
            return
        if sel == '..':
            state['sd'] = posixpath.dirname(state['sd'])
        elif 'dir' in sd_tree.item(sel, 'tags'):
            state['sd'] = posixpath.join(state['sd'], sel)
        else:
            return
        reload_sd()

    pc_tree.bind('<Double-1>', pc_open)
    pc_tree.bind('<Return>', pc_open)
    sd_tree.bind('<Double-1>', sd_open)
    sd_tree.bind('<Return>', sd_open)

    def selected(tree):
        return [i for i in tree.selection() if i != '..']

    # --- The actions
    def send():
        names = selected(pc_tree)
        if not names or not state['card']:
            return
        srcs = [os.path.join(state['pc'], n) for n in names]
        dst_dir = state['sd']

        def job():
            for src in srcs:
                state['card'].put(src, posixpath.join(dst_dir, os.path.basename(src)), progress, log)
            entries = state['card'].ls(dst_dir)
            events.put(lambda: (show_sd(entries), status.set(f'{len(srcs)} élément(s) envoyé(s)')))
        run(job, 'Envoi...')

    def fetch():
        names = selected(sd_tree)
        if not names or not state['card']:
            return
        src_dir, dst_dir = state['sd'], state['pc']

        def job():
            for n in names:
                state['card'].get(posixpath.join(src_dir, n), os.path.join(dst_dir, n), progress, log)
            events.put(lambda: (show_pc(), status.set(f'{len(names)} élément(s) récupéré(s)')))
        run(job, 'Récupération...')

    def new_folder():
        if not state['card']:
            return
        name = simpledialog.askstring('Nouveau dossier', 'Nom du dossier :', parent=root)
        if name:
            path = posixpath.join(state['sd'], name)
            run(lambda: (state['card'].mkdir(path), reload_sd_now()), 'Création...')

    def reload_sd_now():
        entries = state['card'].ls(state['sd'])
        events.put(lambda: (show_sd(entries), status.set('Prêt')))

    def rename():
        names = selected(sd_tree)
        if len(names) != 1 or not state['card']:
            return
        new = simpledialog.askstring('Renommer', 'Nouveau nom :', initialvalue=names[0], parent=root)
        if new and new != names[0]:
            old, path = posixpath.join(state['sd'], names[0]), posixpath.join(state['sd'], new)
            run(lambda: (state['card'].mv(old, path), reload_sd_now()), 'Renommage...')

    def delete():
        names = selected(sd_tree)
        if not names or not state['card']:
            return
        if not messagebox.askyesno('Supprimer', f'Supprimer de la carte : {", ".join(names[:5])}'
                                   f'{"..." if len(names) > 5 else ""} (dossiers compris) ?'):
            return
        paths = [posixpath.join(state['sd'], n) for n in names]

        def job():
            for p in paths:
                log(f'Suppression de {p}...')
                state['card'].rm(p, True)
            reload_sd_now()
        run(job, 'Suppression...')

    def choose_pc_folder():
        path = filedialog.askdirectory(initialdir=state['pc'], parent=root)
        if path:
            state['pc'] = os.path.normpath(path)
            show_pc()

    actions = ttk.Frame(root, padding=6)
    actions.pack(fill='x')
    ttk.Button(actions, text='Dossier du PC...', command=choose_pc_folder).pack(side='left')
    ttk.Button(actions, text='Envoyer sur la carte →', command=send).pack(side='left', padx=(16, 4))
    ttk.Button(actions, text='← Récupérer sur le PC', command=fetch).pack(side='left')
    ttk.Button(actions, text='Supprimer', command=delete).pack(side='right')
    ttk.Button(actions, text='Renommer', command=rename).pack(side='right', padx=4)
    ttk.Button(actions, text='Nouveau dossier', command=new_folder).pack(side='right')
    ttk.Button(actions, text='Actualiser', command=lambda: (show_pc(), reload_sd())).pack(side='right', padx=4)

    bottom = ttk.Frame(root, padding=(6, 0, 6, 6))
    bottom.pack(fill='x')
    bar = ttk.Progressbar(bottom, length=200)
    bar.pack(side='right')
    status = tk.StringVar(value='Choisir le badge, puis Connecter (fermer badge_remote.py avant)')
    ttk.Label(bottom, textvariable=status, anchor='w').pack(side='left', fill='x', expand=True)

    def on_close():
        if state['card']:
            state['card'].close()
        root.destroy()
    root.protocol('WM_DELETE_WINDOW', on_close)

    refresh_ports()
    show_pc()
    poll()
    if port_var.get():
        root.after(200, connect)
    root.mainloop()


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument('--port', help='serial port (default: the first Raspberry Pi Pico found)')
    parser.add_argument('-v', '--verbose', action='store_true', help='show the log lines of the badge')
    parser.add_argument('-r', '--recursive', action='store_true', help='rm: a folder and its content')
    parser.add_argument('command', nargs='?', choices=list(COMMANDS) + ['shell'],
                        help='without a command: the window')
    parser.add_argument('args', nargs='*')
    a = parser.parse_args()
    if not a.command:
        gui(a.port)
        return
    port = a.port or find_port()
    if not port:
        sys.exit('no badge found: is it connected and switched on?')
    try:
        card = Card(port, a.verbose)
    except serial.SerialException as e:
        sys.exit(f'cannot open {port}: {e} (used by another application?)')
    except SdError as e:
        sys.exit(f'error: {e}')
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
