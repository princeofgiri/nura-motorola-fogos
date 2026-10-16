# HANDOVER — Porting Nura OS → Motorola Moto G34 5G (fogos)

Dokumen ini agar model/AI lain bisa langsung lanjut tanpa baca ulang semua log.
Ditulis 15 Okt 2026. Detail harian ada di `PROGRES.md` (bagian Indonesia).

## 1. TL;DR

- Target: Nura OS (= postmarketOS, tooling identik `pmbootstrap` + `pmaports`) di **Moto G34 5G (fogos, XT2363-9, SM6375 Snapdragon 695)**, kernel downstream 5.4 LineageOS.
- Status: **boot permanen + display + Plasma Mobile GUI + touch 10-point multitouch OK ✔** (terverifikasi di HP).
- Kernel aktif: **5.4.302-r10 (`#11-postmarketOS`)** terpasang dan boot sempurna di slot `_b` (`qbootctl -m` aktif).
- GUI Plasma Mobile 6.7.5 berjalan di KWin Wayland menggunakan mode **Atomic KMS** (KWIN_DRM_DEVICES=/dev/dri/card0, tanpa KWIN_DRM_NO_AMS).
- Touchscreen ILITEK_TDDI aktif via `/lib/firmware/ILITEK_FW` + eudev-libs + udev 0666 + libinput quirk. Lockscreen & wizard teruji responsif.
- Display disetel ke 60Hz (`720x1600@60Hz`, scale 2.0) di `kwinoutputconfig.json`.
- Flashing rootfs via `tools/flash_rootfs.py` langsung ke block device `/dev/sde26` (userdata) dan `/dev/sdf18` (boot_b).

## 2. Perangkat, mesin, akses

- HP: `ZP22228PHC`, slot aktif `_b` (pernah flip ke `_a`; cek selalu via `fastboot getvar current-slot`), panel TXD/ILI9883C 720x1600, touch ilitek (dual-source chipone/ilitek, unit ini ilitek).
- Laptop ini (`/home/princeofgiri`): `~/pmbootstrap/pmbootstrap.py` v3.12.0, `~/.config/pmbootstrap_v3.cfg` (device motorola-fogos, edge, console, openrc — lihat jebakan §6.5).
- HP di USB-networking `172.16.42.1`, user `symbiot`, password `147147`.
- **JANGAN pakai `nc`** (tidak ada). Pola akses:
  - Shell cepat: `~/nura-fogos/scripts/ssh-hp.sh "<cmd>"` (SSH + askpass, sudah dipindah dari `/tmp` ke repo agar tahan reboot; `/tmp/ask.sh` lawas jangan dipakai lagi).
  - Root di HP: `echo 147147 | sudo -S <cmd>` (stdin pipe, bukan `-S` interaktif).
  - Debug shell initramfs: TCP 172.16.42.1:23 via `python3` + `socket` (lihat `PROGRES.md` §9 / `tmp/pullblk.py` polanya).
  - `fastboot devices` kosong = HP sudah boot normal (bukan masalah).

## 3. Layout repo (jangan tertukar!)

- `~/nura-fogos/` = workdir kanonis: `device-motorola-fogos/`, `linux-motorola-fogos/` (APKBUILD pkgrel=10, config, `0001-*.patch`, `0002-*.patch`), `firmware-motorola-fogos/`, `local-weston/`, `tmp/` (tools + artefak), `pmaports/` (checkout upstream + overlay `device/downstream/*fogos*`), `remote-setup.sh`, `PROGRES.md`, `scripts/ssh-hp.sh`, `scripts/ask.sh`.
- `~/nura-motorola-fogos/` = repo GitHub (`origin git@github.com:princeofgiri/nura-motorola-fogos.git`, branch `main`, **sudah push sinkron**): aport + README + LICENSE MIT + PROGRES.md + tools + `scripts/remote-setup.sh` (repo-relative) + `local-weston/`.
- `pmbootstrap` build dari `pmaports/device/downstream/*fogos*` — **sinkronkan kanonis → sana sebelum build** (`cp`), lalu `checksum` bila source berubah.
- APK hasil: `~/.local/var/pmbootstrap/packages/edge/aarch64/` (`linux-motorola-fogos-5.4.302-r{0,1,5,6,7,8,9,10}.apk`, weston r1/r2, firmware, device).

## 4. Yang sudah beres (dengan bukti)

- **M1 boot**: `fastboot boot` diterima ABL; USB-net + debug shell OK.
- **M2 display**: `card0`, `card0-DSI-1 connected`, `panel0-backlight`, `/dev/fb0`, tes noise tampil.
- **M3 konsol**: `Console: switching to colour frame buffer device`, vtcon fb `bind=1`, teks + `Welcome to Nura!` + login TERLIHAT di layar. `CONFIG_VT/VT_CONSOLE/FB/FBCON/DRM_FBDEV_EMULATION` dinyalakan (Lineage mematikan).
- **Touch SEMPURNA**: ilitek butuh `vdd/vcc-supply = <&L9A>` (r7, DT merge) DAN urutan `modprobe chipone` → sleep 5 → `modprobe ilitek` (chipone menolak via panel-check tapi "menyentuh bus"); autoload via `/etc/local.d/touchscreen.start`; `event2`/`input2 ILITEK_TDDI`; area aktif X 130..2731, Y 153..6235; `evtest` + `tools/touchdraw.py` akurat (di `/usr/local/bin` di HP juga).
- **Flash permanen**: `vbmeta_b` (avbtool `--flags 2`, manual — avbtool tak ada di chroot), `dtbo_b` kosong 24M, `boot_b`, rootfs openrc di userdata `/dev/sde26` ext4 `pmOS_root` 226G. `fastboot flash userdata` DITOLAK ABL → jalur: `fastboot boot` initramfs → `setup_usb_storage_configfs /dev/sde26` → mkfs + `tune2fs -U <pmos_root_uuid DARI cmdline install terakhir!>` + extract tarball + buat `dev proc sys tmp mnt dev/pts dev/shm` + komentari `/boot` di fstab. Backup stock slot-B di `tmp/stock-backup/` (md5 cocok); slot A dibiarkan Android. Stock zip 4GB korup — jangan andalkan.
- **Firmware 2-r0**: 7 blob TheMuppets terpin (`d11da22` + `450f268`).
- **Weston lokal**: aport `local-weston/` + 2 patch (`tolerate-duplicate-formats`, `...-modifiers`) — build OK, terdeploy r2; assert lewat, DSI-1 720x1600@120/90/60 terinisiasi, desktop-shell+keyboard+terminal jalan.
- **Splash 720x1600 1:1 tampil** via `tools/fbshow.py` (pitch via `FBIOGET_FSCREENINFO` = **2944** bukan 2880; byte order B,G,R,0). Plymounth tema `pmos-spin` terpasang.
- **Audio**: kartu `holi-9370_fs1815-snd-card` pernah terdaftar manual (`aplay -l` tampil), ADSP boot via `/sys/kernel/boot_adsp/boot`, ACDB disalin ke target/, autostart audio+rmtfs di `/etc/local.d/`.
- **WiFi/BT inventaris**: `wlan.ko` memuat `QCA6750 + hif_snoc + __icnss_register_driver`; FW `wlanmdsp.mbn` + `bdwlan.*` hadir; `icnss2` bound; `/dev/wlan` ada tapi `echo ON` macet di `hdd_driver_load` (dugaan ikut power modem). BT: `ttyHS0`, `bt_wcn3990→bt_power`, `BT_HCIUART_QCA=y`, FW `apbtfw*.tlv` ada.
- **Modem**: `modem.mbn/.mdt` + `ipa_fws.*` tersaji; MSS bound `subsys-pil-tz`; `rmtfs -r -P` jalan (butuh r8 `CONFIG_QCOM_RMTFS_MEM`; tanpa `-s` karena remoteproc-sysfs tak ada); qrtr/tqftpserv/soc-qcom-modem terpasang.

## 5. Keputusan teknis yang mengikat

1. Boot header v3 → **v2**; `earlycon` wajib di cmdline (kalau tidak ABL reboot).
2. DTB merge saat compile → `blair-moto-fogos-pmos.dtb` (`prepare()` heredoc + sed Makefile); hapus node `qcom,battery-data` placeholder dulu (label ganda); EVT vs DVT1 identik kecuali board-id; r7 tambah supply ilitek; **r9 tambah `qcom,pon-depends-on = "modem"` di `pil_lpass`** (eksperimen trigger modem — mekanisme `__subsystem_get()` boot dependensi dulu).
3. BusyBox chroot: `dirname` 1 argumen; `patch` tak bisa bikin file baru + sensitif hunk konteks kosong → patch SELALU generate via `git diff` dari tree teredit + verifikasi `patch --dry-run` di salinan bersih SEBELUM commit (pelajaran mahal di 0002).
4. Kernel clang/LLVM, `modules_install` + strip (`/usr/lib/modules`), `dtbs_install` + install DTB merge eksplisit, `KBUILD_BUILD_VERSION="$((pkgrel+1))-postmarketOS"` (uname `-v #N` = pkgrel+1: r8 → `#9`).
5. `service_manager = openrc` HARUS di section `[pmbootstrap]` (pernah mendarat di `[mirrors]` → rootfs systemd rusak). `remote-setup.sh` sudah diperbaiki (insert di bawah `[pmbootstrap]`).
6. **DILARANG `rmmod` driver touch** (ilitek/chipone tak unregister charger-notifier → UAF → panic `CFI failure` di `power_supply_changed_work`, terbukti via pstore). Fix permanen = `0002-*.patch` (r10, flag + unreg + teardown wq).
7. **JANGAN tekan tombol power saat testing** (mayoritas "reboot misterius" adalah intervensi manual). Reboot spontan nyata: watchdog (~10 menitan saat idle/berat) + 1 panic CFI di atas.
8. Initramfs tak auto-load modul (`modprobe` manual / `modules-initfs` + `modules-load.d` / `local.d`); udev coldplug gagal path `/lib` vs `/usr/lib` untuk sebagian DLKM audio.
9. `deviceinfo_super_partitions="/dev/disk/by-partlabel/super"` (=sde25); device v3-r2.
10. `deviceinfo_getty ttyMSM0` tapi driver serial mati → komentar baris `ttyMSM0` di `/etc/inittab` (sementara).

## 6. Status kini + langkah berikut (berurutan)
 
Kondisi: HP online, kernel **r10 (#11-postmarketOS)**, Plasma Mobile 6.7.5 GUI aktif + touch OK + **Wi-Fi wlan0 aktif & dual-band scan 100% OK**.
1. **Mobile Data (rmnet_ipa0)**: registrasi jaringan seluler dan koneksi data 4G/5G.
2. **Audio internal (speaker/mic)**: rute audio via ADSP/HiFi4, UCM config, amp aw882xx/fs1815.
3. **Bluetooth userspace**: BlueZ di `/dev/ttyHS0` (`bt_wcn3990`).
4. **Hardware GPU 3D acceleration**: eksplorasi Turnip KGSL backend atau Zink di atas KGSL agar render tidak 100% di CPU llvmpipe.
5. **Upstream MR**: rapikan aport `device-motorola-fogos` dan ajukan MR ke pmaports.

## 7. Perintah penting

```bash
~/nura-fogos/scripts/ssh-hp.sh "<cmd>"            # SSH ke HP (user symbiot)
~/pmbootstrap/pmbootstrap.py checksum/build/install/export  # alur build
fastboot boot /tmp/postmarketOS-export/boot.img   # tes aman
fastboot --cmdline "<cmdline + pmos.debug-shell>" boot boot.img  # paksa shell initramfs
python3 + socket TCP 172.16.42.1:23               # debug shell (tanpa nc)
```

## 8. Catatan Arsitektur Wi-Fi WCN3990 di Nura OS / postmarketOS

- Qualcomm downstream `wlan.ko` (`qcacld-3.0`) + `icnss2` pada SM6375 membutuhkan userspace daemon Android (`cnss-daemon`) untuk menyelesaikan handshake QMI sebelum WLAN firmware menerbitkan event `FW_READY`.
- Partisi stock vendor dan stock system di-mount secara read-only dari partisi `super` (`/dev/sde25`).
- Android runtime APEX diekstrak untuk menyediakan Bionic libc dan dynamic linker 64-bit (`linker64`).
- Android `binderfs` dimount di `/dev/binderfs` dengan node `/dev/binder` dan `/dev/vndbinder`.
- Bionic WaitForProperty and SELinux checks pada daemon vendor di-bypass secara dinamis via binary patch minimal (4 byte di `libbinder.so` dan 8 byte di `vndservicemanager`).
- Stack daemon vendor (`vndservicemanager`, `pm-service`, `pm-proxy`, `cnss-daemon`) dikelola secara native via OpenRC service `/etc/init.d/fogos-wifi`.
- `wlan0` terdaftar secara native di kernel dan NetworkManager, mendukung pemindaian dual-band 2.4GHz dan 5GHz (1170 Mbit/s, 802.11ac).

