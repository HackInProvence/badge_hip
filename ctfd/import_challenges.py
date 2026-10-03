#!/usr/bin/env python3
# badge_secsea © 2025 by Hack In Provence is licensed under
# Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
# To view a copy of this license,
# visit https://creativecommons.org/licenses/by-nc-sa/4.0/
"""
Create the CTFd challenges and their flags from the single source tools/ctf_flags.py.

Static scoring ("sans décote") : every challenge is of type "standard" (fixed points, no depreciation).

Prerequisites: CTFd running (docker compose -f ctfd/docker-compose.yml up -d), the first-run setup done, and an
admin access token (CTFd: Settings > Access Tokens). No extra Python package needed (urllib only).

    CTFD_URL=http://localhost:8000 CTFD_TOKEN=ctfd_xxx python ctfd/import_challenges.py
"""

import argparse
import json
import os
import sys
import urllib.error
import urllib.request

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
import ctf_flags  # noqa: E402


def api(url, token, path, method='GET', data=None):
    req = urllib.request.Request(
        url.rstrip('/') + path, method=method,
        headers={'Authorization': f'Token {token}', 'Content-Type': 'application/json'})
    body = json.dumps(data).encode() if data is not None else None
    try:
        with urllib.request.urlopen(req, body) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        sys.exit(f'HTTP {e.code} on {method} {path}: {e.read().decode(errors="replace")[:300]}')
    except urllib.error.URLError as e:
        sys.exit(f'cannot reach {url} ({e.reason}): is CTFd running and set up?')


def main():
    ap = argparse.ArgumentParser(description='Import the badge CTF challenges into CTFd (static scoring)')
    ap.add_argument('--url', default=os.environ.get('CTFD_URL', 'http://localhost:8000'))
    ap.add_argument('--token', default=os.environ.get('CTFD_TOKEN'))
    ap.add_argument('--value', type=int, default=100, help='points per challenge (static)')
    args = ap.parse_args()
    if not args.token:
        sys.exit('set --token or CTFD_TOKEN (CTFd: Settings > Access Tokens)')

    existing = {c['name'] for c in api(args.url, args.token, '/api/v1/challenges?view=admin')['data']}
    created = 0
    for ch in ctf_flags.ctfd_challenges():
        if ch['name'] in existing:
            print('skip (exists):', ch['name'])
            continue
        r = api(args.url, args.token, '/api/v1/challenges', 'POST', dict(
            name=ch['name'], category=ch['category'], description=ch['description'],
            value=args.value, type='standard', state='visible'))
        cid = r['data']['id']
        api(args.url, args.token, '/api/v1/flags', 'POST', dict(
            challenge=cid, content=ch['flag'], type='static'))
        print('created:', ch['name'], '->', ch['flag'])
        created += 1
    print(f'\n{created} challenge(s) created, {len(existing)} already there.')


if __name__ == '__main__':
    main()
