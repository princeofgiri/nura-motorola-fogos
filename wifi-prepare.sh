#!/bin/sh
set -e

# 1. Mount binderfs if not mounted
if [ ! -d /dev/binderfs ]; then
    mkdir -p /dev/binderfs
    mount -t binder binder /dev/binderfs
    chmod 0755 /dev/binderfs
fi
for b in binder vndbinder hwbinder; do
    [ -e "/dev/$b" ] || ln -sf "/dev/binderfs/$b" "/dev/$b"
    [ -e "/dev/binderfs/$b" ] && chmod 0666 "/dev/binderfs/$b"
done

# 2. Super partition loop mounts
SUPER_DEV="/dev/disk/by-partlabel/super"
[ -e "$SUPER_DEV" ] || SUPER_DEV="/dev/sde25"

mkdir -p /mnt/stock_system /mnt/stock_vendor /mnt/stock_system_ext /apex/com.android.runtime /apex/com.android.i18n /run/fogos

# System (offset 3464288 sectors = 1773715456, size 2302248 sectors = 1178750976)
if ! mountpoint -q /mnt/stock_system; then
    LOOP_SYS=$(losetup -j "$SUPER_DEV" | grep "1773715456" | cut -d: -f1 | head -n1)
    if [ -z "$LOOP_SYS" ]; then
        LOOP_SYS=$(losetup -r -f -o 1773715456 --sizelimit 1178750976 "$SUPER_DEV" --show)
    fi
    mount -t ext4 -o ro "$LOOP_SYS" /mnt/stock_system
fi

# Vendor (offset 6929312 sectors = 3547807744, size 1455736 sectors = 745336832)
if ! mountpoint -q /mnt/stock_vendor; then
    LOOP_VND=$(losetup -j "$SUPER_DEV" | grep "3547807744" | cut -d: -f1 | head -n1)
    if [ -z "$LOOP_VND" ]; then
        LOOP_VND=$(losetup -r -f -o 3547807744 --sizelimit 745336832 "$SUPER_DEV" --show)
    fi
    mount -t ext4 -o ro "$LOOP_VND" /mnt/stock_vendor
fi

# System_ext (offset 5766536 sectors = 2952466432, size 1162776 sectors = 595341312)
if ! mountpoint -q /mnt/stock_system_ext; then
    LOOP_EXT=$(losetup -j "$SUPER_DEV" | grep "2952466432" | cut -d: -f1 | head -n1)
    if [ -z "$LOOP_EXT" ]; then
        LOOP_EXT=$(losetup -r -f -o 2952466432 --sizelimit 595341312 "$SUPER_DEV" --show)
    fi
    mount -t ext4 -o ro "$LOOP_EXT" /mnt/stock_system_ext
fi

# Android runtime APEX payload
if ! mountpoint -q /apex/com.android.runtime; then
    APEX_IMG="/run/fogos/runtime.img"
    if [ ! -f "$APEX_IMG" ]; then
        unzip -p /mnt/stock_system/system/apex/com.android.runtime.apex apex_payload.img > "$APEX_IMG"
    fi
    mount -t ext4 -o ro "$APEX_IMG" /apex/com.android.runtime
fi

# Android i18n APEX payload
if ! mountpoint -q /apex/com.android.i18n; then
    I18N_IMG="/run/fogos/i18n.img"
    if [ ! -f "$I18N_IMG" ]; then
        unzip -p /mnt/stock_system/system/apex/com.android.i18n.apex apex_payload.img > "$I18N_IMG"
    fi
    mount -t ext4 -o ro "$I18N_IMG" /apex/com.android.i18n
fi

# 3. Android runtime linker & directory shims
mkdir -p /system/bin/bootstrap /system/lib64 /system/etc /system/system_ext /vendor/bin /vendor/lib64 /vendor/etc
ln -sf /apex/com.android.runtime/bin/linker64 /system/bin/linker64
ln -sf /apex/com.android.runtime/bin/linker64 /system/bin/bootstrap/linker64

mount --bind /mnt/stock_system/system/lib64 /system/lib64 2>/dev/null || true
mount --bind /mnt/stock_system/system/etc /system/etc 2>/dev/null || true
mount --bind /mnt/stock_system_ext /system/system_ext 2>/dev/null || true
mount --bind /mnt/stock_vendor/bin /vendor/bin 2>/dev/null || true
mount --bind /mnt/stock_vendor/lib64 /vendor/lib64 2>/dev/null || true
mount --bind /mnt/stock_vendor/etc /vendor/etc 2>/dev/null || true

# 4. Patch binaries & runtime configs
if [ -f /usr/lib/fogos-patch.py ]; then
    python3 /usr/lib/fogos-patch.py
fi

# 5. MAC address and persist config
mkdir -p /mnt/vendor/persist /data/vendor/wifi/sockets
chmod 777 /data/vendor/wifi /data/vendor/wifi/sockets 2>/dev/null || true

if [ ! -f /mnt/vendor/persist/wlan_mac.bin ]; then
    MAC0=$(grep -o 'androidboot.wifimacaddr=[^ ]*' /proc/cmdline 2>/dev/null | cut -d= -f2 | tr -d ':' || true)
    [ -n "$MAC0" ] || MAC0="00FADE53F562"
    cat << MACEOD > /mnt/vendor/persist/wlan_mac.bin
Intf0MacAddress=$MAC0
Intf1MacAddress=00FADE53F563
MACEOD
    chmod 644 /mnt/vendor/persist/wlan_mac.bin
fi
