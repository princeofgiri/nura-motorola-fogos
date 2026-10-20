#!/bin/sh
# Motorola Moto G34 5G (fogos) ModemManager & SIM Provisioning Wrapper
# Run by OpenRC modemmanager service as the supervised daemon

# 1. Setup IPA sysfs shim for ModemManager
mkdir -p /tmp/mm_qmapmux_sysfs
[ -e /tmp/mm_qmapmux_sysfs/lower_rmnet_ipa0 ] || ln -sf /sys/devices/virtual/net/rmnet_ipa0 /tmp/mm_qmapmux_sysfs/lower_rmnet_ipa0

# 2. Initialize IPA WAN pipes once rmnet_ipa0 is registered by kernel
for i in $(seq 1 45); do
    if [ -x /usr/bin/rmnet-setup ] && [ -d /sys/class/net/rmnet_ipa0 ]; then
        /usr/bin/rmnet-setup rmnet_ipa0 >/dev/null 2>&1 || true
        break
    fi
    sleep 1
done

# 3. Wait for QRTR node 0 and modem QMI responsiveness, bring baseband online
for i in $(seq 1 60); do
    if /usr/sbin/qmicli -d qrtr://0 --dms-get-operating-mode >/dev/null 2>&1; then
        /usr/sbin/qmicli -d qrtr://0 --dms-set-operating-mode=online >/dev/null 2>&1 || true
        break
    fi
    sleep 1
done

# 4. Synchronous SIM card provisioning loop
PROVISIONED=0
for i in $(seq 1 60); do
    status=$(/usr/sbin/qmicli -d qrtr://0 --uim-get-card-status 2>/dev/null)
    if echo "$status" | grep -q "Application state: 'ready'"; then
        /usr/sbin/qmicli -d qrtr://0 --dms-set-operating-mode=online >/dev/null 2>&1 || true
        break
    elif [ "$PROVISIONED" -eq 0 ] && echo "$status" | grep -q "Application state: 'detected'"; then
        AID=$(echo "$status" | awk '/Application type:.*usim/{found=1} found && /Application ID:/{getline; gsub(/[: \t\r\n]/,""); print $1; exit}')
        [ -z "$AID" ] && AID=$(echo "$status" | awk '/Application ID:/{getline; gsub(/[: \t\r\n]/,""); print $1; exit}')
        [ -z "$AID" ] && AID="A0000000871002F230FF018907020000"
        /usr/sbin/qmicli -d qrtr://0 --uim-change-provisioning-session="session-type=primary-gw-provisioning,activate=yes,slot=1,aid=$AID" >/dev/null 2>&1 || true
        /usr/sbin/qmicli -d qrtr://0 --dms-set-operating-mode=online >/dev/null 2>&1 || true
        PROVISIONED=1
    fi
    sleep 1
done

/usr/sbin/qmicli -d qrtr://0 --dms-set-operating-mode=online >/dev/null 2>&1 || true

# 5. Launch real ModemManager binary with slot fix shim
REAL_MM="/usr/sbin/ModemManager.real"
[ -x "$REAL_MM" ] || REAL_MM="/usr/sbin/ModemManager"

exec env LD_PRELOAD=/usr/lib/libmm-slot-fix.so "$REAL_MM" --debug "$@"
