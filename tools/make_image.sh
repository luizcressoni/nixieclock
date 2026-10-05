#!/usr/bin/env bash
#
# Monta, do zero, a imagem de cartao SD do relogio nixie.
#
#   ./tools/make_image.sh --password SENHA [opcoes]
#
# O resultado e build/nixie-clock.img, pronto para o Raspberry Pi Imager
# ("Use custom image") ou para um dd. O script NAO escreve em nenhum
# dispositivo: ele so mexe no arquivo de imagem dentro de build/.
#
# O que ele faz, em ordem:
#   1. baixa o Raspberry Pi OS Bullseye armhf lite e confere o sha256
#   2. descompacta e cresce a imagem (o rootfs original nao tem folga)
#   3. compila nixie/camera/nixie.cgi/liblogger.so no container ARM
#   4. compila o lighttpd 1.4.78 no mesmo container
#   5. instala as dependencias dentro da imagem, via chroot emulado
#   6. copia binarios, paginas, scripts, services e configs
#   7. prepara o primeiro boot (usuario, ssh, wi-fi, config.txt)
#
# Por que Bullseye e nao Bookworm: o Pi Zero e ARMv6, e e a ultima versao do
# Raspberry Pi OS com userland ARMv6. Por que armhf e nao arm64: mesma coisa.
# Por que lighttpd da fonte e nao do apt: e o que esta no relogio que funciona,
# compilado com --without-pcre2.

set -euo pipefail

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="$PROJECT_DIR/build"
CACHE_DIR="$BUILD_DIR/cache"
MNT_BOOT="$BUILD_DIR/mnt/boot"
MNT_ROOT="$BUILD_DIR/mnt/root"

# ---------------------------------------------------------------- a imagem base
# Caminho versionado do arquivo: "oldstable" no nome do diretorio e so o rotulo
# que o site usava quando este build saiu, e nao acompanha o Debian.
IMG_URL="https://downloads.raspberrypi.com/raspios_oldstable_lite_armhf/images/raspios_oldstable_lite_armhf-2024-10-28/2024-10-22-raspios-bullseye-armhf-lite.img.xz"
IMG_XZ="2024-10-22-raspios-bullseye-armhf-lite.img.xz"
IMG_SHA256="45dd65d579ec2b106a1e3181032144406eab61df892fcd2da8d83382fa4f7e51"
OUT_IMG="$BUILD_DIR/nixie-clock.img"

# O lighttpd tambem vem de fora, e pelo mesmo caminho da imagem: baixa, confere
# o sha256, guarda em build/cache. O binario que roda no relogio e compilado
# desta fonte, nao instalado pelo apt (ver o bloco de build mais abaixo).
# O nome da imagem de build tem um dono so: o docker/build.sh. Repetir a tag
# aqui significava que bumpar uma e esquecer a outra passava batido ate o
# container nao existir -- ou, pior, existir velho.
BUILD_SH="$PROJECT_DIR/sources/clock/docker/build.sh"
BUILD_IMAGE=$(sed -n 's/^IMAGE=\([^ \t#]*\).*/\1/p' "$BUILD_SH" | head -1)

LIGHTTPD_VER=1.4.78
LIGHTTPD_DIR="lighttpd-$LIGHTTPD_VER"
LIGHTTPD_TGZ="lighttpd-$LIGHTTPD_VER.tar.gz"
LIGHTTPD_URL="https://download.lighttpd.net/lighttpd/releases-1.4.x/$LIGHTTPD_TGZ"
LIGHTTPD_SHA256="6f1a563a23aafc649a76c40ae009445f327296a0d0c352690fbfedc46aea271d"
LIGHTTPD_TAR="$CACHE_DIR/$LIGHTTPD_TGZ"

# -------------------------------------------------------------------- opcoes

USERNAME=pi            # /home/pi esta hardcoded no codigo (json_parser.cpp, lighttpd.conf)
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

Opcoes:
  --password SENHA     senha do usuario pi (obrigatoria)
  --hostname NOME      nome da maquina                      (padrao: nixie)
  --timezone TZ        fuso                                 (padrao: America/Sao_Paulo)
  --country XX         codigo de pais do wi-fi              (padrao: BR)
  --wifi-ssid SSID     rede para o primeiro boot; sem ela o
                       relogio sobe o hotspot "Relogio"
  --wifi-psk SENHA     senha dessa rede
  --weather-key CHAVE  chave do weatherapi.com gravada no nixie.json
  --latitude N         latitude da previsao do tempo
  --longitude N        longitude da previsao do tempo
  --grow-mb N          folga a acrescentar no rootfs, em MB  (padrao: 1536)
  --with-devtools      ja instala o toolchain no cartao, para compilar no
                       proprio Pi (cresce ~1.2 GB). Sem ela, o leiame.txt
                       do cartao ensina a instalar depois
  --compress           gera tambem nixie-clock.img.xz
  --keep-image         reaproveita build/nixie-clock.img em vez de refazer
  -h, --help           esta ajuda

Nada aqui escreve em /dev/sdX. Para gravar o cartao depois:
  xzcat build/nixie-clock.img.xz | sudo dd of=/dev/sdX bs=4M conv=fsync status=progress
  (ou use o Raspberry Pi Imager com "Use custom image")
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
        *) echo "opcao desconhecida: $1" >&2; usage >&2; exit 1 ;;
    esac
done

say()  { printf '\n\033[1;36m>> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m!! %s\033[0m\n' "$*" >&2; }
die()  { printf '\033[1;31mERRO: %s\033[0m\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- pre-flight

[[ -n "$PASSWORD" ]] || die "--password e obrigatoria (o Bullseye nao cria mais um usuario padrao)."
[[ -n "$WIFI_SSID" && -z "$WIFI_PSK" ]] && die "--wifi-ssid pede --wifi-psk."

for t in curl xz sha256sum truncate sfdisk losetup mount umount rsync openssl docker; do
    command -v "$t" >/dev/null 2>&1 || die "falta a ferramenta '$t' no host."
done

[[ -e /proc/sys/fs/binfmt_misc/qemu-arm ]] || \
    die "binfmt qemu-arm nao registrado. Rode: sudo apt-get install -y qemu-user-binfmt (Ubuntu antigo: qemu-user-static binfmt-support)"

# O interpretador e o que o kernel registrou, nao um nome adivinhado: ja foi
# qemu-arm-static (pacote qemu-user-static) e hoje e qemu-arm, estatico, no
# qemu-user. Com a flag F o kernel ja o tem carregado; a copia para dentro do
# chroot so faz falta sem ela, e nao atrapalha com ela.
QEMU_ARM=$(sed -n 's/^interpreter //p' /proc/sys/fs/binfmt_misc/qemu-arm)
[[ -x "$QEMU_ARM" ]] || die "o binfmt aponta para '$QEMU_ARM', que nao existe no host."
[[ -n "$BUILD_IMAGE" ]] || die "nao consegui ler o nome da imagem de build em $BUILD_SH"

if ! docker info >/dev/null 2>&1; then
    sudo -n docker info >/dev/null 2>&1 || \
        die "sem acesso ao daemon do Docker. Veja as instrucoes em sources/clock/docker/build.sh"
fi

say "pedindo sudo adiantado (loop device, mount e chroot precisam de root)"
sudo -v || die "sudo negado."

mkdir -p "$CACHE_DIR" "$MNT_BOOT" "$MNT_ROOT"

# ------------------------------------------------------------------ limpeza
# Um loop device ou um bind mount esquecido para o proximo run do script antes
# de ele comecar, e deixa o cartao do usuario montado em cima de build/. O trap
# roda em qualquer saida, inclusive a bem sucedida.
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
    say "baixando $IMG_XZ (~366 MB)"
    curl -fL --progress-bar -o "$CACHE_DIR/$IMG_XZ.part" "$IMG_URL"
    mv "$CACHE_DIR/$IMG_XZ.part" "$CACHE_DIR/$IMG_XZ"
fi

say "conferindo sha256"
echo "$IMG_SHA256  $CACHE_DIR/$IMG_XZ" | sha256sum -c - || \
    die "sha256 nao confere. Apague $CACHE_DIR/$IMG_XZ e rode de novo."

# Uma copia local em sources/ (de um clone antigo, ou posta a mao por quem
# compila sem rede) serve de semente para o cache e evita o download.
if [[ ! -f "$LIGHTTPD_TAR" && -f "$PROJECT_DIR/sources/$LIGHTTPD_TGZ" ]]; then
    cp "$PROJECT_DIR/sources/$LIGHTTPD_TGZ" "$LIGHTTPD_TAR"
fi
if [[ ! -f "$LIGHTTPD_TAR" ]]; then
    say "baixando $LIGHTTPD_TGZ"
    curl -fL --progress-bar -o "$LIGHTTPD_TAR.part" "$LIGHTTPD_URL"
    mv "$LIGHTTPD_TAR.part" "$LIGHTTPD_TAR"
fi
echo "$LIGHTTPD_SHA256  $LIGHTTPD_TAR" | sha256sum -c - || \
    die "sha256 do lighttpd nao confere. Apague $LIGHTTPD_TAR e rode de novo."

# --------------------------------------------- 2. descompacta e cresce a imagem

if [[ $KEEP_IMAGE -eq 1 && -f "$OUT_IMG" ]]; then
    say "reaproveitando $OUT_IMG (--keep-image)"
else
    say "descompactando para $OUT_IMG"
    rm -f "$OUT_IMG"
    xz -dc "$CACHE_DIR/$IMG_XZ" > "$OUT_IMG"

    say "crescendo a imagem em ${GROW_MB} MB"
    truncate -s "+${GROW_MB}M" "$OUT_IMG"
    # A particao 2 e a ultima, entao da para empurrar o fim dela ate o fim do
    # arquivo sem tocar em mais nada. ",+" no sfdisk quer dizer "todo o resto".
    echo ", +" | sfdisk -N 2 --no-reread --force "$OUT_IMG" >/dev/null
fi

# -------------------------------------------- 3. compila o que vai no Pi

say "compilando o relogio no container ARM"
"$PROJECT_DIR/sources/clock/docker/build.sh"

DOCKERBUILD="$PROJECT_DIR/sources/clock/dockerbuild"
for f in nixie camera nixie.cgi liblogger.so; do
    [[ -f "$DOCKERBUILD/$f" ]] || die "o build nao produziu $f"
done

LIGHTTPD_STAGE="$BUILD_DIR/lighttpd-stage"
if [[ ! -x "$LIGHTTPD_STAGE/usr/local/sbin/lighttpd" ]]; then
    say "compilando o lighttpd 1.4.78 no container ARM (demora: tudo emulado)"
    rm -rf "$BUILD_DIR/$LIGHTTPD_DIR" "$LIGHTTPD_STAGE"
    tar -xzf "$LIGHTTPD_TAR" -C "$BUILD_DIR"
    # Mesma imagem e mesmo usuario do build do relogio, para os artefatos
    # sairem com o dono certo e linkados contra a mesma glibc. Isso roda
    # antes de a imagem ser montada: o bind de $PROJECT_DIR no container
    # nao pode pegar build/mnt com o cartao montado embaixo.
    # --without-pcre2 e o que esta no config.status do relogio que funciona.
    DOCKER=(docker); docker info >/dev/null 2>&1 || DOCKER=(sudo docker)
    "${DOCKER[@]}" run --rm \
        --platform linux/arm/v6 \
        --user "$(id -u):$(id -g)" \
        -v "$PROJECT_DIR":/src -w "/src/build/$LIGHTTPD_DIR" \
        -e HOME=/tmp \
        "$BUILD_IMAGE" \
        /bin/bash -euo pipefail -c "
            # O tarball oficial do 1.4.78 vem sem o ./configure: traz
            # configure.ac e autogen.sh, e o autoreconf e que gera o resto.
            ./autogen.sh
            ./configure --prefix=/usr/local --without-pcre2 >/dev/null
            make -j\$(nproc) >/dev/null
            make install DESTDIR=/src/build/lighttpd-stage >/dev/null
        "
else
    say "lighttpd ja compilado em build/lighttpd-stage"
fi
[[ -x "$LIGHTTPD_STAGE/usr/local/sbin/lighttpd" ]] || die "o lighttpd nao foi compilado"

# -------------------------------------------------- 4. monta a imagem

say "abrindo a imagem em um loop device"
LOOP=$(sudo losetup --find --show --partscan "$OUT_IMG")
[[ -b "${LOOP}p1" && -b "${LOOP}p2" ]] || die "o kernel nao expos as particoes de $LOOP"

sudo e2fsck -pf "${LOOP}p2" >/dev/null || true   # resize2fs exige fsck limpo
sudo resize2fs "${LOOP}p2" >/dev/null

sudo mount "${LOOP}p2" "$MNT_ROOT"
sudo mount "${LOOP}p1" "$MNT_BOOT"
df -h --output=target,size,avail "$MNT_ROOT" "$MNT_BOOT" | sed 's/^/   /'

# ------------------------------------------- 5. chroot emulado para o apt

say "preparando o chroot emulado"
# no mesmo caminho do host, que e onde o kernel vai procurar sem a flag F
QEMU_IN_IMAGE=0
if ! sudo test -e "$MNT_ROOT$QEMU_ARM"; then
    sudo install -D -m755 "$QEMU_ARM" "$MNT_ROOT$QEMU_ARM"
    QEMU_IN_IMAGE=1
fi
# O Raspbian pre-carrega libarmmem via /etc/ld.so.preload. Sob qemu isso faz
# todo binario do chroot morrer com "cannot be preloaded", e a mensagem nao
# diz nada sobre a causa. Sai do caminho e volta no fim.
if sudo test -f "$MNT_ROOT/etc/ld.so.preload"; then
    sudo mv "$MNT_ROOT/etc/ld.so.preload" "$MNT_ROOT/etc/ld.so.preload.disabled"
fi
# O chroot compartilha a rede do host, entao o resolv.conf do host serve --
# inclusive o stub 127.0.0.53 do systemd-resolved. O original volta no fim
# para a imagem nao sair com o DNS de quem a gerou.
sudo cp -a "$MNT_ROOT/etc/resolv.conf" "$MNT_ROOT/etc/resolv.conf.img" 2>/dev/null || true
sudo cp -L /etc/resolv.conf "$MNT_ROOT/etc/resolv.conf"
sudo mount -t proc  none  "$MNT_ROOT/proc"
sudo mount -t sysfs none  "$MNT_ROOT/sys"
sudo mount --bind /dev     "$MNT_ROOT/dev"
sudo mount --bind /dev/pts "$MNT_ROOT/dev/pts"
# O /boot do sistema alvo e a particao 1; o lighttpd nao liga, mas o
# rpi-eeprom e o dphys-swapfile dos postinst do apt ligam.
sudo mount --bind "$MNT_BOOT" "$MNT_ROOT/boot"

in_chroot() { sudo chroot "$MNT_ROOT" /bin/bash -euo pipefail -c "$1"; }

# Os postinst de dnsmasq e hostapd tentam subir o servico; dentro do chroot nao
# ha systemd rodando, e o invoke-rc.d falharia e abortaria o apt.
sudo tee "$MNT_ROOT/usr/sbin/policy-rc.d" >/dev/null <<'EOF'
#!/bin/sh
exit 101
EOF
sudo chmod +x "$MNT_ROOT/usr/sbin/policy-rc.d"

# Runtime, nao -dev: quem compila e o container, nao o Pi. Os pacotes -dev
# so entram com --with-devtools, para quem quiser compilar no proprio relogio.
# A lista sai do "readelf -d" dos tres binarios, nao de um chute: camera pede
# videoio/objdetect/imgproc/core, nixie pede pigpio e curl, nixie.cgi nada.
# imgcodecs entra porque videoio depende dele (e porque EXPORT_FACE_JPG, em
# defines.h, grava jpg quando ligado). highgui fica de fora de proposito:
# arrastaria GTK e X11 para dentro de uma imagem Lite.
PKGS_RUNTIME="libopencv-videoio4.5 libopencv-objdetect4.5 libopencv-imgproc4.5 \
libopencv-core4.5 libopencv-imgcodecs4.5 \
libcurl4 libpigpio1 pigpio pigpio-tools dnsmasq hostapd i2c-tools"
PKGS_DEV="build-essential cmake pkg-config git libopencv-dev libcurl4-openssl-dev libpigpio-dev"

say "instalando as dependencias dentro da imagem (emulado, pode demorar)"
# Acquire::Retries pelo mesmo motivo do Dockerfile: sao centenas de MB vindos
# do archive.raspbian.org, e uma conexao resetada no meio perde tudo.
in_chroot "export DEBIAN_FRONTEND=noninteractive
           apt-get update
           apt-get -o Acquire::Retries=5 install -y --no-install-recommends $PKGS_RUNTIME"

if [[ $WITH_DEVTOOLS -eq 1 ]]; then
    say "instalando tambem o toolchain (--with-devtools)"
    in_chroot "export DEBIAN_FRONTEND=noninteractive
               apt-get -o Acquire::Retries=5 install -y --no-install-recommends $PKGS_DEV"
fi

# ------------------------------------------------------ 6. copia o projeto

say "copiando o relogio para /home/pi"
HOME_PI="$MNT_ROOT/home/$USERNAME"
sudo mkdir -p "$HOME_PI/nixiepi" "$HOME_PI/www"

sudo install -m755 "$DOCKERBUILD/nixie"  "$HOME_PI/nixiepi/nixie"
sudo install -m755 "$DOCKERBUILD/camera" "$HOME_PI/nixiepi/camera"
sudo install -m755 "$PROJECT_DIR/nixiepi/services.sh"   "$HOME_PI/nixiepi/services.sh"
sudo install -m755 "$PROJECT_DIR/nixiepi/update_bins.sh" "$HOME_PI/nixiepi/update_bins.sh"

# As cascatas. Qual delas a camera usa se escolhe na aba Deteccao do site
# (detection.face_cascade no nixie.json); o padrao e o LBP melhorado.
for x in haarcascade_frontalface_default.xml lbpcascade_frontalface.xml \
         lbpcascade_frontalface_improved.xml; do
    sudo install -m644 "$PROJECT_DIR/nixiepi/$x" "$HOME_PI/nixiepi/$x"
done

say "copiando o site"
# -a preserva symlinks como symlinks, que e o que www/logs/nixie.txt precisa ser.
sudo rsync -a --delete \
    --exclude '.idea/' --exclude 'json/nixie.json' \
    "$PROJECT_DIR/www/" "$HOME_PI/www/"
sudo install -d -m755 "$HOME_PI/www/cgi-bin" "$HOME_PI/www/json" "$HOME_PI/www/logs"
# A aba "Logs" do site busca logs/nixie.txt como arquivo estatico; o log de
# verdade e /tmp/nixie.txt, escrito pela liblogger. O link e o que liga os dois.
sudo ln -sfn /tmp/nixie.txt "$HOME_PI/www/logs/nixie.txt"
sudo install -m755 "$DOCKERBUILD/nixie.cgi" "$HOME_PI/www/cgi-bin/nixie.cgi"

# O nixie.json que vai para o cartao sai SEMPRE do modelo publicavel, nunca do
# nixie.json local -- que e a configuracao da casa de quem esta compilando.
say "gravando a configuracao inicial"
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

# Os servidores NTP da pagina so valem pelo systemd-timesyncd: o relogio le a
# hora do sistema e nada mais. O nixie.cgi reescreve este arquivo quando alguem
# salva a aba NTP; aqui ele nasce com os mesmos servidores do nixie.json.
NTP_SERVERS=$(python3 -c '
import json, sys
n = json.load(open(sys.argv[1])).get("ntp", {})
print(" ".join(v for v in (n.get("server_%d" % i, "") for i in range(4)) if v))' "$TMP_JSON")
rm -f "$TMP_JSON"
sudo install -d -m755 "$MNT_ROOT/etc/systemd/timesyncd.conf.d"
printf '# Written by the clock'"'"'s web page (nixie.cgi). Edits here are overwritten.\n[Time]\nNTP=%s\n' \
    "$NTP_SERVERS" | sudo tee "$MNT_ROOT/etc/systemd/timesyncd.conf.d/nixie.conf" >/dev/null

# O relogio abre /home/pi/nixie.json em alguns lugares; no cartao que funciona
# isso e um link para o arquivo de verdade.
sudo ln -sfn www/json/nixie.json "$HOME_PI/nixie.json"

sudo install -m644 "$PROJECT_DIR/leiame.txt" "$HOME_PI/leiame.txt"

# O cartao tem de se bastar: quem so tem o relogio na mao precisa dos fontes
# para compilar e do projeto/ para consertar o hardware. Os fontes vao sempre;
# --with-devtools decide apenas se o toolchain ja vem instalado (o leiame.txt
# ensina a instalar depois). O update_bins.sh procura os binarios em
# sources/clock/dockerbuild, para onde aponta o rsync do fluxo de atualizacao.
say "copiando os fontes e o projeto para o cartao"
# Antes do rsync: ele so cria o ultimo diretorio do destino, e /home/pi/sources
# nao existe na imagem base. O install -d cria a arvore inteira.
sudo install -d -m755 "$HOME_PI/sources/clock/dockerbuild"
sudo rsync -a --exclude '.git/' --exclude '.idea/' --exclude '.vscode/' \
    --exclude 'build/' --exclude 'rpibuild/' --exclude 'dockerbuild/' \
    --exclude 'docs/' \
    "$PROJECT_DIR/sources/clock/" "$HOME_PI/sources/clock/"
# ~100 MB de esquemas, datasheets e fotos; cabe folgado no --grow-mb padrao.
sudo rsync -a "$PROJECT_DIR/projeto/" "$HOME_PI/projeto/"

say "instalando as bibliotecas e o lighttpd em /usr/local"
sudo install -m755 "$DOCKERBUILD/liblogger.so" "$MNT_ROOT/usr/local/lib/liblogger.so"
sudo cp -a "$LIGHTTPD_STAGE/usr/local/." "$MNT_ROOT/usr/local/"
# O "make install" do lighttpd rodou no container com o uid do host, e o cp -a
# preserva isso. No cartao que funciona /usr/local e todo root:root.
sudo chown -R 0:0 "$MNT_ROOT/usr/local"

say "instalando scripts e services"
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
# O "allow-hotplug wlan0" nao e padrao da imagem; esta assim no relogio que
# funciona, e reproduzir e mais barato do que descobrir na bancada que faz falta.
sudo install -m644 "$PROJECT_DIR/sources/configs/etc/network/interfaces" "$MNT_ROOT/etc/network/interfaces"

# ---------------------------------------------------- 7. habilita os servicos

say "habilitando os servicos"
# lighttpd-custom.service fica instalado e DESLIGADO de proposito: quem sobe o
# servidor e a ultima linha do check_wifi_or_hotspot.sh, depois de decidir
# entre wi-fi e hotspot. Ligar os dois poe duas instancias na porta 80.
# SYSTEMD_OFFLINE=1: o systemctl detecta chroot sozinho na maioria das versoes,
# mas quando nao detecta ele tenta falar com o bus e falha com uma mensagem que
# nao tem nada a ver ("Failed to connect to bus"). Aqui so queremos os symlinks.
in_chroot "SYSTEMD_OFFLINE=1 systemctl enable nixie.service camera.service wifi-check.service check_ssid.service" \
    || die "systemctl enable falhou; o cartao subiria sem o relogio."
in_chroot "SYSTEMD_OFFLINE=1 systemctl disable lighttpd-custom.service" 2>/dev/null || true
# hostapd e dnsmasq sobem sob demanda, dentro do check_wifi_or_hotspot.sh.
# Desabilitados, nao mascarados: mascarar impediria o proprio script de subi-los.
in_chroot "SYSTEMD_OFFLINE=1 systemctl disable hostapd dnsmasq" 2>/dev/null || true
in_chroot "SYSTEMD_OFFLINE=1 systemctl unmask hostapd dnsmasq" 2>/dev/null || true
in_chroot "ldconfig"

# A prova de que a lista de pacotes acima esta completa: se faltou uma lib, o
# relogio so descobriria no primeiro boot, num Pi, sem tela. Aqui descobre agora.
say "conferindo que tudo linka dentro da imagem"
missing=$(in_chroot '
    for b in /home/pi/nixiepi/nixie /home/pi/nixiepi/camera \
             /home/pi/www/cgi-bin/nixie.cgi /usr/local/sbin/lighttpd; do
        # o || true e obrigatorio: sem match, o grep sai 1, e com pipefail
        # isso abortaria o laco no primeiro binario que estivesse correto
        ldd "$b" 2>/dev/null | grep "not found" | sed "s|^|  $b: |" || true
    done') || true
if [[ -n "$missing" ]]; then
    echo "$missing" >&2
    die "ha bibliotecas faltando na imagem (veja acima)."
fi
echo "   nixie, camera, nixie.cgi e lighttpd: todas as libs resolvidas."

# ------------------------------------------------ 8. primeiro boot e hardware

say "configurando o primeiro boot"

# O Bullseye nao cria mais um usuario padrao: sem userconf.txt o sistema sobe e
# fica esperando alguem no console. Mas /home/pi esta hardcoded no codigo, e os
# arquivos ja foram copiados para la -- entao o uid/gid tem de bater com o que
# o firstboot vai criar, que e 1000:1000.
HASH=$(openssl passwd -6 "$PASSWORD")

# O /usr/lib/userconf-pi/userconf do Raspberry Pi OS RENOMEIA o usuario de uid
# 1000 que ja vem na imagem -- ele nao cria nenhum. Como o nome alvo e o mesmo
# que a imagem traz, sobra so o chpasswd. Se um dia a imagem base deixar de
# trazer esse usuario, o userconf.txt viraria no-op e o cartao subiria sem
# login nenhum: por isso a checagem abaixo, que cria o usuario na mao.
if ! in_chroot "getent passwd 1000 >/dev/null"; then
    warn "a imagem base nao traz usuario de uid 1000; criando '$USERNAME' aqui."
    in_chroot "groupadd -g 1000 '$USERNAME'
               useradd -u 1000 -g 1000 -M -d '/home/$USERNAME' -s /bin/bash '$USERNAME'
               usermod -aG adm,sudo,video,audio,plugdev,gpio,i2c,spi,netdev '$USERNAME' || true"
fi
printf '%s:%s\n' "$USERNAME" "$HASH" | sudo tee "$MNT_BOOT/userconf.txt" >/dev/null
sudo chmod 600 "$MNT_BOOT/userconf.txt"

# /home/pi ja existe na imagem base; o que importa e que tudo que copiamos para
# dentro dele pertenca ao uid que vai fazer login.
sudo chown -R 1000:1000 "$HOME_PI"

sudo touch "$MNT_BOOT/ssh"                       # ssh ligado desde o primeiro boot
echo "$HOSTNAME" | sudo tee "$MNT_ROOT/etc/hostname" >/dev/null
sudo sed -i "s/^127\.0\.1\.1.*/127.0.1.1\t$HOSTNAME/" "$MNT_ROOT/etc/hosts"
echo "$TIMEZONE" | sudo tee "$MNT_ROOT/etc/timezone" >/dev/null
sudo ln -sfn "/usr/share/zoneinfo/$TIMEZONE" "$MNT_ROOT/etc/localtime"

if [[ -n "$WIFI_SSID" ]]; then
    say "gravando a rede wi-fi inicial"
    # Mesmo formato que o nixie.cpp escreve quando alguem salva pela pagina web
    # (wifi.cpp), para o check_ssid.sh achar o ssid do mesmo jeito nos dois casos.
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
    warn "sem --wifi-ssid: o relogio vai subir o hotspot 'Relogio' e mostrar 192.168.4.1 nas valvulas."
fi

sudo sed -i "s/^country_code=.*/country_code=$COUNTRY/" "$MNT_ROOT/etc/hostapd/hostapd.conf"

say "ajustando config.txt e modulos"
# Idempotente: o bloco so entra uma vez, e so o que nao e padrao do Bullseye.
#   i2c_arm + i2c-dev + i2c-rtc : o DS3231, que segura a hora quando falta luz
#   gpu_mem/start_x            : como esta no relogio que funciona
#   enable_uart                : console serial, util quando nao ha rede
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

# Compilar OpenCV num Zero com 512 MB precisa de swap; o cartao que funciona
# esta em 512 MB. Sem --with-devtools nada aqui compila, mas o custo e zero.
sudo sed -i 's/^CONF_SWAPSIZE=.*/CONF_SWAPSIZE=512/' "$MNT_ROOT/etc/dphys-swapfile"

# ---------------------------------------------------------------- 9. fecha

say "desmontando"
sudo rm -f "$MNT_ROOT/usr/sbin/policy-rc.d"
[[ $QEMU_IN_IMAGE -eq 1 ]] && sudo rm -f "$MNT_ROOT$QEMU_ARM"
if sudo test -e "$MNT_ROOT/etc/resolv.conf.img"; then
    sudo mv "$MNT_ROOT/etc/resolv.conf.img" "$MNT_ROOT/etc/resolv.conf"
else
    sudo truncate -s 0 "$MNT_ROOT/etc/resolv.conf"   # o dhcpcd reescreve no boot
fi
if sudo test -f "$MNT_ROOT/etc/ld.so.preload.disabled"; then
    sudo mv "$MNT_ROOT/etc/ld.so.preload.disabled" "$MNT_ROOT/etc/ld.so.preload"
fi

sudo umount "$MNT_ROOT/dev/pts" "$MNT_ROOT/dev" "$MNT_ROOT/proc" "$MNT_ROOT/sys" "$MNT_ROOT/boot"
sudo umount "$MNT_BOOT" "$MNT_ROOT"
sudo losetup -d "$LOOP"; LOOP=""

if [[ $COMPRESS -eq 1 ]]; then
    say "compactando (demora)"
    xz -T0 -9 -kf "$OUT_IMG"
fi

say "pronto"
cat <<MSG

   imagem : $OUT_IMG  ($(du -h "$OUT_IMG" | cut -f1))
$( [[ $COMPRESS -eq 1 ]] && echo "   .xz    : $OUT_IMG.xz  ($(du -h "$OUT_IMG.xz" | cut -f1))" )
   usuario: $USERNAME   hostname: $HOSTNAME   fuso: $TIMEZONE
   wi-fi  : ${WIFI_SSID:-<nenhuma: sobe o hotspot 'Relogio'>}
   previsao do tempo: $( [[ -n "$WEATHER_KEY" ]] && echo "chave gravada" || echo "sem chave -- cadastre em Localizacao, no site do relogio" )

   Gravar o cartao:
     sudo dd if=$OUT_IMG of=/dev/sdX bs=4M conv=fsync status=progress
     ...ou Raspberry Pi Imager -> "Use custom image"

   ATENCAO: confira o /dev/sdX com 'lsblk' antes. Este script nunca grava em
   dispositivo nenhum justamente para que essa escolha seja sempre sua.
MSG
