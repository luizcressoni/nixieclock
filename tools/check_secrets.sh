#!/usr/bin/env bash
#
# Looks for credentials in what is about to be committed.
#
# The clock keeps three things that must not leave the house: the Wi-Fi
# password, its home coordinates and the weather API key. All three live in
# www/json/nixie.json, which git ignores.
#
# Usage:
#   ./tools/check_secrets.sh            # scan what git tracks
#   ./tools/check_secrets.sh --staged   # scan only the index
#
# As a pre-commit hook:
#   ln -s ../../tools/check_secrets.sh .git/hooks/pre-commit

set -uo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

STAGED=0
[[ "${1:-}" == "--staged" ]] && STAGED=1

# Patterns only, never values:
#   1. weatherapi.com key -- 30+ loose hex digits (ours had 31; do not assume 32)
#   2. psk=/password= with something inside
#   3. a nixie.json wifi block with the ssid filled in
PATTERNS=(
    '[0-9a-f]{30,}'
    '"?(psk|password|apikey|api_key)"?[[:space:]]*[=:][[:space:]]*"[^"]+"'
    '"?ssid"?[[:space:]]*[=:][[:space:]]*"[^"]+"'
)
DESCRIPTIONS=(
    'hardcoded API key (30+ hex)'
    'password/key with a value'
    'SSID with a value'
)

# Third party and build output: spdlog carries harmless hashes and tokens,
# and binaries are just noise.
EXCLUDE_RE='^(sources/opencv/|lighttpd-1\.4\.78/|build/|sources/clock/src/logger/spdlog/|sources/clock/(build|rpibuild|dockerbuild|docs)/|projeto/|sources/configs/etc/(dnsmasq|dhcpcd)\.conf$|tools/check_secrets\.sh$)'

if git rev-parse --git-dir >/dev/null 2>&1; then
    if [[ $STAGED -eq 1 ]]; then
        mapfile -t FILES < <(git diff --cached --name-only --diff-filter=ACM)
    else
        mapfile -t FILES < <(git ls-files)
    fi
else
    # No git yet: scan the working tree, applying .gitignore by hand. This is the
    # check that matters most -- before the first commit.
    [[ $STAGED -eq 1 ]] && { echo "check_secrets: --staged needs a git repository." >&2; exit 2; }
    echo "check_secrets: no git repository; scanning the working tree."
    mapfile -t FILES < <(find . \
        \( -name .git -o -path ./sources/opencv -o -path ./lighttpd-1.4.78 \
           -o -path ./projeto -o -path ./build \
           -o -path ./sources/clock/src/logger/spdlog \
           -o -path './sources/clock/*build' -o -path ./sources/clock/docs \) -prune -o \
        -type f -printf '%P\n' | sort)
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    # A delete-only commit: --diff-filter=ACM skips removals, which leak nothing.
    if [[ $STAGED -eq 1 ]]; then
        echo "check_secrets: nothing added or modified in this commit; ok."
    else
        echo "check_secrets: no files to scan." >&2
    fi
    exit 0
fi

found=0
# Without git nobody applies .gitignore: its literal entries are excluded here,
# or the clock's own (deliberately ignored) config shows up on every scan.
IGNORED=()
if ! git rev-parse --git-dir >/dev/null 2>&1 && [[ -f .gitignore ]]; then
    while IFS= read -r line; do
        line=${line%%#*}; line=${line%"${line##*[![:space:]]}"}
        [[ -z "$line" || "$line" == *'*'* ]] && continue
        IGNORED+=("${line#/}")
    done < .gitignore
fi
is_ignored() {
    local f=$1 pat
    for pat in "${IGNORED[@]:-}"; do
        [[ -z "$pat" ]] && continue
        [[ "$f" == "${pat%/}" || "$f" == "${pat%/}"/* ]] && return 0
    done
    return 1
}

for f in "${FILES[@]}"; do
    [[ -f "$f" ]] || continue
    [[ "$f" =~ $EXCLUDE_RE ]] && continue
    is_ignored "$f" && continue
    # -I skips binaries (jpg, mp4, .so, compressed cascades)
    for i in "${!PATTERNS[@]}"; do
        if hits=$(grep -InEI "${PATTERNS[$i]}" -- "$f" 2>/dev/null); then
            while IFS= read -r line; do
                body=${line#*:}
                # Shell variables, placeholders and regex pieces are templates,
                # not secrets: wpa_supplicant.conf heredocs, check_ssid.sh's grep.
                case "$body" in
                    *'$'*|*'<YOUR_'*|*'\K'*|*'${'*) continue ;;
                esac
                # Line marked as reviewed by hand. Use sparingly.
                case "$body" in *'check-secrets: ok'*) continue ;; esac
                # Public sha256 of the Raspberry Pi OS image: proof the download
                # was not tampered with.
                case "$body" in *[Ss][Hh][Aa]256*) continue ;; esac
                # file and line only, never the matched value
                echo "SECRET? ${f}:${line%%:*}  -- ${DESCRIPTIONS[$i]}"
                found=1
            done <<<"$hits"
        fi
    done
done

# nixie.json must never be tracked, even when clean at the moment.
if printf '%s\n' "${FILES[@]}" | grep -qx 'www/json/nixie.json'; then
    if git rev-parse --git-dir >/dev/null 2>&1; then
        echo "SECRET? www/json/nixie.json is tracked -- it is the clock's real config."
        echo "        Use: git rm --cached www/json/nixie.json"
        found=1
    elif ! grep -qx 'www/json/nixie.json' .gitignore 2>/dev/null; then
        echo "SECRET? www/json/nixie.json exists and is not in .gitignore."
        found=1
    fi
fi

if [[ $found -eq 1 ]]; then
    cat >&2 <<'MSG'

--------------------------------------------------------------------------
Possible credentials above. Check every line before committing.
Where each thing belongs:
  Wi-Fi password / coordinates / weather key -> www/json/nixie.json
                                                (ignored by git)
  publishable template, no values            -> www/json/nixie.default.json
--------------------------------------------------------------------------
MSG
    exit 1
fi

echo "check_secrets: nothing found in ${#FILES[@]} files."
