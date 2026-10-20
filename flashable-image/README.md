# Nura OS Flashable Images — Motorola Moto G34 5G (fogos)

Folder ini berisi file image dan skrip otomatis untuk melakukan instalasi / flashing Nura OS (postmarketOS edge dengan antarmuka KDE Plasma Mobile) pada perangkat **Motorola Moto G34 5G** (codename: `fogos`, model: XT2363-9, SoC: Qualcomm Snapdragon 695 / SM6375).

---

## 📦 Berkas Image yang Tersedia

| File | Ukuran | Deskripsi |
| :--- | :--- | :--- |
| **`boot.img`** | ~49 MB | Kernel Linux downstream 5.4 + pmOS initramfs (Header v2, pagesize 4096) |
| **`dtbo.img`** | 24 MB | Empty DTBO image khusus untuk ABL Motorola (karena DTB sudah di-merge ke kernel) |
| **`vbmeta.img`** | 4 KB | Android Verified Boot (AVB) metadata dengan bendera verifikasi dinonaktifkan (`--flags 2`) |
| **`userdata.img`** | ~3.2 GB | Rootfs ext4 Nura OS Plasma Mobile (format Android sparse image siap flash) |
| **`flash.sh`** | - | Skrip otomatis interaktif untuk menjalankan flashing via fastboot |

---

## 🚀 Cara Flashing (Langkah Cepat)

### Prasyarat
1. Komputer / laptop Linux dengan paket `android-tools` / `fastboot` terpasang.
2. Kabel USB Type-C yang bagus.
3. Bootloader Motorola Moto G34 5G sudah di-unlock (*unlocked bootloader*).

### Langkah-langkah
1. Masuk ke mode **Fastboot** pada HP:
   - Matikan HP sepenuhnya.
   - Tekan dan tahan tombol **Power + Volume Bawah** secara bersamaan hingga masuk ke layar menu Fastboot.
2. Hubungkan HP ke komputer dengan kabel USB.
3. Buka terminal di folder ini dan jalankan:
   ```bash
   ./flash.sh
   ```
4. Ikuti petunjuk di layar. Skrip akan secara otomatis:
   - Mendeteksi serial perangkat Anda.
   - Memeriksa slot aktif saat ini (`_a` atau `_b`).
   - Melakukan flashing `vbmeta`, `dtbo`, `boot`, dan `userdata`.
   - Menanyakan apakah Anda ingin me-reboot perangkat langsung ke Nura OS.

---

## 🔑 Informasi Akun & Akses Sistem

Setelah boot ke Nura OS:
- **User Default**: `symbiot`
- **Password Default**: `1`
- **Akses SSH via USB**:
  ```bash
  ssh symbiot@172.16.42.1
  ```
- **Password Sudo**: `1`

---

## 🛠️ Fitur Bawaan yang Sudah Aktif
- **Display & UI**: Layar 60Hz 720x1600 dengan Wayland KWin Atomic KMS dan Plasma Mobile 6.x.
- **Touchscreen**: 10-point multi-touch responsif (ILITEK TDDI).
- **Konektivitas**: Wi-Fi dual-band (2.4 GHz & 5 GHz) via wlan0, Seluler 4G/5G (ModemManager & rmnet_ipa0).
- **Telepon & SMS**: Dialer seluler (telepon masuk & keluar), SMS (Spacebar), banner notifikasi, dan nada dering Oxygen.
- **Audio**: Speaker stereo, headphone, pemutaran media & notifikasi suara.
- **Suite Aplikasi**: Plasma Dialer, Spacebar, Phonebook, Clock, Calculator, Weather, Calendar, Recorder, Camera, Elisa, Haruna, Portfolio, Dolphin, Discover, Alligator.
