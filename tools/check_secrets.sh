#!/usr/bin/env bash
#
# Procura credenciais no que esta prestes a ser commitado.
#
# O relogio guarda tres coisas que nao podem sair de casa: a senha do wi-fi, as
# coordenadas de onde ele mora e a chave da API de previsao do tempo. As duas
# primeiras vivem em www/json/nixie.json (ignorado pelo git); a terceira ja foi
# um argumento default em weather.h e hoje vem do mesmo arquivo.
#
# Uso:
#   ./tools/check_secrets.sh            # varre o que o git versiona
#   ./tools/check_secrets.sh --staged   # varre so o que esta no index
#
# Como hook de pre-commit:
#   ln -s ../../tools/check_secrets.sh .git/hooks/pre-commit

set -uo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

STAGED=0
[[ "${1:-}" == "--staged" ]] && STAGED=1

# Nao use este script como fonte de exemplos: ele cita os padroes, nao os valores.
#   1. chave do weatherapi.com -- um bolo de 30+ digitos hexadecimais soltos
#      no codigo (a do projeto tinha 31; nao assuma 32)
#   2. psk=/password= com algo dentro
#   3. um bloco wifi de nixie.json com ssid preenchido
PATTERNS=(
    '[0-9a-f]{30,}'
    '"?(psk|password|senha|apikey|api_key)"?[[:space:]]*[=:][[:space:]]*"[^"]+"'
    '"?ssid"?[[:space:]]*[=:][[:space:]]*"[^"]+"'
)
DESCRIPTIONS=(
    'chave de API (30+ hex) hardcoded'
    'senha/chave com valor preenchido'
    'SSID com valor preenchido'
)

# Terceiros e artefatos: o spdlog embutido tem hashes e tokens que nao sao
# segredo nenhum, e varrer binarios so gera ruido.
EXCLUDE_RE='^(sources/opencv/|lighttpd-1\.4\.78/|build/|sources/clock/src/logger/spdlog/|sources/clock/(build|rpibuild|dockerbuild|docs)/|projeto/|sources/configs/etc/(dnsmasq|dhcpcd)\.conf$|tools/check_secrets\.sh$)'

if git rev-parse --git-dir >/dev/null 2>&1; then
    if [[ $STAGED -eq 1 ]]; then
        mapfile -t FILES < <(git diff --cached --name-only --diff-filter=ACM)
    else
        mapfile -t FILES < <(git ls-files)
    fi
else
    # Ainda nao ha git aqui. Varre o working tree aplicando o .gitignore na mao,
    # para que de para conferir o repositorio ANTES de criar o primeiro commit --
    # que e exatamente quando isso importa.
    [[ $STAGED -eq 1 ]] && { echo "check_secrets: --staged precisa de um repositorio git." >&2; exit 2; }
    echo "check_secrets: sem repositorio git; varrendo o working tree."
    mapfile -t FILES < <(find . \
        \( -name .git -o -path ./sources/opencv -o -path ./lighttpd-1.4.78 \
           -o -path ./projeto -o -path ./build \
           -o -path ./sources/clock/src/logger/spdlog \
           -o -path './sources/clock/*build' -o -path ./sources/clock/docs \) -prune -o \
        -type f -printf '%P\n' | sort)
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    # Acontece de verdade num commit que so apaga arquivos: o --diff-filter=ACM
    # acima nao lista remocoes, e remocao nao tem conteudo para vazar nada.
    if [[ $STAGED -eq 1 ]]; then
        echo "check_secrets: nada adicionado ou modificado neste commit; ok."
    else
        echo "check_secrets: nenhum arquivo para varrer." >&2
    fi
    exit 0
fi

found=0
# Sem git, o .gitignore nao e aplicado por ninguem: as entradas literais dele
# entram na exclusao na mao, senao a propria config do relogio -- que esta
# ignorada de proposito -- aparece como achado em toda varredura.
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
    # -I pula binarios (jpg, mp4, .so, cascatas comprimidas)
    for i in "${!PATTERNS[@]}"; do
        if hits=$(grep -InEI "${PATTERNS[$i]}" -- "$f" 2>/dev/null); then
            while IFS= read -r line; do
                body=${line#*:}
                # Um valor que e uma variavel de shell, um placeholder ou um
                # pedaco de regex e um molde, nao um segredo: e assim que ficam
                # os templates de wpa_supplicant.conf e o grep do check_ssid.sh.
                case "$body" in
                    *'$'*|*'<SUA_'*|*'\K'*|*'${'*) continue ;;
                esac
                # Linha marcada a mao como conferida. Use com parcimonia.
                case "$body" in *'check-secrets: ok'*) continue ;; esac
                # O sha256 da imagem do Raspberry Pi OS e publico e tem de estar
                # no script -- e o que prova que o download nao foi adulterado.
                case "$body" in *[Ss][Hh][Aa]256*) continue ;; esac
                # mostra o arquivo e a linha, nunca o valor casado
                echo "SEGREDO? ${f}:${line%%:*}  -- ${DESCRIPTIONS[$i]}"
                found=1
            done <<<"$hits"
        fi
    done
done

# nixie.json nunca deve ser versionado, mesmo que naquele instante esteja limpo.
if printf '%s\n' "${FILES[@]}" | grep -qx 'www/json/nixie.json'; then
    if git rev-parse --git-dir >/dev/null 2>&1; then
        echo "SEGREDO? www/json/nixie.json esta versionado -- e a config real do relogio."
        echo "         Use: git rm --cached www/json/nixie.json"
        found=1
    elif ! grep -qx 'www/json/nixie.json' .gitignore 2>/dev/null; then
        echo "SEGREDO? www/json/nixie.json existe e nao esta no .gitignore."
        found=1
    fi
fi

if [[ $found -eq 1 ]]; then
    cat >&2 <<'MSG'

--------------------------------------------------------------------------
Possiveis credenciais acima. Confira cada linha antes de commitar.
Onde cada coisa deve morar:
  senha do wi-fi / coordenadas / chave da previsao -> www/json/nixie.json
                                                      (ignorado pelo git)
  modelo publicavel, sem valores                   -> www/json/nixie.default.json
--------------------------------------------------------------------------
MSG
    exit 1
fi

echo "check_secrets: nada encontrado em ${#FILES[@]} arquivos."
