# HANDOVER — Porting Nura OS → Motorola Moto G34 5G (fogos)

Dokumen ini agar model/AI lain bisa langsung lanjut tanpa baca ulang semua log.
Ditulis 15 Okt 2026. Detail harian ada di `PROGRES.md` (bagian Indonesia).

## 1. TL;DR

- Target: Nura OS (= postmarketOS, tooling identik `pmbootstrap` + `pmaports`) di **Moto G34 5G (fogos, XT2363-9, SM6375 Snapdragon 695)**, kernel downstream 5.4 LineageOS.
- Status: **boot permanen + display + konsol + touch ✔** (terverifikasi di HP). Audio kartu terdaftar tapi belum bunyi. WiFi/BT/modem tertunda. Weston jalan tapi output tak sampai panel + kadang mati diam-diam.
- Kernel kanonis: **5.4.302-r10 sudah BUILD** (ada apk-nya) tapi **belum diinstal** — HP masih jalan **r8** (`#9-postmarketOS`). Langkah pertama sesi berikut: install r10 → reboot → uji trigger modem.
- HP **online saat dokumen ini ditulis** (uptime menit, `#9`, ping OK).

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

Kondisi: HP online, kernel **r8**, rootfs openrc, SSH OK. **r10 sudah build, BELUM diinstal.**
1. **Install r10** (`scp` apk r10 → `apk add --allow-untrusted` di HP — UUID-safe, tanpa reinstall) → reboot → verifikasi `uname -v #11`.
2. **Tes trigger modem**: `echo 1 > /sys/kernel/boot_adsp/boot` → `dmesg | grep -i pil` + `qrtr-lookup` (apakah modem PIL ikut naik via pon-depends-on).
3. **Tes rmmod ilitek aman** (seharusnya tak panic lagi berkat 0002) — opsional, hati-hati.
4. **Debug hang Weston/atomic**: `echo 0x1f > /sys/module/drm/parameters/debug`, stream `dmesg -w` ke laptop, start weston; catat fungsi macet. Fakta: atomic commit "complete" tapi panel tak update; legacy setcrtc (script `tmp/drmtest.py`) juga OK-tapi-tak-tampil; hanya tulis langsung ke fb0 (fbshow) yang tampil. Ramoops sudah dibesarkan di r9 (console 512K) untuk tangkap trace.
5. Audio: bangun daemon QRTR (`pd-mapper`/`qrtr-ns` — tak ada di Alpine; sumber `linux-msm/*`, github sempat diblokir) → q6core → sound → UCM/amp.
6. Upstream: repo sudah push; wiki + MR pmaports menyusul.

## 7. Perintah penting

```bash
~/nura-fogos/scripts/ssh-hp.sh "<cmd>"            # SSH ke HP (user symbiot)
~/pmbootstrap/pmbootstrap.py checksum/build/install/export  # alur build
fastboot boot /tmp/postmarketOS-export/boot.img   # tes aman
fastboot --cmdline "<cmdline + pmos.debug-shell>" boot boot.img  # paksa shell initramfs
python3 + socket TCP 172.16.42.1:23               # debug shell (tanpa nc)
```

## 8. Pertanyaan terbuka (belum terjawab)

- Fungsi mana yang macet saat modeset atomic weston (butuh trace `drm.debug`)?
- Apakah `pon-depends-on=modem` benar mem-boot modem (uji r10)?
- Dari mana mem-boot modem secara resmi (per_mgr/cnd stock) bila eksperimen gagal?
- Sumber `qrtr-ns`/`peripheral-manager` bila github tetap diblokir?
- WiFi attach menunggu modem, atau ada jalur HIF mandiri?
