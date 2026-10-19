#!/bin/bash
# set -e

echo "=== Menunggu koneksi SSH ke 172.16.42.1... ==="
for i in $(seq 1 60); do
    if ssh -o ConnectTimeout=1 -o StrictHostKeyChecking=no symbiot@172.16.42.1 "echo 147147 | sudo -S id" >/dev/null 2>&1; then
        echo "=== Terhubung via SSH di detik ke-$i! Menerapkan perbaikan darurat... ==="
        break
    fi
    sleep 0.5
done

# 1. Hentikan service modemmanager & fogos-wifi seketika agar tidak memicu crash
ssh -o StrictHostKeyChecking=no symbiot@172.16.42.1 "echo 147147 | sudo -S rc-service modemmanager stop 2>/dev/null || true; echo 147147 | sudo -S rc-service fogos-wifi stop 2>/dev/null || true; echo 147147 | sudo -S killall -9 ModemManager.real 2>/dev/null || true"

# 2. Salin init scripts yang sudah diperbaiki secara bersih
scp -o StrictHostKeyChecking=no /home/princeofgiri/nura-fogos/device-motorola-fogos/modemmanager.initd symbiot@172.16.42.1:/tmp/modemmanager.initd
scp -o StrictHostKeyChecking=no /home/princeofgiri/nura-fogos/device-motorola-fogos/fogos-wifi.initd symbiot@172.16.42.1:/tmp/fogos-wifi.initd

ssh -o StrictHostKeyChecking=no symbiot@172.16.42.1 "echo 147147 | sudo -S cp /tmp/modemmanager.initd /etc/init.d/modemmanager && echo 147147 | sudo -S chmod +x /etc/init.d/modemmanager && echo 147147 | sudo -S cp /tmp/fogos-wifi.initd /etc/init.d/fogos-wifi && echo 147147 | sudo -S chmod +x /etc/init.d/fogos-wifi"

echo "=== Perbaikan init scripts sukses diterapkan! Memulai ulang service secara stabil... ==="
ssh -o StrictHostKeyChecking=no symbiot@172.16.42.1 "echo 147147 | sudo -S rc-service fogos-wifi start && echo 147147 | sudo -S rc-service modemmanager start"

echo "=== Sukses! Memeriksa status perangkat... ==="
ssh -o StrictHostKeyChecking=no symbiot@172.16.42.1 "uptime; nmcli device; mmcli -m any 2>&1 | head -n 20"
