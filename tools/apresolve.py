#!/usr/bin/env python3
"""Resolve aarch64 apk deps from pmbootstrap's cached APKINDEX files."""
import tarfile, glob, re, sys, os

idx_dir = os.path.expanduser('~/.local/var/pmbootstrap/cache_http')
pkgs = {}   # name -> (version, [deps])
provides = {}  # token -> pkgname (for so: provision)

for f in glob.glob(os.path.join(idx_dir, 'APKINDEX_*')):
    try:
        tf = tarfile.open(f, 'r:gz')
        m = tf.getmember('APKINDEX')
        data = tf.extractfile(m).read().decode()
    except Exception:
        continue
    name = ver = None
    deps = []
    prov = []
    def flush():
        if name and ver and name not in pkgs:
            pkgs[name] = (ver, deps)
            for p in prov:
                provides.setdefault(p, name)
    for line in data.splitlines():
        if not line.strip():
            flush(); name = ver = None; deps = []; prov = []
        elif line.startswith('P:'):
            name = line[2:]
        elif line.startswith('V:'):
            ver = line[2:]
        elif line.startswith('D:'):
            deps = line[2:].split()
        elif line.startswith('p:'):
            for tok in line[2:].split():
                prov.append(tok.split('=')[0])
    flush()

installed = set(x.strip() for x in open('/tmp/phone-pkgs.txt') if x.strip())
# also treat cached apks as available
for f in glob.glob(os.path.expanduser('~/.local/var/pmbootstrap/cache_apk_aarch64/*.apk')):
    base = os.path.basename(f)
    m = re.match(r'(.+)-\d[^-]*-r\d+(\.[0-9a-f]+)?\.apk$', base)
    if m:
        installed.add(m.group(1))

def dep_name(d):
    d = d.strip()
    if not d or d.startswith('!') or d.startswith('/'):
        return None
    if d.startswith('so:'):
        lib = d[3:].split('=')[0]
        return provides.get('so:' + lib, provides.get(lib))
    return re.split(r'[<>=~]', d, 1)[0]

targets = sys.argv[1:]
need = {}
stack = list(targets)
seen = set()
while stack:
    t = stack.pop()
    if t in seen or t in installed:
        continue
    seen.add(t)
    if t not in pkgs:
        print(f'MISSING-FROM-INDEX: {t}')
        continue
    ver, deps = pkgs[t]
    need[t] = ver
    for d in deps:
        n = dep_name(d)
        if n and n not in seen and n not in installed:
            stack.append(n)

for n in sorted(need):
    print(f'{n}-{need[n]}.apk')
