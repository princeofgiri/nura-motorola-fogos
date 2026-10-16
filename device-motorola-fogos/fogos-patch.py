#!/usr/bin/env python3
"""
Dynamic binary patcher for Motorola Moto G34 5G (fogos) Android shims.
Bypasses Android Bionic WaitForProperty and SELinux access checks in vendor daemons
running under native postmarketOS without Android property service / SELinux policies.
"""
import struct, os, subprocess

# 1. Patch libbinder.so
libbinder_src = '/mnt/stock_system/system/lib64/libbinder.so'
libbinder_dst = '/run/fogos/libbinder.so'

if os.path.exists(libbinder_src):
    with open(libbinder_src, 'rb') as f:
        data = bytearray(f.read())
    orig = struct.unpack('<I', data[0x99510:0x99514])[0]
    if orig == 0x36000400: # tbz w0, #0, 99590
        struct.pack_into('<I', data, 0x99510, 0x14000020) # b 99590
        with open(libbinder_dst, 'wb') as f:
            f.write(data)
        subprocess.run(['mount', '--bind', libbinder_dst, libbinder_src], check=False)
        print('libbinder.so patched and mounted')

# 2. Patch vndservicemanager
vndsm_src = '/mnt/stock_vendor/bin/vndservicemanager'
vndsm_dst = '/run/fogos/vndservicemanager'

if os.path.exists(vndsm_src):
    with open(vndsm_src, 'rb') as f:
        data = bytearray(f.read())
    orig = struct.unpack('<I', data[0x89c4:0x89c8])[0]
    if orig == 0xd10183ff: # sub sp, sp, #0x60
        struct.pack_into('<II', data, 0x89c4, 0x52800020, 0xd65f03c0) # mov w0, #1; ret
        with open(vndsm_dst, 'wb') as f:
            f.write(data)
        os.chmod(vndsm_dst, 0o755)
        subprocess.run(['mount', '--bind', vndsm_dst, vndsm_src], check=False)
        print('vndservicemanager patched and mounted')
