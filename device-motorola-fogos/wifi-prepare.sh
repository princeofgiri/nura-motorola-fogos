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

# 2. Stock System & Vendor mounts (prefer device-mapper devices)
SLOT=$(grep -o 'androidboot.slot_suffix=_[ab]' /proc/cmdline 2>/dev/null | cut -d= -f2)
[ -z "$SLOT" ] && SLOT="_b"
SLOT_LETTER=$(echo "$SLOT" | tr -d '_')

mkdir -p /mnt/stock_system /mnt/stock_vendor /apex/com.android.runtime /run/fogos

if ! mountpoint -q /mnt/stock_system; then
    SYS_DEV="/dev/mapper/system_${SLOT_LETTER}"
    [ -b "$SYS_DEV" ] || SYS_DEV="/dev/mapper/system_a"
    if [ -b "$SYS_DEV" ]; then
        mount -t ext4 -o ro "$SYS_DEV" /mnt/stock_system 2>/dev/null || true
    fi
fi

if ! mountpoint -q /mnt/stock_vendor; then
    VND_DEV="/dev/mapper/vendor_${SLOT_LETTER}"
    [ -b "$VND_DEV" ] || VND_DEV="/dev/mapper/vendor_a"
    if [ -b "$VND_DEV" ]; then
        mount -t ext4 -o ro "$VND_DEV" /mnt/stock_vendor 2>/dev/null || true
    fi
fi

# Android runtime APEX payload
if ! mountpoint -q /apex/com.android.runtime; then
    APEX_IMG="/run/fogos/runtime.img"
    if [ ! -f "$APEX_IMG" ]; then
        unzip -p /mnt/stock_system/system/apex/com.android.runtime.apex apex_payload.img > "$APEX_IMG"
    fi
    mount -t ext4 -o ro "$APEX_IMG" /apex/com.android.runtime
fi

# 3. Android runtime linker & directory shims
mkdir -p /system/bin/bootstrap /system/lib64 /system/etc /vendor/bin /vendor/lib64 /vendor/etc
ln -sf /apex/com.android.runtime/bin/linker64 /system/bin/linker64
ln -sf /apex/com.android.runtime/bin/linker64 /system/bin/bootstrap/linker64

mount --bind /mnt/stock_system/system/lib64 /system/lib64 2>/dev/null || true
mount --bind /mnt/stock_system/system/etc /system/etc 2>/dev/null || true
mount --bind /mnt/stock_vendor/bin /vendor/bin 2>/dev/null || true
mount --bind /mnt/stock_vendor/lib64 /vendor/lib64 2>/dev/null || true
mount --bind /mnt/stock_vendor/etc /vendor/etc 2>/dev/null || true

# 4. Patch libbinder and vndservicemanager if needed
if [ -f /usr/lib/fogos-patch.py ]; then
    python3 /usr/lib/fogos-patch.py || true
fi

# 5. MAC address and persist config
mkdir -p /mnt/vendor/persist /data/vendor/wifi/sockets
chmod 777 /data/vendor/wifi /data/vendor/wifi/sockets 2>/dev/null || true

if [ ! -f /mnt/vendor/persist/wlan_mac.bin ]; then
    MAC0=$(grep -o 'androidboot.wifimacaddr=[^ ]*' /proc/cmdline | cut -d= -f2 | tr -d ':')
    [ -n "$MAC0" ] || MAC0="00FADE53F562"
    cat << MACEOD > /mnt/vendor/persist/wlan_mac.bin
Intf0MacAddress=$MAC0
Intf1MacAddress=00FADE53F563
MACEOD
    chmod 644 /mnt/vendor/persist/wlan_mac.bin
fi

# 6. Disable WLAN idle shutdown and RRM beacon reports to prevent assertion panic (must be BEFORE 'END' marker)
for ini in /vendor/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /vendor/firmware/wlan/qca_cld/WCNSS_mot_cfg.ini /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/WCNSS_mot_cfg.ini; do
    if [ -f "$ini" ]; then
        sed -i '/gInterfaceChangeWait/d' "$ini"
        sed -i '/gWlanAutoShutdown/d' "$ini"
        sed -i '/gRrmEnable/d' "$ini"
        if grep -q "^END" "$ini"; then
            sed -i '/^END/i gInterfaceChangeWait=0\ngWlanAutoShutdown=0\ngRrmEnable=0' "$ini"
        else
            echo "gInterfaceChangeWait=0" >> "$ini"
            echo "gWlanAutoShutdown=0" >> "$ini"
            echo "gRrmEnable=0" >> "$ini"
        fi
    fi
done

# 7. Fix Plasma Mobile SignalStrengthIndicator.qml duplicate GSM icon and qmldir resource override
QMLDIR="/usr/lib/qt6/qml/org/kde/plasma/private/mobileshell/qmldir"
[ -f "$QMLDIR" ] && sed -i 's/^prefer/#prefer/' "$QMLDIR" 2>/dev/null || true

QML_INDICATOR="/usr/lib/qt6/qml/org/kde/plasma/private/mobileshell/SignalStrengthIndicator.qml"
if [ -f "$QML_INDICATOR" ]; then
    sed -i 's/visible: (!isInternetIndicatorMobileData.*/visible: !isInternetIndicatorMobileData \&\& MobileShell.SignalStrengthInfo.showIndicator/g' "$QML_INDICATOR" 2>/dev/null || true
fi


