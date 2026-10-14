# Patched weston for Motorola fogos (downstream msm_drm reports a pixel
# format twice; upstream aborts, this build reuses the entry).
# Same version as Alpine (16.0.0) plus pkgrel bump, so it upgrades the
# stock weston/libweston/weston-backend-drm cleanly. Only the packages
# needed on-device are split out; everything else is disabled at configure
# time to keep the build small.
maintainer="Fogos Porter <fogos@example.com>"
pkgname=weston
pkgver=16.0.0
pkgrel=1
pkgdesc="Reference compositor for Wayland (fogos downstream-DRM fix)"
url="https://waylab.freedesktop.org/weston/"
arch="all"
license="MIT"
depends="libweston=$pkgver-r$pkgrel"
makedepends="
	cairo-dev
	expat-dev
	libdrm-dev
	libevdev-dev
	libdisplay-info-dev
	libinput-dev
	libjpeg-turbo-dev
	libpng-dev
	libseat-dev
	libwebp-dev
	libxkbcommon-dev
	lcms2-dev
	mesa-dev
	meson
	pixman-dev
	vulkan-headers
	vulkan-loader
	wayland-dev
	wayland-protocols
	"
subpackages="
	$pkgname-backend-drm:backend_drm
	$pkgname-backend-headless:backend_headless
	$pkgname-dev
	libweston
	"
source="https://gitlab.freedesktop.org/wayland/weston/-/archive/$pkgver/weston-$pkgver.tar.gz
	0001-libweston-tolerate-duplicate-formats.patch
	"
options="!check"

build() {
	abuild-meson \
		-Dbackend-drm=true \
		-Dbackend-headless=true \
		-Dbackend-pipewire=false \
		-Dbackend-rdp=false \
		-Dbackend-vnc=false \
		-Dbackend-wayland=false \
		-Dbackend-x11=false \
		-Drenderer-gl=true \
		-Drenderer-vulkan=false \
		-Dxwayland=false \
		-Dsystemd=false \
		-Dshell-desktop=false \
		-Dshell-ivi=false \
		-Dshell-kiosk=false \
		-Dshell-lua=false \
		-Dcolor-management-lcms=true \
		-Dimage-jpeg=true \
		-Dimage-webp=true \
		-Ddemo-clients=false \
		-Dtests=false \
		-Dtest-junit-xml=false \
		. output
	meson compile -C output
}

package() {
	DESTDIR="$pkgdir" meson install --no-rebuild -C output
	# shiped by Alpine weston-terminal (kept); avoid file conflict
	rm -f "$pkgdir"/usr/bin/weston-terminal
	# abuild rejects uncompressed man pages
	if [ -d "$pkgdir/usr/share/man" ]; then
		find "$pkgdir/usr/share/man" -type f ! -name '*.gz' -exec gzip -9 {} +
	fi
}

backend_drm() {
	pkgdesc="Weston DRM/KMS backend"
	amove usr/lib/libweston-16/drm-backend.so
}

backend_headless() {
	pkgdesc="Weston headless backend (testing)"
	amove usr/lib/libweston-16/headless-backend.so
}

dev() {
	default_dev
}

libweston() {
	pkgdesc="Weston compositor library"
	amove usr/lib/libweston-16.so.*
	amove usr/lib/libweston-16/*renderer*.so
}

sha512sums="
7d3b023bc50d9cde35ca97525b1136e7ef201abee4d12c05ddd3d4084e18bb323710b9bfc5e700d7d34d6ada3cdf9521bcdc731487eaa613e14787d47a113e97  weston-16.0.0.tar.gz
fe58aa49c2f608a497c71b738bcc37b2079101916b87e400de36bd8abc1be5bd615a31136ec61f838e80170f4fb9e4d5c91bd587b501dadba9ac62a1d840ffa3  0001-libweston-tolerate-duplicate-formats.patch
"
