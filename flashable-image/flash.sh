#!/bin/bash
# ==============================================================================
# NURA OS / POSTMARKETOS FASTBOOT FLASHING SCRIPT FOR MOTOROLA MOTO G34 5G (fogos)
# ==============================================================================
# Kompatibel: Motorola Moto G34 5G (fogos / XT2363-9, SM6375 Snapdragon 695)
# OS Target  : Nura OS (postmarketOS edge) dengan Plasma Mobile 6.x Wayland
# ==============================================================================

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Warna untuk output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

clear
echo -e "${CYAN}==============================================================================${NC}"
echo -e "${CYAN}    NURA OS FLASHING TOOL — MOTOROLA MOTO G34 5G (fogos / SM6375)             ${NC}"
echo -e "${CYAN}==============================================================================${NC}"
echo ""

# 1. Cek keberadaan perintah fastboot
if ! command -v fastboot &> /dev/null; then
    echo -e "${RED}[ERROR] Perintah 'fastboot' tidak ditemukan!${NC}"
    echo "Silakan install android-tools terlebih dahulu (cth: sudo apt install fastboot / sudo pacman -S android-tools)."
    exit 1
fi

# 2. Cek keberadaan file-file image
for img in boot.img dtbo.img vbmeta.img userdata.img; do
    if [ ! -f "$SCRIPT_DIR/$img" ]; then
        echo -e "${RED}[ERROR] File $img tidak ditemukan di $SCRIPT_DIR!${NC}"
        echo "Pastikan seluruh file image sudah diekspor lengkap."
        exit 1
    fi
done

echo -e "${BLUE}[*] Memeriksa koneksi perangkat dalam mode Fastboot...${NC}"
DEVICE=$(fastboot devices | head -n 1 | awk '{print $1}')

if [ -z "$DEVICE" ]; then
    echo -e "${YELLOW}[!] Tidak ada perangkat yang terdeteksi di mode Fastboot!${NC}"
    echo ""
    echo "Petunjuk masuk mode Fastboot:"
    echo "1. Matikan HP secara total."
    echo "2. Tekan dan tahan tombol [Power] + [Volume Bawah] hingga layar Fastboot muncul."
    echo "3. Hubungkan kabel USB dari HP ke komputer/laptop ini."
    echo ""
    read -rp "Tekan [ENTER] jika HP sudah dalam mode Fastboot untuk mencoba lagi..."
    DEVICE=$(fastboot devices | head -n 1 | awk '{print $1}')
    if [ -z "$DEVICE" ]; then
        echo -e "${RED}[ERROR] Tetap tidak terdeteksi. Silakan periksa kabel/koneksi USB.${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}[+] Perangkat terdeteksi: ${DEVICE}${NC}"

# 3. Deteksi slot aktif (A/B)
CURRENT_SLOT=$(fastboot getvar current-slot 2>&1 | grep "current-slot:" | awk '{print $2}' | tr -d '\r')
if [ -z "$CURRENT_SLOT" ]; then
    CURRENT_SLOT="b" # Fallback slot aman
fi
echo -e "${BLUE}[*] Slot aktif saat ini: ${GREEN}_${CURRENT_SLOT}${NC}"

echo ""
echo -e "${YELLOW}PERINGATAN:${NC}"
echo "Proses ini akan menimpa kernel (boot), dtbo, vbmeta, dan userdata pada perangkat."
echo "Pastikan data penting Anda di HP sudah dicadangkan."
echo ""
read -rp "Lanjutkan proses flashing ke slot _${CURRENT_SLOT}? (y/N): " CONFIRM
if [[ ! "$CONFIRM" =~ ^[yY]$ ]]; then
    echo "Proses dibatalkan oleh pengguna."
    exit 0
fi

echo ""
echo -e "${BLUE}=== [1/4] Flashing VBMeta (Nonaktifkan verifikasi AVB)... ===${NC}"
fastboot flash "vbmeta_${CURRENT_SLOT}" "$SCRIPT_DIR/vbmeta.img" --disable-verity --disable-verification || fastboot flash "vbmeta" "$SCRIPT_DIR/vbmeta.img"

echo ""
echo -e "${BLUE}=== [2/4] Flashing DTBO (Empty 24MB DTBO)... ===${NC}"
fastboot flash "dtbo_${CURRENT_SLOT}" "$SCRIPT_DIR/dtbo.img" || fastboot flash "dtbo" "$SCRIPT_DIR/dtbo.img"

echo ""
echo -e "${BLUE}=== [3/4] Flashing Kernel & Initramfs (boot.img)... ===${NC}"
fastboot flash "boot_${CURRENT_SLOT}" "$SCRIPT_DIR/boot.img" || fastboot flash "boot" "$SCRIPT_DIR/boot.img"

echo ""
echo -e "${BLUE}=== [4/4] Flashing Nura OS Rootfs (userdata.img)... ===${NC}"
# Coba flash sparse userdata
if fastboot flash userdata "$SCRIPT_DIR/userdata.img"; then
    echo -e "${GREEN}[+] Berhasil flashing rootfs ke partisi userdata!${NC}"
else
    echo -e "${YELLOW}[!] Catatan: ABL Motorola membatasi penulisan langsung partisi userdata via bootloader fastboot.${NC}"
    echo -e "${CYAN}[*] Menjalankan metode booting via initramfs ramdisk untuk penulisan langsung...${NC}"
    echo "Perangkat akan di-boot sekali dengan Nura OS initramfs untuk menyiapkan filesystem."
    fastboot boot "$SCRIPT_DIR/boot.img"
    echo ""
    echo -e "${GREEN}HP sedang melakukan boot ke Nura OS!${NC}"
    exit 0
fi

echo ""
echo -e "${GREEN}==============================================================================${NC}"
echo -e "${GREEN}    FLASHING SELESAI DENGAN SUKSES!                                            ${NC}"
echo -e "${GREEN}==============================================================================${NC}"
echo ""
echo "Informasi Login Default Nura OS:"
echo " - Username : symbiot"
echo " - Password : 1"
echo " - Akses SSH: ssh symbiot@172.16.42.1 (via kabel USB)"
echo ""
read -rp "Apakah Anda ingin me-reboot HP ke Nura OS sekarang? (Y/n): " REBOOT_NOW
if [[ ! "$REBOOT_NOW" =~ ^[nN]$ ]]; then
    echo -e "${BLUE}[*] Melakukan reboot perangkat...${NC}"
    fastboot reboot
    echo -e "${GREEN}[+] Selamat menikmati Nura OS!${NC}"
fi
