# Nixie Clock — código

Os fontes em C++ do relógio. **A documentação do projeto inteiro — hardware,
geração do cartão SD, configuração, publicação — está no
[README da raiz](../../README.md).**

## Targets

| Target | O que é |
|---|---|
| `nixie` | O processo principal: hardware, FSM, hora, previsão do tempo |
| `camera` | Detecção de rosto e movimento; avisa o `nixie` por signal |
| `nixie.cgi` | Backend do formulário da página web |
| `logger` | `liblogger.so`, o log compartilhado pelos três |

## Compilando

O Pi Zero é ARMv6, e um toolchain armhf de Debian/Ubuntu produz binários
ARMv7 que não rodam nele. Por isso o build acontece dentro de um container
Raspberry Pi OS Bullseye armhf, emulado via `qemu-arm`:

    ./docker/build.sh                  # todos os targets
    ./docker/build.sh nixie camera     # só alguns
    ./docker/build.sh --shell          # um shell no container
    ./docker/build.sh --clean          # descarta o diretório de build

Os artefatos saem em `dockerbuild/`. As pastas `build/` e `rpibuild/` guardam
caches de CMake gerados no próprio Pi e não são reaproveitáveis no PC.

Pré-requisitos do host e o fluxo de instalação no relógio estão no
[README da raiz](../../README.md#desenvolvimento).

## Estrutura

    src/nixie/        processo principal
      hardware/       válvulas, PWM, bargraph, LEDs, dimmer, GPIO
      modulation/     senoidal, flash, rampa
    src/camera/       captura, detecção de rosto, detecção de movimento
    src/cgi-bin/      o CGI da página web
    src/logger/       liblogger.so (spdlog embutido)
    src/utils/        cJSON, parser de config, signals, timers, mediana

## Documentação do código

Doxygen, a partir do `Doxyfile`:

    doxygen Doxyfile      # sai em docs/html/, abra o index.html

## Logs

O relógio escreve em `/tmp/nixie.txt`, também visível na aba *Logs* do site.

## Autor

[Luiz Cressoni](mailto:luiz@cressoni.com.br)
