# Nura OS / postmarketOS for Motorola Moto G34 5G (`fogos`)

Downstream-kernel (5.4, LineageOS) port of postmarketOS (a.k.a. Nura OS —
same tooling: `pmbootstrap` + `pmaports`) for the **Motorola Moto G34 5G
(XT2363-9, codename `fogos`, Snapdragon 695 / SM6375)**.

Tested unit: `ZP22228PHC`, slot `_b`, TXD/ILI9883C panel + ilitek touch.

## Status

| Part | State |
|---|---|
| Boot (`fastboot boot` + permanent flash) | ✅ kernel 5.4.302-moto, USB-networking, SSH |
| Display (msm_drm, 720x1600@120Hz, backlight) | ✅ |
| Framebuffer console (fbcon + VT) | ✅ login visible on screen |
| Touchscreen (ilitek, dual-source w/ chipone) | ✅ evtest + live visual test |
| Plymouth (`pmos-spin`) | ✅ theme installed |
| Framebuffer demo tools | ✅ `tools/fbshow.py`, `tools/touchdraw.py` |
| Audio (APRS/LPASS nodes merged) | ❌ not tested (modules exist) |
| WiFi/BT (`wlan.ko` qca_cld3) | ❌ firmware missing (not in vendor repos) |
| Modem/IPA, camera | ❌ later |
| Weston 16 (drm+pixman) | ❌ aborts on duplicate DRM format (downstream quirk, see below) |

`PROGRES.md` (Indonesian) is the full day-by-day dev log, including every
technical decision behind this port.

## Layout

- `device-motorola-fogos/` — device package (bootimg v2, empty dtbo,
  touchscreen `modules-initfs`, `deviceinfo`)
- `linux-motorola-fogos/` — downstream kernel (LineageOS `c63a2b5a` + pmOS
  delta: FBCON/VT, TMPFS ACL/XATTR, merged `blair-moto-fogos-pmos` DTB,
  ilitek `vdd/vcc-supply`)
- `firmware-motorola-fogos/` — NFC/touch/amp/GPU blobs from TheMuppets
  (pinned commits, checksummed — no blobs in this repo)
- `tools/` — framebuffer PNG viewer, live touch visualizer, splash
  generator, partition-backup puller, apk dependency resolver
- `scripts/remote-setup.sh` — full setup+build chain for a fresh machine

## Requirements

- x86_64 Linux host, `pmbootstrap` 3.12+, a `pmaports` checkout
- Unlocked bootloader, USB cable, patience

## Build

```bash
git clone https://gitlab.postmarketOS.org/postmarketOS/pmbootstrap.git ~/pmbootstrap
# plus a pmaports checkout (postmarketOS master), then:
cp -r device-motorola-fogos linux-motorola-fogos firmware-motorola-fogos \
  <pmaports>/device/downstream/
# or just run:
./scripts/remote-setup.sh
```

> **Important:** `service_manager = openrc` must live under the
> `[pmbootstrap]` section of `~/.config/pmbootstrap_v3.cfg`. Appended at the
> end of the file it lands in `[mirrors]` and is silently ignored, which
> produces a broken systemd rootfs. `remote-setup.sh` handles this correctly.

```bash
~/pmbootstrap/pmbootstrap.py checksum linux-motorola-fogos
~/pmbootstrap/pmbootstrap.py checksum device-motorola-fogos
~/pmbootstrap/pmbootstrap.py checksum firmware-motorola-fogos
~/pmbootstrap/pmbootstrap.py build linux-motorola-fogos --arch aarch64
~/pmbootstrap/pmbootstrap.py build device-motorola-fogos
~/pmbootstrap/pmbootstrap.py build firmware-motorola-fogos
~/pmbootstrap/pmbootstrap.py install --zap --password 147147
~/pmbootstrap/pmbootstrap.py export
fastboot boot /tmp/postmarketOS-export/boot.img   # safe test, no flashing
# shell: python3 + raw TCP to 172.16.42.1:23 (this host has no netcat)
```

## Permanent install (what actually works on this ABL)

`fastboot flash userdata` is **denied** by Motorola's bootloader, so the
rootfs goes on **after** booting (see `PROGRES.md` §8 for the full story):

1. Back up your stock slot first: `tools/pullblk.py` over the initramfs
   debug shell (`boot_b`/`dtbo_b`/`vbmeta_b`, verify md5!).
2. `fastboot flash vbmeta` a verification-disabled image
   (`avbtool make_vbmeta_image --flags 2`), flash empty `dtbo`, flash `boot`.
3. `fastboot boot` the initramfs, expose userdata:
   `setup_usb_storage_configfs /dev/sde26`.
4. On the host: `mkfs.ext4`, `tune2fs -U <pmos_root_uuid from the install's
   boot.img cmdline>`, extract the installed rootfs, create
   `dev proc sys tmp mnt` (+`dev/pts`, `dev/shm`), comment out `/boot` fstab.
5. Reboot → full system, SSH `symbiot@172.16.42.1`.

Note: every `pmbootstrap install` regenerates rootfs UUIDs — always read the
current `pmos_root_uuid` from the boot.img header, never from old notes.
Also make sure `distfiles`/tarballs never enter git (see `.gitignore`).

## Returning to stock

Slot A was left intact during development, and `tools/pullblk.py` backups +
a full stock CFC package restore the rest. Keep your own backups; stock
images are intentionally **not** in this repo.

## Known quirks

- `earlycon` on the kernel cmdline is mandatory or ABL reboots instantly.
- Touch needs `chipone` probed before `ilitek` (ordered autoload in
  `/etc/local.d/touchscreen.start` on the device).
- No `/dev/ttyMSM0` (serial driver off) → the generated inittab getty line
  must stay commented until the serial driver is enabled.
- Weston 16 aborts: `!weston_drm_format_array_find_format` — downstream
  msm_drm reports a duplicate format; needs a weston patch or another path.
- The touchscreen firmware (`ILITEK_FW`) ships in `firmware-motorola-fogos`
  but is absent from the initramfs (driver still registers input).

## License

Packaging (APKBUILDs, deviceinfo, scripts, tools): MIT, see `LICENSE`.
The kernel patch under `linux-motorola-fogos/` is GPL-2.0-only, and all
firmware remains property of its vendors (`license="proprietary"` in the
firmware aport; blobs are fetched at build time, never committed).
