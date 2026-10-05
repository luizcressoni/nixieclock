#!/usr/bin/env bash
#
# Builds the clock for the Raspberry Pi Zero (ARMv6) inside a Raspberry Pi OS
# Bullseye armhf container, emulated through binfmt_misc + qemu-arm.
#
# Usage:
#   ./docker/build.sh                  # build all targets
#   ./docker/build.sh nixie camera     # build the given targets
#   ./docker/build.sh -j4              # limit parallel jobs (default: nproc)
#   ./docker/build.sh --shell          # open a shell in the container
#   ./docker/build.sh --clean          # discard the build directory
#   ./docker/build.sh --rebuild-image  # rebuild the image from scratch
#
# Output goes to dockerbuild/, apart from build/ and rpibuild/: those hold
# CMake caches made on the Pi itself and cannot be reused here.

set -euo pipefail

IMAGE=nixie-rpi-build:bullseye-3   # bump the tag when the Dockerfile changes, or the old image stays
PLATFORM=linux/arm/v6
BUILD_DIR=dockerbuild
# qemu emulates per process: N compilers use N real host cores. The Zero's
# single core only matters when building on the Pi itself.
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
ERROR: no access to the Docker daemon.

Run once (asks for the sudo password):

    sudo addgroup --system docker
    sudo adduser "$USER" docker
    sudo snap disable docker && sudo snap enable docker

Then open a new terminal (or run 'newgrp docker') and try again.
MSG
        exit 1
    fi
fi

# Snap Docker is confined to non-hidden paths under $HOME. Anything else (/tmp,
# ~/.cache) fails as a clueless "no such file or directory" in the build context.
if "${DOCKER[@]}" info --format '{{.DockerRootDir}}' 2>/dev/null | grep -q '^/var/snap/docker'; then
    case "$PROJECT_DIR/" in
        "$HOME"/*) [[ "${PROJECT_DIR#"$HOME"/}" == .* ]] && {
            echo "ERROR: snap Docker cannot reach hidden directories under \$HOME." >&2
            echo "       Move the project to a visible path: $PROJECT_DIR" >&2
            exit 1
        } ;;
        *)  echo "ERROR: snap Docker only reaches paths under \$HOME ($HOME)." >&2
            echo "       The project is at $PROJECT_DIR and will not be visible." >&2
            exit 1 ;;
    esac
fi

if [[ ! -e /proc/sys/fs/binfmt_misc/qemu-arm ]]; then
    echo "ERROR: binfmt qemu-arm not registered. Install qemu-user-binfmt (older Ubuntu: qemu-user-static and binfmt-support)." >&2
    exit 1
fi

# ----------------------------------------------------------------- options

REBUILD_IMAGE=0
SHELL_MODE=0
TARGETS=()

for arg in "$@"; do
    case "$arg" in
        --clean)
            echo ">> removing $PROJECT_DIR/$BUILD_DIR"
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

# ------------------------------------------------------------------- image

if [[ $REBUILD_IMAGE -eq 1 ]] || ! "${DOCKER[@]}" image inspect "$IMAGE" >/dev/null 2>&1; then
    echo ">> building image $IMAGE (slow: everything is emulated)"
    build_args=(build --platform "$PLATFORM" -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR")
    [[ $REBUILD_IMAGE -eq 1 ]] && build_args+=(--no-cache)
    "${DOCKER[@]}" "${build_args[@]}"
fi

# -------------------------------------------------------------------- run

run_in_container() {
    # -it only with a terminal, so CI and pipes still work.
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

# CMAKE_SKIP_BUILD_RPATH: otherwise CMake bakes RUNPATH=/src/dockerbuild, a path
# that only exists in the container. liblogger.so is found via ldconfig instead
# (see update_bins.sh).
MAKE_TARGETS="${TARGETS[*]:-}"

run_in_container /bin/bash -euo pipefail -c "
    cmake -S /src -B /src/$BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_BUILD_RPATH=ON
    make -C /src/$BUILD_DIR -j$JOBS ${MAKE_TARGETS}
"

# ------------------------------------------------------------------ result

echo
echo ">> artifacts in $PROJECT_DIR/$BUILD_DIR:"
for f in nixie camera nixie.cgi liblogger.so; do
    p="$PROJECT_DIR/$BUILD_DIR/$f"
    [[ -f "$p" ]] && printf '   %-14s %s\n' "$f" "$(file -b "$p" | cut -d, -f1-2)"
done
