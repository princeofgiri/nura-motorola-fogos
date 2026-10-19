#!/usr/bin/env python3
import struct, os, subprocess, shutil

# 1. libbinder.so patch
libbinder_src = '/mnt/stock_system/system/lib64/libbinder.so'
libbinder_dst = '/run/fogos/libbinder.so'
if os.path.exists(libbinder_src) and not os.path.exists(libbinder_dst):
    with open(libbinder_src, 'rb') as f:
        data = bytearray(f.read())
    orig = struct.unpack('<I', data[0x99510:0x99514])[0]
    if orig == 0x36000400:
        struct.pack_into('<I', data, 0x99510, 0x14000020)
        with open(libbinder_dst, 'wb') as f:
            f.write(data)
if os.path.exists(libbinder_dst):
    subprocess.run(['mount', '--bind', libbinder_dst, libbinder_src], check=False)

# 2. vndservicemanager patch
vndsm_src = '/mnt/stock_vendor/bin/vndservicemanager'
vndsm_dst = '/run/fogos/vndservicemanager'
if os.path.exists(vndsm_src) and not os.path.exists(vndsm_dst):
    with open(vndsm_src, 'rb') as f:
        data = bytearray(f.read())
    orig = struct.unpack('<I', data[0x89c4:0x89c8])[0]
    if orig == 0xd10183ff:
        struct.pack_into('<II', data, 0x89c4, 0x52800020, 0xd65f03c0)
        with open(vndsm_dst, 'wb') as f:
            f.write(data)
        os.chmod(vndsm_dst, 0o755)
if os.path.exists(vndsm_dst):
    subprocess.run(['mount', '--bind', vndsm_dst, vndsm_src], check=False)

# 3. libc.so patch (disable HandleUsingDestroyedMutex abort)
libc_src = '/apex/com.android.runtime/lib64/bionic/libc.so'
libc_dst = '/run/fogos/libc.so'
if os.path.exists(libc_src) and not os.path.exists(libc_dst):
    shutil.copyfile(libc_src, libc_dst)
    with open(libc_dst, 'r+b') as f:
        f.seek(0x8d940)
        f.write(bytes.fromhex('c0035fd6')) # ret
if os.path.exists(libc_dst):
    subprocess.run(['mount', '--bind', libc_dst, libc_src], check=False)

# 4. hwservicemanager patch
hwsm_src = '/system/system_ext/bin/hwservicemanager'
hwsm_dst = '/run/fogos/hwservicemanager'
if os.path.exists(hwsm_src) and not os.path.exists(hwsm_dst):
    shutil.copyfile(hwsm_src, hwsm_dst)
    with open(hwsm_dst, 'r+b') as f:
        for offset in [0xc118, 0xc1a4, 0xc3b8, 0xc408, 0xc9d8, 0xc9f4]:
            f.seek(offset)
            f.write(bytes.fromhex('1f2003d5'))
        f.seek(0xc9f8)
        f.write(bytes.fromhex('00008052c0035fd6')) # mov w0, #0; ret
    os.chmod(hwsm_dst, 0o755)
if os.path.exists(hwsm_dst):
    subprocess.run(['mount', '--bind', hwsm_dst, hwsm_src], check=False)

# 5. manifest.xml patch
manifest_src = '/mnt/stock_system/system/etc/vintf/manifest.xml'
manifest_dst = '/run/fogos/manifest.xml'
if os.path.exists(manifest_src) and not os.path.exists(manifest_dst):
    with open(manifest_src, 'r') as f:
        c = f.read()
    if 'android.hidl.manager' not in c:
        entry = '''    <hal format="hidl">
        <name>android.hidl.manager</name>
        <transport>hwbinder</transport>
        <fqname>@1.2::IServiceManager/default</fqname>
    </hal>
'''
        c = c.replace('</manifest>', entry + '</manifest>')
        with open(manifest_dst, 'w') as f:
            f.write(c)
if os.path.exists(manifest_dst):
    subprocess.run(['mount', '--bind', manifest_dst, manifest_src], check=False)

# 6. Radio directories
for d in ['/dev/socket/qmux_radio', '/dev/socket/qcrild', '/data/vendor/radio',
          '/data/vendor/misc/cutback', '/mnt/vendor/persist/mdm',
          '/data/vendor/connectivity', '/data/vendor/modem_config/mcfg_sw',
          '/data/vendor/modem_config/mcfg_hw', '/data/vendor/diag_logs']:
    os.makedirs(d, exist_ok=True)
    try:
        os.chmod(d, 0o777)
    except Exception:
        pass

# 7. Copy telkomsel mbn to mcfg_sw if present in /vendor/fsg
if os.path.exists('/vendor/fsg/telkomsel_idn.mbn'):
    fsg_dst = '/data/vendor/modem_config/mcfg_sw/telkomsel_idn.mbn'
    if not os.path.exists(fsg_dst):
        try:
            shutil.copyfile('/vendor/fsg/telkomsel_idn.mbn', fsg_dst)
        except Exception:
            pass

print('fogos-patch completed successfully')
