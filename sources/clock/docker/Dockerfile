# Build environment for the nixie clock (Raspberry Pi Zero / ARMv6).
#
# Raspbian Bullseye armhf: unlike Debian armhf, an ARMv6 userland (Pi Zero's
# ARM1176). Matches the clock:
#   gcc     Raspbian 10.2.1-6+rpi1
#   OpenCV  4.5 (with the objdetect module, used by the 'camera' target)
#   glibc   2.31 / GLIBCXX_3.4.26
#
# Runs on x86_64 via binfmt_misc + qemu-arm.
FROM balenalib/rpi-raspbian:bullseye

ENV DEBIAN_FRONTEND=noninteractive

# Same as PKGS_DEV in tools/make_image.sh.
# autoconf..libbz2-dev are for lighttpd 1.4.78, built from source: its tarball
# ships no ./configure, so autoreconf runs here.
# Retries: ~260 MB from archive.raspbian.org; one reset kills the whole build.
RUN apt-get update \
 && apt-get -o Acquire::Retries=5 install -y --no-install-recommends \
        build-essential \
        cmake \
        make \
        pkg-config \
        ca-certificates \
        libopencv-dev \
        libcurl4-openssl-dev \
        pigpio \
        libpigpio-dev \
        autoconf \
        automake \
        libtool \
        m4 \
        libpcre3-dev \
        zlib1g-dev \
        libbz2-dev \
    && rm -rf /var/lib/apt/lists/*

# Fail early, naming what is missing. curl headers are multiarch on Raspbian,
# hence pkg-config.
RUN for f in /usr/include/pigpio.h \
             /usr/lib/arm-linux-gnueabihf/libopencv_objdetect.so \
             /usr/lib/arm-linux-gnueabihf/cmake/opencv4/OpenCVConfig.cmake; do \
        [ -e "$f" ] || { echo "MISSING: $f" >&2; exit 1; }; \
    done; \
    pkg-config --exists libcurl || { echo "MISSING: libcurl (pkg-config)" >&2; exit 1; }; \
    pkg-config --exists libpcre || { echo "MISSING: libpcre (pkg-config)" >&2; exit 1; }; \
    command -v autoreconf >/dev/null || { echo "MISSING: autoreconf" >&2; exit 1; }; \
    command -v libtoolize >/dev/null || { echo "MISSING: libtoolize" >&2; exit 1; }; \
    echo "deps OK"

WORKDIR /src
ENTRYPOINT []
CMD ["/bin/bash"]
