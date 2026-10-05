#!/usr/bin/env bash
#
# Compila o relogio para o Raspberry Pi Zero (ARMv6) dentro de um container
# Raspberry Pi OS Bullseye armhf, emulado via binfmt_misc + qemu-arm.
#
# Uso:
#   ./docker/build.sh                  # compila todos os targets
#   ./docker/build.sh nixie camera     # compila targets especificos
#   ./docker/build.sh -j4              # limita o paralelismo (padrao: nproc)
#   ./docker/build.sh --shell          # abre um shell no container
#   ./docker/build.sh --clean          # descarta o diretorio de build
#   ./docker/build.sh --rebuild-image  # reconstroi a imagem do zero
#
# Os artefatos saem em dockerbuild/ -- separado de build/ e rpibuild/, que
# contem caches do CMake gerados no proprio Pi e nao sao reutilizaveis aqui.

set -euo pipefail

IMAGE=nixie-rpi-build:bullseye-3   # bump a tag ao mexer no Dockerfile, senao a imagem velha fica
PLATFORM=linux/arm/v6
BUILD_DIR=dockerbuild
# Compilacoes em paralelo. O qemu emula processo a processo, entao N compiladores
# simultaneos ocupam N nucleos reais do host -- nada a ver com o nucleo unico do
# Zero, que so limita quem compila no proprio Pi.
JOBS=${JOBS:-$(nproc)}

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(dirname -- "$SCRIPT_DIR")

# ---------------------------------------------------------------- pre-flight

DOCKER=(docker)
if ! docker info >/dev/null 2>&1; then
    if sudo -n docker info >/dev/null 2>&1; then
        DOCKER=(sudo docker)
    else
        cat >&2 <<'MSG'
ERRO: sem acesso ao daemon do Docker.

Rode uma vez (pede senha de sudo):

    sudo addgroup --system docker
    sudo adduser "$USER" docker
    sudo snap disable docker && sudo snap enable docker

Depois abra um novo terminal (ou rode 'newgrp docker') e tente de novo.
MSG
        exit 1
    fi
fi

# O Docker instalado via snap e confinado pela interface 'home': ele so enxerga
# caminhos sob $HOME que nao sejam ocultos. /tmp e ~/.cache, por exemplo, sao
# invisiveis -- e a falha aparece como "no such file or directory" no contexto
# de build, o que nao da nenhuma pista do motivo real.
if "${DOCKER[@]}" info --format '{{.DockerRootDir}}' 2>/dev/null | grep -q '^/var/snap/docker'; then
    case "$PROJECT_DIR/" in
        "$HOME"/*) [[ "${PROJECT_DIR#"$HOME"/}" == .* ]] && {
            echo "ERRO: o Docker do snap nao acessa diretorios ocultos sob \$HOME." >&2
            echo "      Mova o projeto para um caminho visivel: $PROJECT_DIR" >&2
            exit 1
        } ;;
        *)  echo "ERRO: o Docker do snap so acessa caminhos sob \$HOME ($HOME)." >&2
            echo "      O projeto esta em $PROJECT_DIR e nao sera visivel." >&2
            exit 1 ;;
    esac
fi

if [[ ! -e /proc/sys/fs/binfmt_misc/qemu-arm ]]; then
    echo "ERRO: binfmt qemu-arm nao registrado. Instale qemu-user-binfmt (Ubuntu antigo: qemu-user-static e binfmt-support)." >&2
    exit 1
fi

# ------------------------------------------------------------------ opcoes

REBUILD_IMAGE=0
SHELL_MODE=0
TARGETS=()

for arg in "$@"; do
    case "$arg" in
        --clean)
            echo ">> removendo $PROJECT_DIR/$BUILD_DIR"
            rm -rf -- "${PROJECT_DIR:?}/$BUILD_DIR"
            exit 0
            ;;
        -j|--jobs)       JOBS_NEXT=1 ;;
        -j*)             JOBS="${arg#-j}" ;;
        --rebuild-image) REBUILD_IMAGE=1 ;;
        --shell)         SHELL_MODE=1 ;;
        -h|--help)       sed -n '3,16p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *)  if [[ ${JOBS_NEXT:-0} -eq 1 ]]; then JOBS="$arg"; JOBS_NEXT=0
            else TARGETS+=("$arg"); fi ;;
    esac
done

# ------------------------------------------------------------------ imagem

if [[ $REBUILD_IMAGE -eq 1 ]] || ! "${DOCKER[@]}" image inspect "$IMAGE" >/dev/null 2>&1; then
    echo ">> construindo a imagem $IMAGE (demora: tudo roda emulado)"
    build_args=(build --platform "$PLATFORM" -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR")
    [[ $REBUILD_IMAGE -eq 1 ]] && build_args+=(--no-cache)
    "${DOCKER[@]}" "${build_args[@]}"
fi

# -------------------------------------------------------------------- run

run_in_container() {
    # -it so quando ha terminal: permite rodar em CI / pipe sem quebrar.
    local tty_args=()
    [[ -t 0 && -t 1 ]] && tty_args=(-it)
    "${DOCKER[@]}" run --rm "${tty_args[@]}" \
        --platform "$PLATFORM" \
        --user "$(id -u):$(id -g)" \
        -v "$PROJECT_DIR":/src \
        -w /src \
        -e HOME=/tmp \
        "$IMAGE" "$@"
}

if [[ $SHELL_MODE -eq 1 ]]; then
    exec_cmd=(/bin/bash)
    run_in_container "${exec_cmd[@]}"
    exit 0
fi

# CMAKE_SKIP_BUILD_RPATH: sem isso o CMake grava RUNPATH=/src/dockerbuild nos
# binarios -- um caminho que so existe dentro do container. No Pi ele e lixo
# morto, e a resolucao de liblogger.so passa a depender do fallback para
# /usr/local/lib. Sem o RUNPATH a resolucao fica explicita (ver update_bins.sh,
# que instala a lib em /usr/local/lib e roda ldconfig).
MAKE_TARGETS="${TARGETS[*]:-}"

run_in_container /bin/bash -euo pipefail -c "
    cmake -S /src -B /src/$BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_BUILD_RPATH=ON
    make -C /src/$BUILD_DIR -j$JOBS ${MAKE_TARGETS}
"

# ---------------------------------------------------------------- resultado

echo
echo ">> artefatos em $PROJECT_DIR/$BUILD_DIR:"
for f in nixie camera nixie.cgi liblogger.so; do
    p="$PROJECT_DIR/$BUILD_DIR/$f"
    [[ -f "$p" ]] && printf '   %-14s %s\n' "$f" "$(file -b "$p" | cut -d, -f1-2)"
done
