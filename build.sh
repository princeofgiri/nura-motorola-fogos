#!/bin/bash
# ==============================================================================
# NURA OS / POSTMARKETOS AUTOMATED BUILD SCRIPT
# Target Device: Motorola Moto G34 5G (codename: fogos / SM6375)
# ==============================================================================

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/flashable-image"

# Warna output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

clear
echo -e "${CYAN}==============================================================================${NC}"
echo -e "${CYAN}    NURA OS FULL BUILD SYSTEM — MOTOROLA MOTO G34 5G (fogos)                  ${NC}"
echo -e "${CYAN}==============================================================================${NC}"
echo ""

# 1. Cek dependensi host
echo -e "${BLUE}[1/6] Memeriksa dependensi host...${NC}"
MISSING_TOOLS=()
for tool in pmbootstrap img2simg avbtool; do
    if ! command -v "$tool" &>/dev/null; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
    echo -e "${RED}[ERROR] Alat-alat berikut belum terpasang di sistem host:${NC}"
    for t in "${MISSING_TOOLS[@]}"; do
        echo "  - $t"
    done
    echo ""
    echo "Instalasi dependensi:"
    echo "  - pmbootstrap : pip install --user pmbootstrap atau install via paket distro"
    echo "  - img2simg    : sudo apt install android-sdk-libsparse-utils / sudo pacman -S android-tools"
    echo "  - avbtool     : sudo apt install avbtool / pip install avbtool"
    exit 1
fi
echo -e "${GREEN}[+] Semua dependensi host tersedia!${NC}"

# 2. Sinkronisasi aports ke pmaports
echo ""
echo -e "${BLUE}[2/6] Menyinkronkan aports lokal ke pmaports downstream...${NC}"
PMAPORTS_DIR="$SCRIPT_DIR/pmaports"

if [ ! -d "$PMAPORTS_DIR" ]; then
    echo -e "${YELLOW}[!] Folder pmaports belum ada. Mengkloning pmaports resmi postmarketOS...${NC}"
    git clone --depth=1 https://gitlab.postmarketos.org/postmarketOS/pmaports.git "$PMAPORTS_DIR"
fi

DOWNSTREAM_DIR="$PMAPORTS_DIR/device/downstream"
mkdir -p "$DOWNSTREAM_DIR/device-motorola-fogos" "$DOWNSTREAM_DIR/linux-motorola-fogos" "$DOWNSTREAM_DIR/firmware-motorola-fogos"

cp -av "$SCRIPT_DIR/device-motorola-fogos/"* "$DOWNSTREAM_DIR/device-motorola-fogos/"
cp -av "$SCRIPT_DIR/linux-motorola-fogos/"* "$DOWNSTREAM_DIR/linux-motorola-fogos/"
cp -av "$SCRIPT_DIR/firmware-motorola-fogos/"* "$DOWNSTREAM_DIR/firmware-motorola-fogos/"

# Pastikan pmbootstrap mengarah ke pmaports ini
pmbootstrap config aports "$PMAPORTS_DIR"
pmbootstrap config device motorola-fogos
pmbootstrap config ui plasma-mobile
pmbootstrap config user symbiot

echo -e "${GREEN}[+] Aports berhasil disinkronkan!${NC}"

# 3. Update checksums
echo ""
echo -e "${BLUE}[3/6] Memperbarui checksums package...${NC}"
pmbootstrap checksum device-motorola-fogos
pmbootstrap checksum firmware-motorola-fogos
pmbootstrap checksum linux-motorola-fogos
echo -e "${GREEN}[+] Checksums selesai diperbarui!${NC}"

# 4. Bangun paket kernel, firmware, dan device
echo ""
echo -e "${BLUE}[4/6] Membangun paket APK lokal (kernel, firmware, device)...${NC}"
pmbootstrap build linux-motorola-fogos
pmbootstrap build firmware-motorola-fogos
pmbootstrap build device-motorola-fogos --force
echo -e "${GREEN}[+] Seluruh paket APK berhasil dikompilasi!${NC}"

# 5. Bangun rootfs & boot image
echo ""
echo -e "${BLUE}[5/6] Membangun Rootfs Plasma Mobile & Image Partisi...${NC}"
DEFAULT_APPS="plasma-dialer,spacebar,plasma-phonebook,kclock,kcalc,kweather,calindori,krecorder,qrca,plasma-camera,elisa,haruna,portfolio,dolphin,discover,alligator,oxygen-sounds,pulseaudio-utils,libcanberra-gtk3"

pmbootstrap install \
  --split \
  --sparse \
  --password 1 \
  --add "$DEFAULT_APPS"

echo -e "${GREEN}[+] Rootfs & Boot image sukses dibangun!${NC}"

# 6. Ekspor ke flashable-image
echo ""
echo -e "${BLUE}[6/6] Menyiapkan berkas flashable di $OUTPUT_DIR...${NC}"
mkdir -p "$OUTPUT_DIR"

WORK_DIR="$(pmbootstrap config work)"
ROOTFS_CHROOT="$WORK_DIR/chroot_rootfs_motorola-fogos"
NATIVE_ROOTFS="$WORK_DIR/chroot_native/home/pmos/rootfs"

# Salin boot.img dan dtbo.img
cp -v "$ROOTFS_CHROOT/boot/boot.img" "$OUTPUT_DIR/boot.img"
cp -v "$ROOTFS_CHROOT/boot/dtbo.img" "$OUTPUT_DIR/dtbo.img"

# Buat vbmeta.img dengan verification disabled
echo -e "${CYAN}[*] Menghasilkan vbmeta.img (AVB verification disabled)...${NC}"
avbtool make_vbmeta_image --flags 2 --padding_size 4096 --output "$OUTPUT_DIR/vbmeta.img"

# Konversi motorola-fogos-root.img ke Android sparse userdata.img
echo -e "${CYAN}[*] Mengonversi rootfs ke Android sparse userdata.img...${NC}"
img2simg "$NATIVE_ROOTFS/motorola-fogos-root.img" "$OUTPUT_DIR/userdata.img"

# Pastikan flash.sh dan README.md ada di output folder
cp -v "$SCRIPT_DIR/flashable-image/flash.sh" "$OUTPUT_DIR/flash.sh" 2>/dev/null || true
chmod +x "$OUTPUT_DIR/flash.sh"
ln -sf flashable-image/flash.sh "$SCRIPT_DIR/flash.sh"

echo ""
echo -e "${GREEN}==============================================================================${NC}"
echo -e "${GREEN}    BUILD SELESAI DENGAN SUKSES!                                               ${NC}"
echo -e "${GREEN}==============================================================================${NC}"
echo ""
echo "Berkas hasil build yang siap di-flash via fastboot berada di:"
echo -e "  📂 ${CYAN}$OUTPUT_DIR${NC}"
ls -lh "$OUTPUT_DIR"
echo ""
echo "Cara Flashing ke HP:"
echo -e "  1. Hubungkan HP dalam mode Fastboot (Power + Vol Bawah)."
echo -e "  2. Jalankan perintah: ${YELLOW}./flash.sh${NC} atau ${YELLOW}$OUTPUT_DIR/flash.sh${NC}"
echo ""
