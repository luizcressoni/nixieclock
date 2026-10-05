# Ambiente de build para o relogio nixie (Raspberry Pi Zero / ARMv6).
#
# A imagem base e Raspberry Pi OS (Raspbian) Bullseye armhf, que ao contrario
# do Debian armhf tem userland ARMv6 -- requisito do ARM1176 do Pi Zero.
# Isso casa com o ambiente real do relogio:
#   gcc     Raspbian 10.2.1-6+rpi1
#   OpenCV  4.5 (com o modulo objdetect, usado pelo target 'camera')
#   glibc   2.31 / GLIBCXX_3.4.26
#
# Roda em x86_64 por emulacao via binfmt_misc + qemu-arm.
FROM balenalib/rpi-raspbian:bullseye

ENV DEBIAN_FRONTEND=noninteractive

# Mesmas dependencias do PKGS_DEV de tools/make_image.sh.
# O segundo bloco (autoconf..libbz2-dev) nao e do relogio: e o que o lighttpd
# 1.4.78 precisa para compilar aqui dentro, ja que o binario que roda no Pi vem
# da fonte e nao do apt (ver tools/make_image.sh). O tarball oficial do 1.4.78
# NAO traz o ./configure pronto -- so configure.ac e autogen.sh -- entao o
# autoreconf tem de rodar aqui, e por isso autoconf/automake/libtool/m4.
# Retries: sao ~260 MB vindos do archive.raspbian.org, e uma conexao resetada
# no meio derruba a imagem inteira depois de varios minutos de download.
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

# Sanidade: falha o build da imagem se algo essencial nao aparecer, dizendo
# exatamente o que faltou. Os headers do curl sao multiarch no Raspbian
# (/usr/include/arm-linux-gnueabihf/curl/), por isso a checagem usa pkg-config.
RUN for f in /usr/include/pigpio.h \
             /usr/lib/arm-linux-gnueabihf/libopencv_objdetect.so \
             /usr/lib/arm-linux-gnueabihf/cmake/opencv4/OpenCVConfig.cmake; do \
        [ -e "$f" ] || { echo "FALTANDO: $f" >&2; exit 1; }; \
    done; \
    pkg-config --exists libcurl || { echo "FALTANDO: libcurl (pkg-config)" >&2; exit 1; }; \
    pkg-config --exists libpcre || { echo "FALTANDO: libpcre (pkg-config)" >&2; exit 1; }; \
    command -v autoreconf >/dev/null || { echo "FALTANDO: autoreconf" >&2; exit 1; }; \
    command -v libtoolize >/dev/null || { echo "FALTANDO: libtoolize" >&2; exit 1; }; \
    echo "deps OK"

WORKDIR /src
ENTRYPOINT []
CMD ["/bin/bash"]
