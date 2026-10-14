#!/usr/bin/env python3
"""Pull a block device from the pmOS initramfs debug shell over telnet as base64."""
import socket, time, re, base64, hashlib, sys

dev, outfile, expect_md5, expect_b64 = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])

s = socket.create_connection(('172.16.42.1', 23), timeout=15)
s.settimeout(10)
time.sleep(1)
s.recv(65536)
s.sendall(f'dd if={dev} bs=1M 2>/dev/null | base64\n'.encode())
time.sleep(3)
chunks = []
got = 0
t0 = time.time()
while time.time() - t0 < 600 and got < expect_b64:
    try:
        ch = s.recv(65536)
    except socket.timeout:
        continue
    if not ch:
        break
    chunks.append(ch)
    # count only plausible base64 bytes to track progress
    got += sum(1 for b in ch if b in b'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=')
    if got // (1 << 20) != (got - len(ch)) // (1 << 20):
        print(f'b64 MB: {got / 1048576:.1f}/{expect_b64 / 1048576:.1f}', flush=True)
raw = b''.join(chunks).decode(errors='replace')
s.close()
lines = [l for l in (x.strip() for x in raw.splitlines())
         if len(l) >= 4 and len(l) % 4 == 0 and re.fullmatch(r'[A-Za-z0-9+/=]+', l)]
data = base64.b64decode(''.join(lines))
open(outfile, 'wb').write(data)
md5 = hashlib.md5(data).hexdigest()
print(f'bytes: {len(data)} md5: {md5} expect {expect_md5} MATCH={md5 == expect_md5}')
