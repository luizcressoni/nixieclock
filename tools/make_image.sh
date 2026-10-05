#!/usr/bin/env bash
#
# Builds the nixie clock SD card image from scratch.
#
#   ./tools/make_image.sh --password PASSWORD [options]
#
# Output is build/nixie-clock.img, ready for Raspberry Pi Imager
# ("Use custom image") or dd. The script NEVER writes to a device:
# it only touches the image file inside build/.
#
# Steps:
#   1. download Raspberry Pi OS Bullseye armhf lite and check its sha256
#   2. unpack and grow the image (the stock rootfs has no headroom)
#   3. build nixie/camera/nixie.cgi/liblogger.so in the ARM container
#   4. build lighttpd 1.4.78 in the same container
#   5. install dependencies inside the image, through an emulated chroot
#   6. copy binaries, pages, scripts, services and configs
#   7. prepare first boot (user, ssh, Wi-Fi, config.txt)
#
# Bullseye, not Bookworm: the Pi Zero is ARMv6, and Bullseye is the last
# Raspberry Pi OS with an ARMv6 userland. armhf, not arm64: same reason.
# lighttpd from source, not apt: it is what runs on the working clock,
# built with --without-pcre2.

set -euo pipefail

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="$PROJECT_DIR/build"
CACHE_DIR="$BUILD_DIR/cache"
MNT_BOOT="$BUILD_DIR/mnt/boot"
MNT_ROOT="$BUILD_DIR/mnt/root"

# --------------------------------------------------------------- base image
# Archived path: "oldstable" is the label the site used back then, not a
# moving target.
IMG_URL="https://downloads.raspberrypi.com/raspios_oldstable_lite_armhf/images/raspios_oldstable_lite_armhf-2024-10-28/2024-10-22-raspios-bullseye-armhf-lite.img.xz"
IMG_XZ="2024-10-22-raspios-bullseye-armhf-lite.img.xz"
IMG_SHA256="45dd65d579ec2b106a1e3181032144406eab61df892fcd2da8d83382fa4f7e51"
OUT_IMG="$BUILD_DIR/nixie-clock.img"

# lighttpd follows the same path as the image: download, check sha256, cache
# in build/cache, then build from source (see step 3).
# The build image tag has a single owner, docker/build.sh. A second copy here
# would silently drift -- or, worse, find a stale container.
BUILD_SH="$PROJECT_DIR/sources/clock/docker/build.sh"
BUILD_IMAGE=$(sed -n 's/^IMAGE=\([^ \t#]*\).*/\1/p' "$BUILD_SH" | head -1)

LIGHTTPD_VER=1.4.78
LIGHTTPD_DIR="lighttpd-$LIGHTTPD_VER"
LIGHTTPD_TGZ="lighttpd-$LIGHTTPD_VER.tar.gz"
LIGHTTPD_URL="https://download.lighttpd.net/lighttpd/releases-1.4.x/$LIGHTTPD_TGZ"
LIGHTTPD_SHA256="6f1a563a23aafc649a76c40ae009445f327296a0d0c352690fbfedc46aea271d"
LIGHTTPD_TAR="$CACHE_DIR/$LIGHTTPD_TGZ"

# ------------------------------------------------------------------- options

USERNAME=pi            # /home/pi is hardcoded (json_parser.cpp, lighttpd.conf)
PASSWORD=""
HOSTNAME=nixie
TIMEZONE="America/Sao_Paulo"
COUNTRY=BR
WIFI_SSID=""
WIFI_PSK=""
WEATHER_KEY=""
LATITUDE=""
LONGITUDE=""
GROW_MB=1536
WITH_DEVTOOLS=0
COMPRESS=0
KEEP_IMAGE=0

usage() {
    sed -n '3,23p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    cat <<'MSG'

Options:
  --password PASSWORD  password for user pi (required)
  --hostname NAME      host name                            (default: nixie)
  --timezone TZ        time zone                            (default: America/Sao_Paulo)
  --country XX         Wi-Fi country code                   (default: BR)
  --wifi-ssid SSID     network for first boot; without it the
                       clock brings up the "NixieClock" hotspot
  --wifi-psk PASSWORD  password for that network
  --weather-key KEY    weatherapi.com key, stored in nixie.json
  --latitude N         weather forecast latitude
  --longitude N        weather forecast longitude
  --grow-mb N          extra rootfs space, in MB            (default: 1536)
  --with-devtools      preinstall the toolchain, to build on the Pi
                       itself (~1.2 GB more). Without it, the card's
                       readme.txt explains how to install it later
  --compress           also produce nixie-clock.img.xz
  --keep-image         reuse build/nixie-clock.img instead of rebuilding
  -h, --help           this help

Nothing here writes to /dev/sdX. To flash the card afterwards:
  xzcat build/nixie-clock.img.xz | sudo dd of=/dev/sdX bs=4M conv=fsync status=progress
  (or use Raspberry Pi Imager with "Use custom image")
MSG
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --password)      PASSWORD="$2"; shift 2 ;;
        --hostname)      HOSTNAME="$2"; shift 2 ;;
        --timezone)      TIMEZONE="$2"; shift 2 ;;
        --country)       COUNTRY="$2"; shift 2 ;;
        --wifi-ssid)     WIFI_SSID="$2"; shift 2 ;;
        --wifi-psk)      WIFI_PSK="$2"; shift 2 ;;
        --weather-key)   WEATHER_KEY="$2"; shift 2 ;;
        --latitude)      LATITUDE="$2"; shift 2 ;;
        --longitude)     LONGITUDE="$2"; shift 2 ;;
        --grow-mb)       GROW_MB="$2"; shift 2 ;;
        --with-devtools) WITH_DEVTOOLS=1; shift ;;
        --compress)      COMPRESS=1; shift ;;
        --keep-image)    KEEP_IMAGE=1; shift ;;
        -h|--help)       usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

say()  { printf '\n\033[1;36m>> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m!! %s\033[0m\n' "$*" >&2; }
die()  { printf '\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- pre-flight

[[ -n "$PASSWORD" ]] || die "--password is required (Bullseye no longer creates a default user)."
[[ -n "$WIFI_SSID" && -z "$WIFI_PSK" ]] && die "--wifi-ssid needs --wifi-psk."

for t in curl xz sha256sum truncate sfdisk losetup mount umount rsync openssl docker; do
    command -v "$t" >/dev/null 2>&1 || die "missing host tool '$t'."
done

[[ -e /proc/sys/fs/binfmt_misc/qemu-arm ]] || \
    die "binfmt qemu-arm not registered. Run: sudo apt-get install -y qemu-user-binfmt (older Ubuntu: qemu-user-static binfmt-support)"

# Ask the kernel for the interpreter instead of guessing: it used to be
# qemu-arm-static, now it is a static qemu-arm. With the F flag the kernel has
# it preloaded; the copy into the chroot only matters without it.
QEMU_ARM=$(sed -n 's/^interpreter //p' /proc/sys/fs/binfmt_misc/qemu-arm)
[[ -x "$QEMU_ARM" ]] || die "binfmt points to '$QEMU_ARM', which does not exist on the host."
[[ -n "$BUILD_IMAGE" ]] || die "could not read the build image name from $BUILD_SH"

if ! docker info >/dev/null 2>&1; then
    sudo -n docker info >/dev/null 2>&1 || \
        die "no access to the Docker daemon. See sources/clock/docker/build.sh"
fi

say "asking for sudo up front (loop device, mount and chroot need root)"
sudo -v || die "sudo denied."

mkdir -p "$CACHE_DIR" "$MNT_BOOT" "$MNT_ROOT"

# ------------------------------------------------------------------ cleanup
# A leftover loop device or bind mount breaks the next run. Runs on every exit.
LOOP=""
cleanup() {
    local rc=$?
    set +e
    if [[ -n "$LOOP" ]]; then
        for m in "$MNT_ROOT/dev/pts" "$MNT_ROOT/dev" "$MNT_ROOT/proc" "$MNT_ROOT/sys" \
                 "$MNT_ROOT/boot" "$MNT_BOOT" "$MNT_ROOT"; do
            mountpoint -q "$m" && sudo umount -l "$m"
        done
        sudo losetup -d "$LOOP" 2>/dev/null
    fi
    set -e
    return $rc
}
trap cleanup EXIT

# ------------------------------------------------------------- 1. download

if [[ ! -f "$CACHE_DIR/$IMG_XZ" ]]; then
    say "downloading $IMG_XZ (~366 MB)"
    curl -fL --progress-bar -o "$CACHE_DIR/$IMG_XZ.part" "$IMG_URL"
    mv "$CACHE_DIR/$IMG_XZ.part" "$CACHE_DIR/$IMG_XZ"
fi

say "checking sha256"
echo "$IMG_SHA256  $CACHE_DIR/$IMG_XZ" | sha256sum -c - || \
    die "sha256 mismatch. Delete $CACHE_DIR/$IMG_XZ and run again."

# A local copy in sources/ (old clone, or offline build) seeds the cache.
if [[ ! -f "$LIGHTTPD_TAR" && -f "$PROJECT_DIR/sources/$LIGHTTPD_TGZ" ]]; then
    cp "$PROJECT_DIR/sources/$LIGHTTPD_TGZ" "$LIGHTTPD_TAR"
fi
if [[ ! -f "$LIGHTTPD_TAR" ]]; then
    say "downloading $LIGHTTPD_TGZ"
    curl -fL --progress-bar -o "$LIGHTTPD_TAR.part" "$LIGHTTPD_URL"
    mv "$LIGHTTPD_TAR.part" "$LIGHTTPD_TAR"
fi
echo "$LIGHTTPD_SHA256  $LIGHTTPD_TAR" | sha256sum -c - || \
    die "lighttpd sha256 mismatch. Delete $LIGHTTPD_TAR and run again."

# ------------------------------------------------- 2. unpack and grow image

if [[ $KEEP_IMAGE -eq 1 && -f "$OUT_IMG" ]]; then
    say "reusing $OUT_IMG (--keep-image)"
else
    say "unpacking to $OUT_IMG"
    rm -f "$OUT_IMG"
    xz -dc "$CACHE_DIR/$IMG_XZ" > "$OUT_IMG"

    say "growing the image by ${GROW_MB} MB"
    truncate -s "+${GROW_MB}M" "$OUT_IMG"
    # Partition 2 is the last one: stretch it to the end of the file.
    # ",+" means "all the rest" to sfdisk.
    echo ", +" | sfdisk -N 2 --no-reread --force "$OUT_IMG" >/dev/null
fi

# ---------------------------------------------- 3. build what goes on the Pi

say "building the clock in the ARM container"
"$PROJECT_DIR/sources/clock/docker/build.sh"

DOCKERBUILD="$PROJECT_DIR/sources/clock/dockerbuild"
for f in nixie camera nixie.cgi liblogger.so; do
    [[ -f "$DOCKERBUILD/$f" ]] || die "the build did not produce $f"
done

LIGHTTPD_STAGE="$BUILD_DIR/lighttpd-stage"
if [[ ! -x "$LIGHTTPD_STAGE/usr/local/sbin/lighttpd" ]]; then
    say "building lighttpd 1.4.78 in the ARM container (slow: everything is emulated)"
    rm -rf "$BUILD_DIR/$LIGHTTPD_DIR" "$LIGHTTPD_STAGE"
    tar -xzf "$LIGHTTPD_TAR" -C "$BUILD_DIR"
    # Same image and user as the clock build: right owner, same glibc.
    # Runs before mounting, so the $PROJECT_DIR bind never sees build/mnt.
    # --without-pcre2 matches config.status on the working clock.
    DOCKER=(docker); docker info >/dev/null 2>&1 || DOCKER=(sudo docker)
    "${DOCKER[@]}" run --rm \
        --platform linux/arm/v6 \
        --user "$(id -u):$(id -g)" \
        -v "$PROJECT_DIR":/src -w "/src/build/$LIGHTTPD_DIR" \
        -e HOME=/tmp \
        "$BUILD_IMAGE" \
        /bin/bash -euo pipefail -c "
            # The 1.4.78 tarball ships no ./configure, only autogen.sh.
            ./autogen.sh
            ./configure --prefix=/usr/local --without-pcre2 >/dev/null
            make -j\$(nproc) >/dev/null
            make install DESTDIR=/src/build/lighttpd-stage >/dev/null
        "
else
    say "lighttpd already built in build/lighttpd-stage"
fi
[[ -x "$LIGHTTPD_STAGE/usr/local/sbin/lighttpd" ]] || die "lighttpd was not built"

# ----------------------------------------------------- 4. mount the image

say "attaching the image to a loop device"
LOOP=$(sudo losetup --find --show --partscan "$OUT_IMG")
[[ -b "${LOOP}p1" && -b "${LOOP}p2" ]] || die "the kernel did not expose the partitions of $LOOP"

sudo e2fsck -pf "${LOOP}p2" >/dev/null || true   # resize2fs wants a clean fsck
sudo resize2fs "${LOOP}p2" >/dev/null

sudo mount "${LOOP}p2" "$MNT_ROOT"
sudo mount "${LOOP}p1" "$MNT_BOOT"
df -h --output=target,size,avail "$MNT_ROOT" "$MNT_BOOT" | sed 's/^/   /'

# ---------------------------------------------- 5. emulated chroot for apt

say "preparing the emulated chroot"
# same path as on the host: where the kernel looks without the F flag
QEMU_IN_IMAGE=0
if ! sudo test -e "$MNT_ROOT$QEMU_ARM"; then
    sudo install -D -m755 "$QEMU_ARM" "$MNT_ROOT$QEMU_ARM"
    QEMU_IN_IMAGE=1
fi
# Raspbian preloads libarmmem via /etc/ld.so.preload, which kills every chroot
# binary under qemu with "cannot be preloaded". Moved aside, restored at the end.
if sudo test -f "$MNT_ROOT/etc/ld.so.preload"; then
    sudo mv "$MNT_ROOT/etc/ld.so.preload" "$MNT_ROOT/etc/ld.so.preload.disabled"
fi
# The chroot shares the host network, so the host resolv.conf works. The
# original is restored so the image does not ship the builder's DNS.
sudo cp -a "$MNT_ROOT/etc/resolv.conf" "$MNT_ROOT/etc/resolv.conf.img" 2>/dev/null || true
sudo cp -L /etc/resolv.conf "$MNT_ROOT/etc/resolv.conf"
sudo mount -t proc  none  "$MNT_ROOT/proc"
sudo mount -t sysfs none  "$MNT_ROOT/sys"
sudo mount --bind /dev     "$MNT_ROOT/dev"
sudo mount --bind /dev/pts "$MNT_ROOT/dev/pts"
# rpi-eeprom and dphys-swapfile postinst scripts need /boot (partition 1).
sudo mount --bind "$MNT_BOOT" "$MNT_ROOT/boot"

in_chroot() { sudo chroot "$MNT_ROOT" /bin/bash -euo pipefail -c "$1"; }

# dnsmasq and hostapd postinst try to start the service; with no systemd in the
# chroot invoke-rc.d would fail and abort apt.
sudo tee "$MNT_ROOT/usr/sbin/policy-rc.d" >/dev/null <<'EOF'
#!/bin/sh
exit 101
EOF
sudo chmod +x "$MNT_ROOT/usr/sbin/policy-rc.d"

# Runtime only: the container builds, not the Pi. -dev packages come with
# --with-devtools. List taken from "readelf -d" of the three binaries:
# camera needs videoio/objdetect/imgproc/core, nixie needs pigpio and curl.
# imgcodecs: videoio depends on it (and EXPORT_FACE_JPG writes jpgs).
# No highgui on purpose: it drags GTK and X11 into a Lite image.
PKGS_RUNTIME="libopencv-videoio4.5 libopencv-objdetect4.5 libopencv-imgproc4.5 \
libopencv-core4.5 libopencv-imgcodecs4.5 \
libcurl4 libpigpio1 pigpio pigpio-tools dnsmasq hostapd i2c-tools"
PKGS_DEV="build-essential cmake pkg-config git libopencv-dev libcurl4-openssl-dev libpigpio-dev"

say "installing dependencies inside the image (emulated, may take a while)"
# Acquire::Retries: same reason as in the Dockerfile.
in_chroot "export DEBIAN_FRONTEND=noninteractive
           apt-get update
           apt-get -o Acquire::Retries=5 install -y --no-install-recommends $PKGS_RUNTIME"

if [[ $WITH_DEVTOOLS -eq 1 ]]; then
    say "installing the toolchain too (--with-devtools)"
    in_chroot "export DEBIAN_FRONTEND=noninteractive
               apt-get -o Acquire::Retries=5 install -y --no-install-recommends $PKGS_DEV"
fi

# -------------------------------------------------------- 6. copy the project

say "copying the clock to /home/pi"
HOME_PI="$MNT_ROOT/home/$USERNAME"
sudo mkdir -p "$HOME_PI/nixiepi" "$HOME_PI/www"

sudo install -m755 "$DOCKERBUILD/nixie"  "$HOME_PI/nixiepi/nixie"
sudo install -m755 "$DOCKERBUILD/camera" "$HOME_PI/nixiepi/camera"
sudo install -m755 "$PROJECT_DIR/nixiepi/services.sh"   "$HOME_PI/nixiepi/services.sh"
sudo install -m755 "$PROJECT_DIR/nixiepi/update_bins.sh" "$HOME_PI/nixiepi/update_bins.sh"

# Face cascades. Picked in the site's Detection tab
# (detection.face_cascade in nixie.json); default is the improved LBP.
for x in haarcascade_frontalface_default.xml lbpcascade_frontalface.xml \
         lbpcascade_frontalface_improved.xml; do
    sudo install -m644 "$PROJECT_DIR/nixiepi/$x" "$HOME_PI/nixiepi/$x"
done

say "copying the web site"
# -a keeps symlinks, which www/logs/nixie.txt must stay.
sudo rsync -a --delete \
    --exclude '.idea/' --exclude 'json/nixie.json' \
    "$PROJECT_DIR/www/" "$HOME_PI/www/"
sudo install -d -m755 "$HOME_PI/www/cgi-bin" "$HOME_PI/www/json" "$HOME_PI/www/logs"
# The "Logs" tab fetches logs/nixie.txt; liblogger writes /tmp/nixie.txt.
sudo ln -sfn /tmp/nixie.txt "$HOME_PI/www/logs/nixie.txt"
sudo install -m755 "$DOCKERBUILD/nixie.cgi" "$HOME_PI/www/cgi-bin/nixie.cgi"

# The card's nixie.json ALWAYS comes from the template, never from the local
# nixie.json -- that one is the builder's home config.
say "writing the initial configuration"
TMP_JSON=$(mktemp)
cp "$PROJECT_DIR/www/json/nixie.default.json" "$TMP_JSON"
WEATHER_KEY="$WEATHER_KEY" LATITUDE="$LATITUDE" LONGITUDE="$LONGITUDE" \
WIFI_SSID="$WIFI_SSID" WIFI_PSK="$WIFI_PSK" python3 - "$TMP_JSON" <<'PY'
import json, os, sys
p = sys.argv[1]
d = json.load(open(p))
if os.environ.get('WEATHER_KEY'): d['maps']['apikey']    = os.environ['WEATHER_KEY']
if os.environ.get('LATITUDE'):    d['maps']['latitude']  = float(os.environ['LATITUDE'])
if os.environ.get('LONGITUDE'):   d['maps']['longitude'] = float(os.environ['LONGITUDE'])
if os.environ.get('WIFI_SSID'):   d['wifi']['ssid']      = os.environ['WIFI_SSID']
if os.environ.get('WIFI_PSK'):    d['wifi']['password']  = os.environ['WIFI_PSK']
json.dump(d, open(p, 'w'), indent='\t', ensure_ascii=False)
PY
sudo install -m644 "$TMP_JSON" "$HOME_PI/www/json/nixie.json"

# The clock reads system time; NTP servers only matter to systemd-timesyncd.
# nixie.cgi rewrites this file from the NTP tab; it starts from nixie.json.
NTP_SERVERS=$(python3 -c '
import json, sys
n = json.load(open(sys.argv[1])).get("ntp", {})
print(" ".join(v for v in (n.get("server_%d" % i, "") for i in range(4)) if v))' "$TMP_JSON")
rm -f "$TMP_JSON"
sudo install -d -m755 "$MNT_ROOT/etc/systemd/timesyncd.conf.d"
printf '# Written by the clock'"'"'s web page (nixie.cgi). Edits here are overwritten.\n[Time]\nNTP=%s\n' \
    "$NTP_SERVERS" | sudo tee "$MNT_ROOT/etc/systemd/timesyncd.conf.d/nixie.conf" >/dev/null

# Some code opens /home/pi/nixie.json; on the working card it is a link.
sudo ln -sfn www/json/nixie.json "$HOME_PI/nixie.json"

sudo install -m644 "$PROJECT_DIR/readme.txt" "$HOME_PI/readme.txt"

# The card is self-sufficient: sources to rebuild, projeto/ to fix the hardware.
# --with-devtools only decides whether the toolchain comes preinstalled.
# update_bins.sh looks for binaries in sources/clock/dockerbuild.
say "copying sources and projeto/ to the card"
# rsync only creates the last directory, and /home/pi/sources does not exist yet.
sudo install -d -m755 "$HOME_PI/sources/clock/dockerbuild"
sudo rsync -a --exclude '.git/' --exclude '.idea/' --exclude '.vscode/' \
    --exclude 'build/' --exclude 'rpibuild/' --exclude 'dockerbuild/' \
    --exclude 'docs/' \
    "$PROJECT_DIR/sources/clock/" "$HOME_PI/sources/clock/"
# ~100 MB of schematics, datasheets and photos; fits the default --grow-mb.
sudo rsync -a "$PROJECT_DIR/projeto/" "$HOME_PI/projeto/"

say "installing libraries and lighttpd into /usr/local"
sudo install -m755 "$DOCKERBUILD/liblogger.so" "$MNT_ROOT/usr/local/lib/liblogger.so"
sudo cp -a "$LIGHTTPD_STAGE/usr/local/." "$MNT_ROOT/usr/local/"
# make install ran with the host uid and cp -a kept it; /usr/local is root:root.
sudo chown -R 0:0 "$MNT_ROOT/usr/local"

say "installing scripts and services"
for s in check_ssid.sh check_wifi_or_hotspot.sh http.sh; do
    sudo install -m755 "$PROJECT_DIR/sources/scripts/usr/local/bin/$s" "$MNT_ROOT/usr/local/bin/$s"
done
for u in nixie.service camera.service wifi-check.service check_ssid.service lighttpd-custom.service; do
    sudo install -m644 "$PROJECT_DIR/sources/configs/etc/systemd/system/$u" \
        "$MNT_ROOT/etc/systemd/system/$u"
done
sudo install -m644 "$PROJECT_DIR/sources/configs/etc/dnsmasq.conf" "$MNT_ROOT/etc/dnsmasq.conf"
sudo install -m644 "$PROJECT_DIR/sources/configs/etc/dhcpcd.conf"  "$MNT_ROOT/etc/dhcpcd.conf"
sudo install -d -m755 "$MNT_ROOT/etc/hostapd"
sudo install -m600 "$PROJECT_DIR/sources/configs/etc/hostapd/hostapd.conf" "$MNT_ROOT/etc/hostapd/hostapd.conf"
sudo install -m644 "$PROJECT_DIR/sources/configs/etc/default/hostapd" "$MNT_ROOT/etc/default/hostapd"
# "allow-hotplug wlan0" is not stock, but the working clock has it.
sudo install -m644 "$PROJECT_DIR/sources/configs/etc/network/interfaces" "$MNT_ROOT/etc/network/interfaces"

# ------------------------------------------------------ 7. enable services

say "enabling services"
# lighttpd-custom.service stays DISABLED on purpose: check_wifi_or_hotspot.sh
# starts the server after choosing Wi-Fi or hotspot. Both would fight for port 80.
# SYSTEMD_OFFLINE=1: when systemctl misses the chroot it fails with an unrelated
# "Failed to connect to bus". Only the symlinks are wanted here.
in_chroot "SYSTEMD_OFFLINE=1 systemctl enable nixie.service camera.service wifi-check.service check_ssid.service" \
    || die "systemctl enable failed; the card would boot without the clock."
in_chroot "SYSTEMD_OFFLINE=1 systemctl disable lighttpd-custom.service" 2>/dev/null || true
# hostapd and dnsmasq start on demand from check_wifi_or_hotspot.sh.
# Disabled, not masked: masking would stop the script too.
in_chroot "SYSTEMD_OFFLINE=1 systemctl disable hostapd dnsmasq" 2>/dev/null || true
in_chroot "SYSTEMD_OFFLINE=1 systemctl unmask hostapd dnsmasq" 2>/dev/null || true
in_chroot "ldconfig"

# Proof the package list is complete: better now than on first boot, headless.
say "checking that everything links inside the image"
missing=$(in_chroot '
    for b in /home/pi/nixiepi/nixie /home/pi/nixiepi/camera \
             /home/pi/www/cgi-bin/nixie.cgi /usr/local/sbin/lighttpd; do
        # || true: grep exits 1 on no match, and pipefail would abort the loop
        ldd "$b" 2>/dev/null | grep "not found" | sed "s|^|  $b: |" || true
    done') || true
if [[ -n "$missing" ]]; then
    echo "$missing" >&2
    die "missing libraries in the image (see above)."
fi
echo "   nixie, camera, nixie.cgi and lighttpd: all libraries resolved."

# ------------------------------------------------- 8. first boot and hardware

say "configuring first boot"

# Bullseye has no default user: without userconf.txt it waits on the console.
# /home/pi is hardcoded and already populated, so uid/gid must be 1000:1000.
HASH=$(openssl passwd -6 "$PASSWORD")

# userconf-pi RENAMES the existing uid 1000 user, it never creates one. Should
# the base image drop that user, userconf.txt would be a no-op and the card
# would boot with no login at all -- hence the check below.
if ! in_chroot "getent passwd 1000 >/dev/null"; then
    warn "the base image has no uid 1000 user; creating '$USERNAME' here."
    in_chroot "groupadd -g 1000 '$USERNAME'
               useradd -u 1000 -g 1000 -M -d '/home/$USERNAME' -s /bin/bash '$USERNAME'
               usermod -aG adm,sudo,video,audio,plugdev,gpio,i2c,spi,netdev '$USERNAME' || true"
fi
printf '%s:%s\n' "$USERNAME" "$HASH" | sudo tee "$MNT_BOOT/userconf.txt" >/dev/null
sudo chmod 600 "$MNT_BOOT/userconf.txt"

# Everything copied into /home/pi must belong to the login uid.
sudo chown -R 1000:1000 "$HOME_PI"

sudo touch "$MNT_BOOT/ssh"                       # ssh on from first boot
echo "$HOSTNAME" | sudo tee "$MNT_ROOT/etc/hostname" >/dev/null
sudo sed -i "s/^127\.0\.1\.1.*/127.0.1.1\t$HOSTNAME/" "$MNT_ROOT/etc/hosts"
echo "$TIMEZONE" | sudo tee "$MNT_ROOT/etc/timezone" >/dev/null
sudo ln -sfn "/usr/share/zoneinfo/$TIMEZONE" "$MNT_ROOT/etc/localtime"

if [[ -n "$WIFI_SSID" ]]; then
    say "writing the initial Wi-Fi network"
    # Same format wifi.cpp writes, so check_ssid.sh parses both alike.
    sudo install -d -m755 "$MNT_ROOT/etc/wpa_supplicant"
    sudo tee "$MNT_ROOT/etc/wpa_supplicant/wpa_supplicant.conf" >/dev/null <<EOF
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
ap_scan=1

update_config=1

country=$COUNTRY
network={
    ssid="$WIFI_SSID"
    psk="$WIFI_PSK"
}
EOF
    sudo chmod 600 "$MNT_ROOT/etc/wpa_supplicant/wpa_supplicant.conf"
else
    warn "no --wifi-ssid: the clock will bring up the 'NixieClock' hotspot and show 192.168.4.1 on the tubes."
fi

sudo sed -i "s/^country_code=.*/country_code=$COUNTRY/" "$MNT_ROOT/etc/hostapd/hostapd.conf"

say "adjusting config.txt and modules"
# Idempotent; only what Bullseye does not set by default.
#   i2c_arm + i2c-dev + i2c-rtc : the DS3231 keeps time through power cuts
#   gpu_mem/start_x            : as on the working clock
#   enable_uart                : serial console, for when there is no network
if ! sudo grep -q '^# --- nixie clock ---' "$MNT_BOOT/config.txt"; then
    sudo tee -a "$MNT_BOOT/config.txt" >/dev/null <<'EOF'

# --- nixie clock ---
dtparam=i2c_arm=on
enable_uart=1
gpu_mem=128
start_x=1
dtoverlay=i2c-rtc,ds3231
EOF
fi
sudo grep -qx 'i2c-dev' "$MNT_ROOT/etc/modules" || \
    echo 'i2c-dev' | sudo tee -a "$MNT_ROOT/etc/modules" >/dev/null

# Building OpenCV on a 512 MB Zero needs swap. Free without --with-devtools.
sudo sed -i 's/^CONF_SWAPSIZE=.*/CONF_SWAPSIZE=512/' "$MNT_ROOT/etc/dphys-swapfile"

# ---------------------------------------------------------------- 9. finish

say "unmounting"
sudo rm -f "$MNT_ROOT/usr/sbin/policy-rc.d"
[[ $QEMU_IN_IMAGE -eq 1 ]] && sudo rm -f "$MNT_ROOT$QEMU_ARM"
if sudo test -e "$MNT_ROOT/etc/resolv.conf.img"; then
    sudo mv "$MNT_ROOT/etc/resolv.conf.img" "$MNT_ROOT/etc/resolv.conf"
else
    sudo truncate -s 0 "$MNT_ROOT/etc/resolv.conf"   # dhcpcd rewrites it on boot
fi
if sudo test -f "$MNT_ROOT/etc/ld.so.preload.disabled"; then
    sudo mv "$MNT_ROOT/etc/ld.so.preload.disabled" "$MNT_ROOT/etc/ld.so.preload"
fi

sudo umount "$MNT_ROOT/dev/pts" "$MNT_ROOT/dev" "$MNT_ROOT/proc" "$MNT_ROOT/sys" "$MNT_ROOT/boot"
sudo umount "$MNT_BOOT" "$MNT_ROOT"
sudo losetup -d "$LOOP"; LOOP=""

if [[ $COMPRESS -eq 1 ]]; then
    say "compressing (slow)"
    xz -T0 -9 -kf "$OUT_IMG"
fi

say "done"
cat <<MSG

   image  : $OUT_IMG  ($(du -h "$OUT_IMG" | cut -f1))
$( [[ $COMPRESS -eq 1 ]] && echo "   .xz    : $OUT_IMG.xz  ($(du -h "$OUT_IMG.xz" | cut -f1))" )
   user   : $USERNAME   hostname: $HOSTNAME   timezone: $TIMEZONE
   Wi-Fi  : ${WIFI_SSID:-<none: brings up the 'NixieClock' hotspot>}
   weather: $( [[ -n "$WEATHER_KEY" ]] && echo "key stored" || echo "no key -- set it in the Location tab of the clock's site" )

   Flash the card:
     sudo dd if=$OUT_IMG of=/dev/sdX bs=4M conv=fsync status=progress
     ...or Raspberry Pi Imager -> "Use custom image"

   WARNING: check /dev/sdX with 'lsblk' first. This script never writes to
   a device precisely so that choice is always yours.
MSG
