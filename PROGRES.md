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
- [x] **Touchscreen ✔ SEMPURNA (13-15 Okt)**: driver ilitek probe deterministik
      via `/etc/local.d/touchscreen.start`. Di Wayland/Plasma Mobile, sempat tidak merespons
      karena `libudev-zero` salah mengklasifikasikan sebagai touchpad gagal uji sanity.
      Setelah ditukar ke `eudev-libs` + udev rule 99-touchscreen (`MODE=0666`) +
      quirk `-BTN_TOOL_FINGER`, touchscreen 10-point multitouch (`keyboard touch`)
      berjalan 100% responsif di UI Plasma Mobile.
- [x] **M4 — Graphical UI Plasma Mobile (15 Okt)**: `postmarketos-ui-plasma-mobile`
      berjalan di KWin Wayland menggunakan mode Legacy KMS (`KWIN_DRM_NO_AMS=1`).
      Optimasi UI: panel diturunkan ke 60Hz (`720x1600@60Hz`), multi-threaded LLVMpipe
      `LP_NUM_THREADS=8` + `QSG_RENDER_LOOP=threaded`, efek berat (blur, contrast, translucency)
      dimatikan untuk menjaga responsivitas CPU software rasterizer.
- [x] **M5 — Modem Baseband Online & Registrasi Tower Telkomsel ✔ SEMPURNA (15-17 Okt)**:
      - ModemManager mendeteksi perangkat Qualcomm SoC (`/org/freedesktop/ModemManager1/Modem/1`), terikat ke `qrtr0` (qmi) dan `rmnet_ipa0` (net).
      - Bug DSDS ModemManager (slot 2 kosong menimpa slot 1 aktif) berhasil diatasi secara bersih via shim library `libmm-slot-fix.so` (`LD_PRELOAD`) yang secara dinamis menandai slot kosong sebagai `INACTIVE`, mem-bypass timeout 25 carrier config pada PDC `list_configs`, dan menangani quirk QMI DMS `Set Operating Mode` & NAS preference.
      - SIM fisik By.U di Slot 1 terdeteksi 100% sempurna di userspace & baseband: ICCID `8962100048909141989`, IMSI `510104890914198`, Operator ID `51010` (Telkomsel), GID1/GID2 `02`, sesi provisioning aktif di slot 1, app 1.
      - **Root Cause QMI Error 52 (DeviceNotReady / HOST_IMAGE_MISCONFIGURATION) Dipecahkan**:
        - Saat boot, Hexagon modem MPSS meminta DSP shared object RF `/readonly/firmware/image/modem_pr/so/568_0_0.mbn` via TFTP.
        - `tqftpserv` menolak permintaan tersebut karena symlink `/lib/firmware/modem_pr` belum ada dan mock remoteproc belum diinisialisasi sebelum daemon berjalan (`tqftpserv` berjalan di runlevel `boot`, sebelum `local.d`).
        - Diperbaiki dengan memindahkan setup symlink (`image`, `modem_pr/so`, `remoteproc`) dan mount partisi FSG ke dalam `start_pre()` di `/etc/init.d/tqftpserv`.
      - **Band Capabilities Penuh & Baseband Online**:
        - `dms-get-capabilities` melaporkan `gsm, umts, lte, 5gnr`.
        - `dms-get-band-capabilities` melaporkan seluruh band aktif: LTE B1, B2, B3, B5, B7, B8, B20, B26, B28, B32, B38, B40, B41, B42 serta NR5G N1, N3, N5, N7, N8, N20, N26, N28, N38, N40, N41, N77, N78.
        - `dms-set-operating-mode=online` sukses tanpa error (`Mode: 'online'`, `HW restricted: 'no'`).
      - **MBN Telkomsel Aktif & Registrasi Tower Sukses**:
        - MBN Telkomsel (`20016B7FEE23ACA24DC4D7D145C9BF2C13E943C0`) diverifikasi sebagai active software config di PDC.
        - Modem berhasil register ke tower seluler: `state: registered`, `registration: home`, `CS domain: attached`, `PS domain: attached`.
        - Operator: `IND TELKOMSEL` (`51010` / `by.U`), Cell ID: `37838879`, TAC: `1152`.
        - Sinyal: `99%` (LTE & 5G NR, RSRP -90 dBm, RSSI -57 dBm, SNR 6.6 dB).
        - Audio ALSA stereo (`qcom_q6core-audio_sound`) dan Wi-Fi (`wlan0` 192.168.0.191) tetap utuh dan stabil.
- [x] **M6 — Packaging & KWin Legacy KMS DRM Layer Fix (15 Okt)**:
      - Seluruh perbaikan live (`libqrtr-fix.so`, `libmm-slot-fix.so`, udev rules, local.d autostart, conf.d) dikemas resmi ke paket `device-motorola-fogos` (pkgver=3-r3).
      - Build APK sukses via `pmbootstrap build device-motorola-fogos --arch aarch64` (`device-motorola-fogos-3-r3.apk`).
- [x] **Screen Sleep & Power Crash Resolved (16 Okt)**:
      - Layar sleep memicu reboot karena register access downstream SDE pada saat DPMS off.
- [x] **M7 — Wi-Fi WCN3990 & Dual-Band 2.4/5GHz Scanning ✔ SEMPURNA (16 Okt)**:
      - Driver Qualcomm WCN3990 (`qcacld-3.0` / `wlan.ko` + `icnss2`) membutuhkan userspace daemon `cnss-daemon` untuk melakukan handshake QMI, penyediaan MAC address, dan inisialisasi kalibrasi sebelum WLAN firmware menerbitkan event `FW_READY`.
      - Loop mount partisi stock vendor (`/mnt/stock_vendor`) dan stock system (`/mnt/stock_system`) dari partisi `super` (`/dev/sde25`) secara read-only.
      - Android runtime APEX diekstrak untuk menyediakan Bionic libc dan dynamic linker 64-bit (`linker64`).
      - Android `binderfs` dimount di `/dev/binderfs` dengan node `/dev/binder` dan `/dev/vndbinder`.
      - Bionic WaitForProperty and SELinux checks pada daemon vendor di-bypass secara dinamis (`fogos-patch.py`).
      - Stack daemon Android vendor (`vndservicemanager`, `pm-service`, `pm-proxy`, `cnss-daemon`) berhasil berjalan sepenuhnya secara native di postmarketOS.
      - `wlan0` terdaftar secara native di kernel dan NetworkManager, mendukung pemindaian dual-band 2.4GHz dan 5GHz (terdeteksi puluhan AP dengan sinyal penuh dan kecepatan hingga 1170 Mbit/s).
      - Seluruh stack dikemas ke dalam OpenRC service `fogos-wifi` di `device-motorola-fogos` (pkgver=3-r5).
- [x] **M8 — Akselerasi Hardware GPU Penuh: Mesa Turnip-KGSL + Zink ✔ SEMPURNA (19 Okt)**:
      - **Driver Mesa Turnip-KGSL**: Mesa 26.2.5 di-fork dan dikompilasi dengan `-Dfreedreno-kmds=msm,kgsl,virtio`. Driver Turnip Vulkan berhasil mengakses node `/dev/kgsl-3d0` secara native di kernel downstream 5.4.
      - **Deteksi Hardware Adreno 619**: Terdeteksi penuh sebagai `Turnip Adreno (TM) 619`, Vulkan API 1.3.354 (Device ID `0x06010901`, Vendor ID `0x5143`, clock aktif 266 MHz).
      - **Patch KMSRO & Fallback Zink (`zink-kgsl-kmsro.patch`)**:
        - Memperbaiki Zink device picker (`zink_get_display_device`) agar otomatis memilih GPU non-CPU saat menjalankan KMSRO pada driver display `/dev/dri/card0` tanpa DRM render match.
        - Mengaktifkan `VK_EXT_physical_device_drm` di Turnip KGSL dengan menautkan ID `/dev/dri/card0` dan `/dev/dri/renderD128`.
        - Meniadakan fatal error pada `KHR_display` sehingga `vulkaninfo --summary` berjalan 100% mulus tanpa crash.
      - **Benchmark & Hasil Desktop**:
        - `glmark2-es2-wayland`: **260 FPS** (FrameTime: 3.856 ms) pada Wayland resolusi native (naik >15x lipat dibanding software rasterizer llvmpipe).
        - KWin Wayland mengenali dan memilih render device `/dev/dri/renderD128: Turnip Adreno ™ 619` dan `/dev/dri/card0: zink Vulkan 1.3` sebagai GPU utama.
        - CPU load turun drastis (load average 0.10) dan sistem berjalan stabil tanpa crash/reboot.
- [ ] Berikutnya: Packet data attach (EPS/PS attach), mobile data routing rmnet_ipa0, audio internal routing.

## File port (kanonis, di `~/nura-fogos/`)

- `device-motorola-fogos/` (pkgver=3-r3): `deviceinfo`, `kernel-cmdline.conf`,
  `modules-initfs`, `libqrtr-fix.c` (shim QRTR service publication instance APSS),
  `libmm-slot-fix.c` (shim DSDS ModemManager), conf.d OpenRC (`pd-mapper`, `tqftpserv`,
  `rmtfs`, `modemmanager`), udev rules (`99-touchscreen`, `78-mm-rmnet-ipa`, libinput quirks),
  dan autostart local.d (`display.start`, `touchscreen.start`, `rmtfs.start`, `modem-setup.start`,
  `net-route.start`). Built & verified via pmbootstrap.
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
- r6 (14 Okt): +`CONFIG_TMPFS_POSIX_ACL=y` + `TMPFS_XATTR=y` → elogind
  `Failed to apply ACL` hilang, dmesg bersih ✔ (terverifikasi via SSH).
- r7 (14 Okt): +`vdd/vcc-supply = <&L9A>` di `ilitek@1` (DT merge) → probe
  ilitek sukses bila chipone di-modprobe dulu (urutan via
  `/etc/local.d/touchscreen.start`); `input2`/`event2` + `evtest` +
  `touchdraw.py` akurat (kalibrasi X 130..2731, Y 153..6235; pitch fb0 2944).
- r8 (14 Okt): +`CONFIG_QCOM_RMTFS_MEM=y` → `/dev/qcom_rmtfs_uio1` ada;
  `rmtfs -r -P` jalan (tanpa `-s`: butuh remoteproc-sysfs yang tak ada di
  downstream); deploy via apk di HP (UUID-safe, tanpa reinstall).
- r9 (15 Okt, belum dites — HP offline): eksperimen trigger modem via
  `qcom,pon-depends-on = "modem"` di `pil_lpass` (mekanisme
  `__subsystem_get()` → boot dependensi dulu saat ADSP di-boot manual) +
  rebalance ramoops (console 256K→512K untuk trace hang Weston; total tetap
  0xBF800; WDOG/TZLOG/MMI di bawahnya tak tersentuh). Terverifikasi di DTB:
  `pon-depends-on`, `console-size 0x80000`, supply ilitek utuh.
- r10 (15 Okt, belum dites — HP offline): +`0002-ilitek-charger-notifier-
  unregister.patch` (tambah flag + unreg notifier + teardown wq di
  `ili_dev_remove`; perbaiki UAF rmmod → panic CFI). Pelajaran pahit:
  tulis patch tangan rawan hunk korup (nomor/konteks meleset diam-diam);
  SELALU generate via `git diff` dari tree teredit + verifikasi
  `patch --dry-run` pada salinan bersih SEBELUM commit ke aport.

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
2. **Audio — kartu TERDAFTAR manual, autoload rapuh, bunyi tertunda (13–14 Okt)**:
   rantai `holi-9370_fs1815-snd-card` di `/proc/asound/cards` TERCAPAI via:
   ADSP boot manual + `stub_dlkm` (coldplug gagal path) + macro + slave +
   `q6core` muncul → machine register ✔. TAPI: (a) `audio.start` sempat
   rusak (`$m` hilang saat escaping → modprobe tanpa argumen; sudah ditulis
   ulang dari laptop), (b) `rmtfs.start` foreground memblokir `local.d`
   (touch ikut mati; kini background `&`), (c) speaker-test tetap
   `ADSP_EFAILED` walau ACDB sudah ditaruh di target/ — q6core device TAK
   MUNCUL di boot normal (butuh PDR → `pd-mapper`/`qrtr-ns` yang belum ada),
   (d) UCM + amp aw882xx/fs1815 belum disentuh. Proyek lanjutan:
   bangun daemon QRTR userspace → q6core → sound → UCM/amp.
   AWAS (14 Okt): tulis ke `/sys/kernel/boot_adsp/ssr` me-reboot HP —
   jangan utak-atik SSR ADSP dengan stack termuat.
3. **WiFi (driver OK, HIF belum attach) + BT (kernel siap) — 13 Okt**: `wlan.ko`
   qcacld-3.0 TERBUKTI mengandung dukungan QCA6750 + SNOC/iCNSS (strings:
   `QCA6750`, `hif_snoc_*`, `__icnss_register_driver`); FW `wlanmdsp.mbn` +
   puluhan `bdwlan.*` hadir via msm-firmware-loader; `icnss2` bound; `wlan.ko`
   load + `/dev/wlan` ada. Tapi `echo ON > /dev/wlan` memicu `hdd_driver_load`
   yang macet sunyi (tanpa wlan0, tanpa FW request) — HIF/iCNSS tak pernah
   attach; kuat dugaan WCN ikut power modem (lihat Modem). BT: `/dev/ttyHS0`
   ada, `bt_wcn3990` bound ke `bt_power`, `BT_HCIUART_QCA` built-in,
   FW `apbtfw*.tlv`+`apnv*` tersedia → tinggal attach userspace (BlueZ).
4. **Modem — FW+stack siap, trigger boot belum ketemu (13–14 Okt)**: `modem.mbn/.mdt`
   tersaji via msm-firmware-loader; MSS bound ke `subsys-pil-tz`; rmtfs JALAN
   (`-r -P`, tanpa `-s` yang butuh remoteproc-sysfs tak ada di downstream;
   butuh kernel r8 `CONFIG_QCOM_RMTFS_MEM` untuk `/dev/qcom_rmtfs_uio1`);
   qrtr/tqftpserv/soc-qcom-modem terpasang. Tapi modem tak pernah PIL:
   tak ada auto-boot/ioctl/sysfs-trigger di kernel maupun paket pmOS
   (`/dev/subsys_modem` hanya SSR, `waiting_for_supplier`=0, `pd-mapper`
   hanya lookup tak mem-boot, `qrtr-ns`/`peripheral-manager` tak ada di
    Alpine). Kandidat tersisa: port `peripheral-manager` (pm-service) /
    teliti init stock (`per_mgr`, `cnd`). Autostart rmtfs dipasang
    (`/etc/local.d/rmtfs.start`).
    UPDATE 15 Okt (r9, offline, belum dites): `qcom,pon-depends-on = "modem"`
    ditambahkan ke `pil_lpass` di DT merge — `__subsystem_get("adsp")`
    (dipicu manual `echo 1 > /sys/kernel/boot_adsp/boot`, sudah terbukti
    untuk audio) akan mem-boot modem dulu. Eksperimen reversibel; uji saat
    HP online: cek `dmesg | grep -i pil` + `qrtr-lookup`.
5. ~~**Flash permanen**: petakan partisi `super` …~~ SELESAI 13 Okt (lihat atas;
   `deviceinfo_super_partitions="/dev/disk/by-partlabel/super"` → sde25,
   device v3-r2).
6. **Upstream**: rapikan repo `postmarketos-motorola-fogos`, tambah halaman wiki
   (syarat minimal Nura: bisa boot + ada wiki), ajukan MR ke pmaports.
7. **Stabilitas**: HP reboot beberapa kali. Dua sebab berbeda TELAH
   diidentifikasi: (a) tombol power dipencet manual saat layar hitam
   (diakui user — JANGAN saat testing!); (b) SATU kernel panic nyata 14 Okt:
   `CFI failure` di `power_supply_changed_work` → notifier power_supply
   dengan pointer sampah (UAF atau signature salah; ilitek & chipone sudah
   dibersihkan dari daftar tersangka — signature benar & unregister/exit
   path dicek; `touchscreen_mmi` pun reg/unreg rapi). Butuh frekuensi via
   `/root/uptime.log` (monitor tiap menit) + jejak pstore bila terulang.
   Reboot-reboot misterius lain kemungkinan besar (a), bukan kernel.

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
  dari blob IN_FORMATS msm) — TAPI sesudah di-patch (aport lokal
  `local-weston/`, build OK, terdeploy), weston start → kernel HANG →
  APSS watchdog reset (`bootreason=watchdog`, tanpa jejak panic; pstore
  ramoops bawaan overlay terlalu kecil sehingga log crash terpotong).
   Teori kuat: modeset ATOMIC macet (fbcon pakai path legacy dan selalu OK);
   weston 16 tampaknya atomic-only. UPDATE 15 Okt (r9, offline): ramoops
   console 256K→512K (total tetap, region bawah aman) SUDAH di DTB;
   `drm.debug` tinggal via override cmdline fastboot
   (`fastboot --cmdline "<cmdline + drm.debug=0x1f>" boot boot.img) tanpa rebuild.
- Demo pixman murni tanpa compositor: `tmp/fbshow.py` (decode PNG + mmap fb0,
  pitch dibaca via `FBIOGET_FSCREENINFO` — **2944**, bukan 2880; itulah yang
  bikin gambar miring) + `tmp/mksplash.py` → `tmp/splash-fogos-720x1600.png`
  tampil penuh 1:1 ✔ (fbcon di-unbind dulu: `echo 0 >
  /sys/class/vtconsole/vtcon1/bind`; byte order B,G,R,0 ✔).
- Touch: ilitek probe SUKSES sekali di initramfs (→`input3`), tapi GAGAL di boot
  penuh (`Dummy check incorrect`, SPI tak jawab) — selidiki power/pinctrl/sequencing
  (`fogos-touchscreen.dtsi`, LDO L9A) sebelum uji sentuh fisik.

### 16 Okt 2026: Direct Flashing, Atomic KMS Fix, Touchscreen & 60Hz Display OK

- **Direct Rootfs & Boot Flashing**:
  - Dibuat `tools/flash_rootfs.py` untuk streaming rootfs mentah langsung ke `/dev/sde26` (`userdata`, ext4 241 GiB) dan `boot.img` ke `/dev/sdf18` (`boot_b`) dari shell Linux / recovery.
  - Slot `_b` ditandai boot sukses permanen menggunakan `qbootctl -m`.
- **KWin Display Fix (Atomic KMS)**:
  - Root cause layar hitam / dmesg tty0: `KWIN_DRM_NO_AMS=1` memaksa KWin menggunakan legacy path (`drmModeSetCrtc`), yang gagal di driver downstream MSM DRM (`Applying output configuration failed: No such device or address` / ENXIO) saat mencoba sharing frame antara `renderD128` dan `card0`.
  - Solusi: Menghapus `KWIN_DRM_NO_AMS=1` dan menyetel `KWIN_DRM_DEVICES=/dev/dri/card0`. KWin berjalan mulus dengan Atomic KMS di `/dev/dri/card0` tanpa error.
- **Refresh Rate 60Hz**:
  - Mengubah mode aktif display ke 60Hz (`720x1600@60Hz`, scale 2.0) via `kscreen-doctor output.1.mode.3`. Driver MSM DSI sukses menyetel `fps=60`. Konfigurasi tersimpan permanen di `~/.config/kwinoutputconfig.json`.
- **Touchscreen Firmware & Multitouch**:
  - Firmware `ILITEK_FW` (507 KB) disalin ke `/lib/firmware/ILITEK_FW`.
  - Driver `ILITEK_TDDI` sukses memuat firmware dan mendaftarkan perangkat input di `/dev/input/event2`.
  - Quirk `local-overrides.quirks` menonaktifkan `BTN_TOOL_FINGER` sehingga libinput mengenali sebagai touch device. Multitouch 10-point terverifikasi responsif secara fisik di lockscreen dan setup wizard.
- **System**:
  - Daemon `seatd` diaktifkan di default runlevel OpenRC.
  - Sudo passwordless dikonfigurasi untuk user `symbiot`.

### 16 Okt 2026: Wi-Fi, SSR Stability, and Audio Bring-up

- **Wi-Fi & Internet Routing**:
  - Wi-Fi aktif dan terhubung via `wlan0` (IP 192.168.0.191).
  - SSH direct via Wi-Fi (`symbiot@192.168.0.191`) diverifikasi tanpa kabel USB.
  - Perbaikan metrik rute di `/etc/local.d/net-route.start` (`default via 172.16.42.2 dev usb0 metric 1000`) sehingga default gateway utama otomatis melalui `wlan0` (metric 600). Ping `google.com` via Wi-Fi sukses (0% packet loss, avg 19-21ms).
- **Subsystem Restart (SSR) & Bootloop Fix**:
  - Root cause reboot saat Wi-Fi tap / modem crash: Hexagon modem mengalami stalled watchdog `DOG detects stalled initialization` karena `rmtfs` belum siap saat `cnss-daemon` mengakses modem. Nilai default `restart_level` adalah `SYSTEM` sehingga memicu kernel panic / SoC reset.
  - Solusi: Menyetel `restart_level` ke `RELATED` untuk semua subsistem via `/etc/local.d/00-subsys-ssr.start` dan `/etc/udev/rules.d/99-subsys-ssr.rules`.
  - Service `rmtfs` dikonfigurasi dengan flag `-r -P` dan dijadikan dependency `need` di `/etc/init.d/fogos-wifi`.
  - Memulihkan kernel binary valid di `/boot/vmlinuz-5.4.302-moto` dan `/boot/vmlinuz` serta menonaktifkan auto-flash `boot-deploy` (`deviceinfo_flash_kernel_on_update="false"`).
- **Audio Bring-Up (ALSA & PulseAudio)**:
  - Hexagon ADSP di-boot via `/sys/kernel/boot_adsp/boot` (`apr_adsp_up: Q6 is Up`).
  - Techpack audio kernel modules dimuat (`pinctrl_lpi_dlkm`, `swr_ctrl_dlkm`, `tx_macro_dlkm`, `rx_macro_dlkm`, `va_macro_dlkm`, `wcd937x_slave_dlkm`, `stub_dlkm`, `native_dlkm`, `fs1815_dlkm`, `machine_dlkm`).
  - Sound card terdaftar di ALSA: `card 0: holi9370fs1815s [holi-9370_fs1815-snd-card]` dengan 41 PCM devices.
  - Firmware Smart PA `/lib/firmware/fs1815.fsm` (1052 bytes) berhasil dimuat oleh `fs1815_dlkm`; kedua instance PA (0x35 Speaker dan 0x34 Receiver/Handset) terinisialisasi (`dev initilized`, `preset ver:A5E0`).
  - Routing MultiMedia1 -> `RX_CDC_DMA_RX_0` dan switch amplifier (`FSM_Amp_Switch=1`, `FSM_Scene=0`) disetel sesuai `mixer_paths.xml`.
  - PulseAudio 17.0 otomatis mendeteksi sink stereo fallback sebagai default sink.
  - Dibuat `/etc/local.d/audio.start` dan dimasukkan ke paket `device-motorola-fogos` (pkgrel 7).
  - Playback tes nada chime terverifikasi via `paplay` dan `aplay`.

- **Battery Detection & Icon (Plasma Mobile)**:
  - Downstream Qualcomm kernel secara default melaporkan `/sys/class/power_supply/qcom_battery/type` sebagai `Mains`, sehingga UPower mengklasifikasikan baterai sebagai charger dinding (`line_power_qcom_battery`) dan menampilkan status 0% / `battery-missing-symbolic`.
  - Dibuat `/etc/local.d/01-battery.start` yang melakukan bind mount teks `Battery` ke `/sys/class/power_supply/qcom_battery/type` dan me-restart `upowerd`.
  - UPower dan Plasma Mobile kini mengenali `battery_qcom_battery` dengan akurat (kapasitas persentase, voltase, waktu pengosongan, dan status discharging) dan menampilkan ikon baterai + persentase di status bar.

- **Pencegahan Crash Layar Blank / DPMS Sleep (Kernel Panic SDE)**:
  - Saat UPower beralih ke mode baterai (`On Battery`), KDE PowerDevil secara default mengaktifkan timeout layar 30 detik (`TurnOffDisplayIdleTimeoutSec=30`).
  - Pada kernel downstream MSM DRM Qualcomm 5.4 (Snapdragon 695), mematikan tampilan atau meredupkan via DPMS memutus clock subsistem display (SDE). Saat commit grafis atau pembersihan dim layer berjalan (`_sde_crtc_blend_setup` -> `sde_hw_lm_clear_dim_layer` -> `sde_reg_read`), pembacaan register tanpa clock memicu `synchronous external abort: 96000010` (kernel panic seketika).
  - Solusi: Dibuat template konfigurasi KDE PowerDevil `/etc/xdg/powerdevilrc` (dan `~/.config/powerdevilrc`) yang menonaktifkan pemadaman display idle dan auto-suspend (`TurnOffDisplayWhenIdle=false`, `DimDisplayWhenIdle=false`, `TurnOffDisplayIdleTimeoutSec=-1`) pada seluruh profil (`[AC]`, `[Battery]`, `[LowBattery]`).
  - Layar kini tetap menyala stabil dan responsif tanpa resiko hang/reboot saat ditinggal idle.

- **Power Button Sleep/Wake & Double Tap to Wake (DT2W)**:
  - Dibuat daemon `fogos-powerd` (`/usr/bin/fogos-powerd.py` dan OpenRC service `/etc/init.d/fogos-powerd`).
  - Mengambil akses eksklusif (`EVIOCGRAB`) terhadap tombol power fisik (`/dev/input/event0`, `qpnp_pon`) sehingga KWin tidak memicu DPMS off.
  - Saat tombol power ditekan (layar ON): Backlight dimatikan (`brightness=0`) dan sesi dikunci (`loginctl lock-session`).
  - Saat tombol power ditekan (layar OFF): Backlight dipulihkan seketika ke nilai semula (50%), menampilkan lockscreen PIN/jam/baterai.
  - Fitur **Double Tap to Wake (DT2W)** diaktifkan pada IC Ilitek (`echo 1 > /sys/devices/virtual/touchscreen/primary/gesture`). Ketukan 2x pada layar saat kondisi padam langsung membangunkan layar tanpa perlu menyentuh tombol power fisik.
  - Auto-sleep timer 120 detik: Jika ponsel tidak disentuh selama 2 menit, layar otomatis padam dan terkunci secara aman tanpa crash.

- **GPU Bring-Up & Deteksi Adreno 619 (19 Okt)**:
  - **Identifikasi Perangkat Keras**: SoC Snapdragon 695 5G (SM6375 / Blair) dilengkapi GPU **Qualcomm Adreno 619 v2** (`Adreno619v2`), Device ID 1, Chip ID `0x6010901`, Vulkan Device ID `0x6010901`, GMEM SRAM 512 KB (`0x80000`), frekuensi dinamis 266–900 MHz.
  - **Penyebab Timeout / Error Awal**: Percobaan membuka `/dev/kgsl-3d0` sebelumnya mengalami hang 60 detik (`ETIMEDOUT 110`) karena kernel gagal memuat firmware SQE (`kgsl kgsl-3d0: request_firmware(a630_sqe.fw) failed: -110`).
  - **Deployment Firmware**:
    - Microcode SQE (`a630_sqe.fw`, 32 KB) dan GMU power microcontroller (`a619_gmu.bin`, 32 KB) disalin dari partisi vendor ke `/lib/firmware/` dan `/lib/firmware/qcom/`.
    - Firmware Zap Shader (`a615_zap.mbn/.mdt/.b00/.b01/.b02`) dipasangkan untuk otentikasi TrustZone subsys-pil-tz.
    - Kernel downstream berhasil mem-PIL Zap shader: `subsys-pil-tz soc:qcom,kgsl-hyp: a615_zap: Brought out of reset`, `pil_boot is successful from a615_zap`.
  - **Udev Permissions**:
    - Node `/dev/kgsl-3d0` awalnya berstatus `0600 root:root` sehingga tidak bisa diakses sesi non-root (`symbiot`).
    - Dibuat `50-kgsl.rules` (`KERNEL=="kgsl-3d0", GROUP="video", MODE="0666"`) dan dimasukkan ke paket `device-motorola-fogos` (pkgrel 9).
    - Akses ioctl KGSL (`IOCTL_KGSL_DEVICE_GETPROPERTY`) dari userspace biasa kini 100% sukses dan membaca properti Adreno 619 secara instan.

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
