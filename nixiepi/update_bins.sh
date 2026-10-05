#! /bin/bash
#
# Installs the clock binaries and restarts the services. Run this ON THE PI.
#
# The binaries come from dockerbuild/, produced on a PC by
# sources/clock/docker/build.sh. Sync the tree to the Pi first, e.g.:
#
#     rsync -av --delete <pc>:develop/full_nixie/sources/clock/dockerbuild/ \
#           /home/pi/sources/clock/dockerbuild/
#
# Or point the script somewhere else:
#
#     BUILD_DIR=/tmp/dockerbuild ./update_bins.sh

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR=${BUILD_DIR:-/home/pi/sources/clock/dockerbuild}

# Check everything before stopping the services: an incomplete copy must not
# leave the clock down or restart it on stale binaries.
for f in camera nixie nixie.cgi liblogger.so; do
    if [ ! -f "$BUILD_DIR/$f" ]; then
        echo "ERROR: $BUILD_DIR/$f not found -- build it first (docker/build.sh)" >&2
        exit 1
    fi
done

echo "Stopping services..."
"$SCRIPT_DIR/services.sh" disable

echo "Copying files..."
cp "$BUILD_DIR/camera"    /home/pi/nixiepi/camera
cp "$BUILD_DIR/nixie"     /home/pi/nixiepi/nixie
cp "$BUILD_DIR/nixie.cgi" /home/pi/www/cgi-bin/nixie.cgi
sudo cp "$BUILD_DIR/liblogger.so" /usr/local/lib/liblogger.so

# No RUNPATH in the binaries: liblogger.so is found through the ld cache,
# and /usr/local/lib only gets there after ldconfig.
sudo ldconfig

echo "Starting services..."
"$SCRIPT_DIR/services.sh" enable
