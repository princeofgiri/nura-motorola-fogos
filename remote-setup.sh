#!/bin/sh
# Setup + build port motorola-fogos di mesin baru (dijalankan via remote).
set -e
echo "=== branch pmaports: $(git -C "$HOME/nura-fogos/pmaports" rev-parse --abbrev-ref HEAD 2>/dev/null) $(git -C "$HOME/nura-fogos/pmaports" log --oneline -1 2>/dev/null)"
# 1. pmbootstrap
if [ ! -x "$HOME/pmbootstrap/pmbootstrap.py" ]; then
    git clone --depth 1 https://gitlab.postmarketOS.org/postmarketOS/pmbootstrap.git "$HOME/pmbootstrap"
fi
# 2. aport kanonis ke pmaports yang dipakai config
mkdir -p "$HOME/nura-fogos/pmaports/device/downstream"
cp -r "$HOME/nura-fogos/device-motorola-fogos" "$HOME/nura-fogos/linux-motorola-fogos" "$HOME/nura-fogos/firmware-motorola-fogos" "$HOME/nura-fogos/pmaports/device/downstream/"
# 3. config: openrc (systemd rusak di pmbootstrap 3.12 + pmaports baru).
# PENTING: harus di section [pmbootstrap]; append di akhir file mendarat di
# section [mirrors] dan DIABAIKAN (itulah yang dulu bikin rootfs systemd!).
if ! grep -q "^service_manager" "$HOME/.config/pmbootstrap_v3.cfg" 2>/dev/null; then
    sed -i '/^\[pmbootstrap\]/a service_manager = openrc' "$HOME/.config/pmbootstrap_v3.cfg"
fi
mkdir -p "$HOME/.local/var/pmbootstrap"
echo 8 > "$HOME/.local/var/pmbootstrap/version"
# 4. rantai build
"$HOME/pmbootstrap/pmbootstrap.py" checksum linux-motorola-fogos
"$HOME/pmbootstrap/pmbootstrap.py" checksum device-motorola-fogos
"$HOME/pmbootstrap/pmbootstrap.py" checksum firmware-motorola-fogos
"$HOME/pmbootstrap/pmbootstrap.py" --details-to-stdout build linux-motorola-fogos --arch aarch64
"$HOME/pmbootstrap/pmbootstrap.py" --details-to-stdout build device-motorola-fogos
"$HOME/pmbootstrap/pmbootstrap.py" --details-to-stdout build firmware-motorola-fogos
"$HOME/pmbootstrap/pmbootstrap.py" --details-to-stdout install --zap --password 147147
"$HOME/pmbootstrap/pmbootstrap.py" --details-to-stdout export
echo "REMOTE-CHAIN-DONE"
