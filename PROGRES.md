# Progres Porting Motorola Fogos → Nura OS / postmarketOS

Perangkat: **Motorola Moto G34 5G (fogos, XT2363-9)** — SoC SM6375 Snapdragon 695,
bootloader unlocked (`flashing_unlocked`), slot aktif `_b`.
Target: Nura OS (= nama baru postmarketOS; tooling identik: pmbootstrap + pmaports).
Jalur kernel: **downstream 5.4** dari LineageOS (`android_kernel_motorola_sm6375`, branch `lineage-24.0`).

## Status milestone

- [x] **M1 — Boot**: `fastboot boot` diterima ABL, kernel 5.4.302-moto jalan,
      USB-networking + debug shell via telnet `172.16.42.1` (user `user`, initramfs).
- [x] **M2 — Display**: `card0` + konektor `card0-DSI-1` (`connected`, 720x1600@120/90/60),
      `panel0-backlight` nyala, `/dev/fb0` ada, **tes noise ke framebuffer tampil di layar**.
- [x] **M3 — Konsol teks di layar (sisi kernel)**: `Console: switching to colour
      frame buffer device 90x100`, `fb0: msm_drmdrmfb`, vtcon framebuffer `bind=1`,
      fb0 720x1600x32 (tes 13 Okt, r5). Tinggal konfirmasi visual di layar HP.
- [x] **Touchscreen ✔ SEMPURNA (13 Okt)**: driver ilitek butuh `vdd/vcc-supply`
  → kernel r7 (+supply `&L9A` di DT merge; chipone tanpa regulator-consumer,
  nodenya disentuh); probe ilitek hanya sukses bila chipone di-modprobe dulu
  (urutan deterministik via `/etc/local.d/touchscreen.start`, auto tiap boot →
  `input2`/`event2` hadir sendiri); `evtest` + visualizer `touchdraw.py`
  (kini di `/usr/local/bin` di HP + kanonis di `tmp/`) membuktikan sentuh+
  geser akurat setelah 3 bug tool diperbaiki (duplikat ST+MT, buffer event
  terbelah, `mtx = mty = {}` satu dict!). Kalibrasi: area aktif X 130..2731,
  Y 153..6235.
- [ ] Berikutnya: audio, WiFi/BT, modem (IPA), kamera, flash permanen rootfs.

## File port (kanonis, di `~/nura-fogos/`)

- `device-motorola-fogos/` (pkgver=3-r1): `deviceinfo` (header v2, pagesize 4096,
  dtb `vendor/qcom/blair-moto-fogos-pmos` + `append_dtb` + `flash_kernel_on_update`,
  `drm`, `getty` ttyMSM0, layar 720x1600, `boot_filesystem` fat32, cmdline qcom +
  `androidboot.hab.product=fogos`, dtbo kosong 25165824 byte), `kernel-cmdline.conf`
  (`earlycon`, HACK wajib ABL), `modules-initfs` (touchscreen `chipone_tddi_v2_mmi`
  + `ilitek_v3_mmi`, dual-source panel; otomatis jadi `initramfs.load` via devicepkg).
- `linux-motorola-fogos/`: `APKBUILD` (5.4.302-r5, source LineageOS commit `c63a2b5a`,
  pola `linux-xiaomi-monet` + clang/LLVM), `config-motorola-fogos.aarch64` (862 opsi:
  gabungan `holi-qgki_defconfig` + `lineage_moto-holi.config` + `moto-holi-fogos.config`
  + delta pmOS), `0001-*.patch` (fix kamera, lihat bawah).
- `firmware-motorola-fogos/` (pkgver=2-r0): 7 blob dari TheMuppets
  (`d11da22` fogos + `450f268` sm6375-common, V1UGS35H.75-14-3-10):
  NFC `sec_s3nrn4v`, touch `chipone`/`ILITEK`, amp `aw963xx`/`fs1815`,
  GPU `a619_gmu`/`a630_sqe`. WiFi/BT **tidak** termasuk (lihat rencana).
- `fogos-boot-test.img`: boot.img siap `fastboot boot` (di-refresh tiap build sukses;
  saat ini r5 + initramfs touch, 51M).
- `remote-setup.sh`: skrip setup+build penuh untuk mesin baru via SSH.

## Temuan teknis (keputusan yang mengikat)

1. Boot header stock v3 → turunkan ke **v2** (seperti dubai/rhodep/monet) agar mkbootimg/ABL mau.
2. `earlycon` di cmdline wajib, kalau tidak ABL langsung reboot (pelajaran dari dubai).
3. DTB: base `blair-moto-fogos-base.dtb` **tidak berisi node display** (display/touch/sensor
   hidup di overlay EVT/DVT). ABL tak bisa menempelkan stock dtbo ke DTB asing →
   **merge overlay saat compile** jadi `blair-moto-fogos-pmos.dtb` (dibuat di `prepare()`
   + didaftarkan via sed; EVT vs DVT1 isinya identik kecuali board-id).
   Jebakan: node placeholder `qcom,battery-data` di base harus di-`/delete-node/` dulu
   (label `mot_batterydata` ganda).
4. BusyBox (chroot Alpine): `dirname` hanya 1 argumen → patch 0001 ganti
   `dirname $(find …)` dengan `$(sort $(dir …))`; `patch` tak bisa bikin file baru
   (file DTS dibuat via heredoc) dan gagal pada hunk berisi baris konteks kosong.
5. `pmbootstrap 3.12.0` vs pmaports baru tidak sinkron soal systemd
   (`postmarketos-base-systemd` sudah dilebur) → pakai `service_manager = openrc`
   **di section `[pmbootstrap]`** (`~/.config/pmbootstrap_v3.cfg`). Jebakan 13 Okt:
   `remote-setup.sh` dulu append di akhir file → mendarat di `[mirrors]` →
   DIABAIKAN → rootfs systemd rusak (`Failed to mount early API filesystems`
   dari PID1 systemd, padahal udevadm-nya relocation-error). Sudah diperbaiki
   di config + skrip (insert di bawah `[pmbootstrap]`).
6. Driver display msm_drm **built-in** (23 simbol di kallsyms); panel terdeteksi otomatis
   via `msm_drm.dsi_display0=` dari ABL (unit ini: TXD ILI9883C).
7. `CONFIG_FB/FBCON/VT` dimatikan Lineage → dinyalakan di delta pmOS untuk konsol layar.
8. Flash permanen di ABL Motorola ini: `fastboot flash userdata` DITOLAK
   (`flash permission denied`) dan stock zip 4GB korup (tak bisa jadi jalan pulang).
   Jalur yang benar: flash `vbmeta` (avbtool `--flags 2`, manual karena avbtool
   tak ada di chroot pmbootstrap) + `dtbo` kosong + `boot` slot aktif saja
   (slot A dibiarkan Android sebagai fallback), lalu rootfs ditaruh **setelah boot**:
   `fastboot boot` initramfs → `setup_usb_storage_configfs /dev/sde26` → di laptop
   mkfs.ext4 + `tune2fs -U <pmos_root_uuid dari cmdline install terakhir>` +
   extract tarball rootfs + buat `dev proc sys tmp mnt (+dev/pts,dev/shm)` +
   komentari baris `/boot` di fstab → reboot. Catatan: UUID rootfs berubah tiap
   `install` (baca dari `/proc/cmdline` live, JANGAN pakai catatan lama!);
   tarball default MENGECUALIKAN dir pseudo-fs sehingga wajib dibuat manual.
   Backup pra-flash tersimpan di `tmp/stock-backup/` (`boot_b` 96M, `dtbo_b` 24M,
   `vbmeta_b` 64K, md5 terverifikasi) + tarball di `tmp/rootfs-fogos.tar.gz`.
9. Trik debug: `fastboot --cmdline "<cmdline + pmos.debug-shell>" boot boot.img`
   memaksa initramfs berhenti di shell walau rootfs ditemukan (alternatif: tahan
   Vol Bawah saat boot → `check_keys()`). Mesin ini tak punya `nc`; shell via
   python3/socket, SSH via askpass (`SSH_ASKPASS`, password `147147`, user `symbiot`).

## Riwayat build kernel (pkgrel)

- r0: awal (gagal: maintainer tanpa email → `Fogos Porter <fogos@example.com>`).
- r1: +LLVM/clang (tanpa cross-gcc), +`modules_install` (60 .ko: wlan, camera, audio…).
- r2: +DTB eksplisit (base dtb lolos dari `dtbs_install` karena hanya efek-samping overlay).
- r3: +merge overlay (gagal 2x: fdtoverlay tak ada di 5.4; patch file-baru vs BusyBox).
- r4: +`DRM_FBDEV_EMULATION`/`FB`/`FBCON` → `/dev/fb0` + tes noise OK.
- r5 (selesai, 13 Okt di remote): +`VT`/`VT_CONSOLE` → terverifikasi di `vmlinuz`:
  `fbcon_init`, `con_init`, `drivers/tty/vt/vt.c` ada; DTB merge memuat 1248 node
  display + node `chipone_tddi`; apk ramping 25M (modul di-strip ke
  `/usr/lib/modules`, layout installkernel baru) vs r1 171M yang belum strip.
  Header boot.img v2, pagesize 4096 ✔.
- device v3-r1 (13 Okt): +`drm`/`getty`/`screen 720x1600`/`external_storage`/
  `boot_filesystem`/`append_dtb`/`flash_kernel_on_update` (sejajar dubai/rhodep;
  kanonis sempat kehilangan ini saat perampingan dari port paralel 0.1-r7) +
  `modules-initfs` touchscreen → terverifikasi di initramfs (`cpio` memuat kedua
  `.ko` + `initramfs.load` bersih 2 baris); `fogos-boot-test.img` di-refresh (51M).
- firmware 2-r0 (13 Okt): dibangun ulang dari repo kanonis, isi identik 1-r0
  (7 file); WiFi (`wlan.ko` qca_cld3) + BT (BTFM) firmwarenya **belum ada**
  (tidak di repo TheMuppets; kandidat: paket stock di `tmp/stock/` 4,2GB atau
  `linux-firmware`) + `msm-firmware-loader` sudah di depends device.

## Hasil test HP (via `fastboot boot`, tanpa flash)

- Boot OK (11 dtk), ping `172.16.42.1` OK, telnet debug shell OK.
- `card0`, `renderD128`, `sde-crtc-0`, `sde-conn-0-DSI-1`, `event0` (power), `event1` (volume).
- Layar: logo ABL → hitam (backlight nyala) → **noise warna-warni** (tulis ke fb0) ✔.

## Flash permanen (13 Okt — slot `_b`, userdata) ✔ BOOT PENUH
- Terflash: `vbmeta_b` (verification-disabled) + `dtbo_b` kosong + `boot_b` pmOS r5;
  rootfs openrc di userdata (`/dev/sde26`, ext4 `pmOS_root`, 226G).
 userdata Android ikut terhapus (sesuai permintaan flash bersih); slot A utuh.
- Verifikasi via SSH (`symbiot@172.16.42.1`): `/` di `/dev/sde26` ✔, OpenRC
  runlevel boot+default jalan (dbus, haveged, unudhcpd.usb0,
  msm-firmware-loader-unpack, tqftpserv…) ✔, `/dev/fb0` + DSI `connected` ✔,
  `event0/1` ada ✔. Telnet initramfs mati (port 23 closed) seperti ekspektasi.

### Sesi 13 Okt, image r5 + device v3-r1 (51M), unit ZP22228PHC, slot `_b`

- Identitas: `product: fogos`, `XT2363-9`, `retid`, `revision=pvt`, bootloader
  `MBM-3.0-uefi-...-V1UGS35H.75-14-3-7-2d75a`; cmdline ABL:
  `msm_drm.dsi_display0=qcom,mdss_dsi_txd_ili9883c_6528_hd_plus_120_video:`
  (panel TXD/ILI9883C terkonfirmasi).
- M3 sisi kernel ✔ (detail di milestone); dmesg penuh + snapshot device-tree
  tersimpan di `tmp/hp-dmesg.txt`, `tmp/hp-devtree.txt`.
- Touch: ilitek → `input3` ✔ (FW `ILITEK_FW` tak ada di initramfs → upgrade gagal
  tapi input tetap terdaftar; di rootfs penuh FW tersedia via firmware 2-r0).
  chipone nonaktif by design. `/dev/input` sebelum modprobe: hanya
  `event0` (qpnp_pon), `event1` (gpio-keys), `event2` (buffyboard).
- Catatan: modul initramfs **tidak** auto-load sebelum shell (perlu `modprobe`
  manual); charger/smblib jalan (USB SDP); IPA/modem gagal seperti ekspektasi
  (firmware `ipa_fws.mdt` belum ada — milestone modem).

## Mesin

- **Lokal** (`galadriel`, Tumbleweed, 8 core): build awal + semua test HP. `pmbootstrap` di
  `~/pmbootstrap`, pmaports sparse di `~/.local/var/pmbootstrap/cache_git/pmaports`,
  config `~/.config/pmbootstrap_v3.cfg` (device motorola-fogos, edge, console, openrc).
- **Remote/cepat** (`princeofgiri@192.168.68.57`, 16 core/30GB): port paralel lama di
  `device/testing/` sudah **dihapus** (sesuai instruksi); rantai setup+build penuh
  dijalankan via SSH (`remote-setup.sh`), aport kanonis sama dengan lokal.
  Keadaan 13 Okt malam: kernel r5 + device v3-r1 + firmware 2-r0 + rootfs **openrc**
  terflash permanen di slot B (userdata); HP boot penuh pmOS, SSH hidup.
  `fogos-boot-test.img` (51M) tetap tersedia untuk tes `fastboot boot`.

## Rencana kerja

1. ~~**Sesi HP berikutnya (kini via SSH, jauh lebih enak)**: (a) konfirmasi **visual**
   teks konsol di layar (M3, sisi kernel sudah ✔); (b) `modprobe ilitek_v3_mmi`
   lalu `evtest /dev/input/event2` (atau hexdump) sambil sentuh layar —
   `input3` ILITEK_TDDI terdaftar ~2 mnt setelah modprobe tanpa FW.~~
   SELESAI 13 Okt (lihat "Visual" di bawah); sisa touch: uji sentuh fisik.
2. **Audio**: node APR/LPASS ikut merge; uji via UCM + speaker (rootfs penuh kini ada).
3. **WiFi/BT** (`wlan.ko` qca_cld3): `firmware-motorola-fogos` 2-r0 sudah ada tapi
   **belum** memuat firmware wlan/BT (tidak ada di repo TheMuppets) → gali dari
   paket stock 4,2GB di `tmp/stock/` atau `linux-firmware`; `msm-firmware-loader`
   sudah di depends device.
4. **Modem/data**: pola rhodep (IPA + interconnect SM6375); prioritas belakangan.
5. ~~**Flash permanen**: petakan partisi `super` …~~ SELESAI 13 Okt (lihat atas;
   `deviceinfo_super_partitions="/dev/sde25"` terpetakan dari sysfs saat backup).
6. **Upstream**: rapikan repo `postmarketos-motorola-fogos`, tambah halaman wiki
   (syarat minimal Nura: bisa boot + ada wiki), ajukan MR ke pmaports.
7. **Stabilitas**: HP reboot sendiri beberapa kali tanpa sebab jelas
   (`bootreason=watchdog`, wdog-snarl/bark smb5 berulang di dmesg) — selidiki
   charger-watchdog PM7250B bila mengganggu.

## Visual di layar (13 Okt — M3 visual ✔, splash ✔)

- Konsol + `Welcome to Nura!` + login tampil di layar (fbcon). Dua spam pembersih:
  (a) elogind `Failed to apply ACL` → akar: `CONFIG_TMPFS_POSIX_ACL` mati →
  kernel r6 (+`TMPFS_XATTR`), elogind sempat di-mask lalu dikembalikan,
  dmesg bersih; (b) `can't open /dev/ttyMSM0` berulang → getty serial dari
  `deviceinfo_getty` padahal driver serial mati → baris `ttyMSM0` di
  `/etc/inittab` dikomentari sementara (jangka panjang: nyalakan
  `SERIAL_MSM_GENI` di kernel ATAU hapus `deviceinfo_getty`).
- Plymouth + tema `pmos-spin` terpasang (`/etc/plymouth/plymouthd.conf`).
- Weston 16 gagal di DRM downstream: abort
  `!weston_drm_format_array_find_format` (`drm-formats.c:131`, format ganda
  dari blob IN_FORMATS msm) — butuh patch weston atau hindari path itu.
- Demo pixman murni tanpa compositor: `tmp/fbshow.py` (decode PNG + mmap fb0,
  pitch dibaca via `FBIOGET_FSCREENINFO` — **2944**, bukan 2880; itulah yang
  bikin gambar miring) + `tmp/mksplash.py` → `tmp/splash-fogos-720x1600.png`
  tampil penuh 1:1 ✔ (fbcon di-unbind dulu: `echo 0 >
  /sys/class/vtconsole/vtcon1/bind`; byte order B,G,R,0 ✔).
- Touch: ilitek probe SUKSES sekali di initramfs (→`input3`), tapi GAGAL di boot
  penuh (`Dummy check incorrect`, SPI tak jawab) — selidiki power/pinctrl/sequencing
  (`fogos-touchscreen.dtsi`, LDO L9A) sebelum uji sentuh fisik.

## Perintah penting

```bash
# rebuild kernel + install + export (di kedua mesin sama)
~/pmbootstrap/pmbootstrap.py checksum linux-motorola-fogos
~/pmbootstrap/pmbootstrap.py build linux-motorola-fogos --arch aarch64
~/pmbootstrap/pmbootstrap.py install --zap --password 147147
~/pmbootstrap/pmbootstrap.py export
# test aman tanpa flash (HP di fastboot):
fastboot boot /tmp/postmarketOS-export/boot.img
# shell HP: telnet via nc (HP 172.16.42.1:23), ssh user@172.16.42.1 (hanya dengan rootfs penuh)
# sinkronisasi lokal <-> remote:
rsync -av /home/galadriel/nura-fogos/ princeofgiri@192.168.68.57:~/nura-fogos/
```
