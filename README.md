# Nixie Clock

Um relógio de válvulas nixie rodando num Raspberry Pi Zero: mostra data e hora
nos tubos, temperatura ou dia da semana no bargraph, acende quando alguém chega
na frente dele e se configura por uma página web que ele mesmo serve.

![o relógio](www/jpgs/clock.jpg)

- Sincroniza a hora por NTP e busca a previsão do tempo pela internet
- Abre o próprio hotspot quando não acha a rede de casa, e mostra o IP nos tubos
- LEDs RGB sob as válvulas indicam o estado do relógio e a cobertura de nuvens
- Detecção de rosto e de movimento para acordar o display e trocar o que ele mostra
- Regeneração de catodo (anti-envenenamento) automática, de madrugada

Autor: [Luiz Cressoni](mailto:luiz@cressoni.com.br)

---

## Sumário

- [O que você precisa](#o-que-você-precisa)
- [Gerando o cartão SD](#gerando-o-cartão-sd)
- [Primeiro boot](#primeiro-boot)
- [Configurando pela página web](#configurando-pela-página-web)
- [Como o relógio é montado por dentro](#como-o-relógio-é-montado-por-dentro)
- [Desenvolvimento](#desenvolvimento)
- [Segredos: o que nunca pode entrar no git](#segredos-o-que-nunca-pode-entrar-no-git)
- [Diagnóstico](#diagnóstico)
- [Onde cada coisa vai parar no cartão](#onde-cada-coisa-vai-parar-no-cartão)

---

## O que você precisa

**Hardware**

| | |
|---|---|
| Placa | Raspberry Pi Zero ou Zero W (ARMv6) |
| Cartão | 8 GB ou mais |
| Câmera | webcam USB (UVC) — o `camera` abre `/dev/video0` via V4L2 |
| RTC | DS3231 no barramento I²C, para segurar a hora quando falta luz |
| Display | 6 válvulas nixie + bargraph IN-9 + LEDs RGB |

Os esquemas, datasheets e fotos da montagem estão em [`projeto/`](projeto/), e
vão junto para o cartão, em `/home/pi/projeto`.

**No PC que vai gerar o cartão** (testado em Ubuntu):

```bash
sudo apt-get install -y docker.io qemu-user-binfmt \
                        xz-utils curl rsync fdisk e2fsprogs openssl python3
# Ubuntu até o 24.04: qemu-user-static binfmt-support no lugar de qemu-user-binfmt

# acesso ao docker sem sudo
sudo addgroup --system docker
sudo adduser "$USER" docker
# só para instalação via snap:
sudo snap disable docker && sudo snap enable docker
```

Depois abra um terminal novo (ou `newgrp docker`) para o grupo valer.

Para conferir o QEMU: `cat /proc/sys/fs/binfmt_misc/qemu-arm` tem de existir e
mostrar um `F` na linha `flags:` (é ele que faz o emulador funcionar dentro do
container), e `docker run --rm balenalib/rpi-raspbian:bullseye uname -m` tem de
responder `armv6l`.

> **Por que QEMU?** O Pi Zero é ARMv6. Um toolchain armhf de Debian/Ubuntu
> produz binários ARMv7, que simplesmente não rodam nele. Por isso tudo é
> compilado dentro de um container Raspberry Pi OS armhf de verdade, emulado
> por `binfmt_misc` + `qemu-arm`. É mais lento e é o que funciona.

---

## Gerando o cartão SD

```bash
git clone <url-deste-repositorio> nixie
cd nixie

./tools/make_image.sh --password 'uma-senha-boa'
```

Isso, sozinho, produz `build/nixie-clock.img`. O script:

1. baixa o Raspberry Pi OS **Bullseye armhf lite** (2024-10-22) e o tarball do
   **lighttpd 1.4.78**, conferindo o SHA-256 dos dois;
2. descompacta e aumenta o rootfs — a imagem original não tem folga;
3. compila `nixie`, `camera`, `nixie.cgi` e `liblogger.so` no container ARM;
4. compila o lighttpd no mesmo container, com `--without-pcre2`;
5. instala as dependências *dentro* da imagem, por chroot emulado;
6. copia binários, páginas, scripts, units e configs para os lugares certos;
7. prepara o primeiro boot: usuário, SSH, Wi-Fi, `config.txt`, fuso, hostname.

A primeira execução demora bastante — compilar OpenCV-linkado e lighttpd sob
emulação não é rápido. As seguintes reaproveitam o download e o lighttpd já
compilado.

### Opções úteis

```bash
./tools/make_image.sh \
  --password 'uma-senha-boa' \
  --hostname nixie \
  --timezone America/Sao_Paulo \
  --wifi-ssid 'MinhaRede' --wifi-psk 'senha-do-wifi' \
  --weather-key 'sua-chave-do-weatherapi' \
  --latitude -23.5505 --longitude -46.6333 \
  --compress
```

`./tools/make_image.sh --help` lista todas. As mais importantes:

| Opção | Para quê |
|---|---|
| `--password` | **obrigatória** — o Bullseye não cria mais um usuário padrão |
| `--wifi-ssid` / `--wifi-psk` | já deixa a rede gravada; sem elas o relógio sobe o hotspot |
| `--weather-key` | chave do [weatherapi.com](https://www.weatherapi.com/) (grátis); sem ela a previsão fica desligada |
| `--with-devtools` | já deixa o toolchain instalado no cartão, para compilar no próprio Pi (+~1,2 GB). Sem ela dá para instalar depois — o `leiame.txt` do cartão diz como |
| `--compress` | gera também o `.img.xz` |
| `--keep-image` | reaproveita a imagem já montada, em vez de refazer do zero |

O usuário é sempre `pi`: `/home/pi` está embutido no código (`json_parser.cpp`,
`lighttpd.conf`, os units do systemd) e mudar isso exigiria recompilar.

### Gravando

O script **nunca escreve em dispositivo nenhum** — essa escolha é sempre sua.
Confira o alvo com `lsblk` antes, e então:

```bash
lsblk                      # ache o seu cartão. NÃO chute.
sudo dd if=build/nixie-clock.img of=/dev/sdX bs=4M conv=fsync status=progress
```

ou abra `build/nixie-clock.img` no Raspberry Pi Imager em *Use custom image*.

---

## Primeiro boot

Com `--wifi-ssid`, o relógio conecta na rede e segue a vida. Sem ela — ou se a
rede não estiver no ar — ele:

1. sobe um hotspot aberto chamado **`Relogio`**;
2. mostra nos tubos o IP para conectar, algo como `192.168.4.1`;
3. fica assim até a rede cadastrada aparecer. Isso acontece sozinho quando falta
   luz: o relógio costuma bootar antes do roteador.

Para configurar em modo hotspot: ponha o celular em modo avião, ligue só o
Wi-Fi, conecte em `Relogio` e abra o IP no navegador. Qualquer endereço serve —
`google.com`, o que for — tudo cai na página de configuração (o `dnsmasq`
responde todo domínio com o IP do relógio).

Em modo normal, o site fica em `http://<ip-do-relogio>/` ou
`http://nixie.local/`.

---

## Configurando pela página web

| Aba | O que ajustar |
|---|---|
| **Wi-Fi** | SSID e senha da rede. Respeite maiúsculas e minúsculas — isso aqui é Linux. A senha gravada nunca volta para a página: para salvar, digite de novo. Salvar reinicia o relógio. |
| **Localização** | Latitude, longitude e a **chave da API de previsão**. Sinal negativo para sul e oeste; vírgula ou ponto, tanto faz. A chave gravada não aparece: campo em branco mantém, e a caixa *Apagar* tira. |
| **Servidores NTP** | Fuso (com horário de verão) e até quatro servidores. Vão direto para o sistema: `timedatectl` e `systemd-timesyncd`. |
| **Detecção** | Rosto, movimento ou ambos, tempo aceso, quadros por segundo e limiar de movimento. Do rosto: detector (LBP melhorado, LBP ou Haar), fator de escala, vizinhos mínimos, quadros seguidos para confirmar e tamanhos. Vale na hora, sem reiniciar. A aba mostra o quadro que a câmera entrega e os tamanhos de rosto que a busca testa de fato (`/tmp/camera_face.json`), e avisa quando a faixa pedida não continha nenhum. |
| **Limites das válvulas** | Brilho geral, faixa do bargraph, horário de funcionamento e regeneração de catodo: a automática (até 6 rodadas por dia, cada uma no seu horário, e as voltas por rodada) e a manual, numa válvula escolhida. |
| **Logs** | O log corrente do relógio, sem precisar de SSH. |

**Calibrando o bargraph:** ao mexer nos valores, a válvula mostra a barra.
Ajuste o *mínimo* para a barra cair sobre o zero e o *máximo* para ela chegar
nos 45 °C. De vez em quando precisa reajustar.

> A **chave da previsão do tempo** sai de [weatherapi.com](https://www.weatherapi.com/),
> o plano gratuito basta. Sem chave o relógio funciona normalmente, só não
> mostra temperatura nem usa o nascer/pôr do sol real (cai para o horário fixo
> configurado em *Limites das válvulas*).

---

## Como o relógio é montado por dentro

Três processos e um punhado de scripts de rede:

```
  camera ──signal──▶ nixie ──▶ válvulas, PWM, bargraph, LEDs RGB
    │                  │
    │                  ├──▶ NTP + weatherapi.com (via curl)
    │                  └──◀── /tmp/network_mode, /tmp/wifi.txt
    │
  /dev/video0                lighttpd ──▶ nixie.cgi ──▶ nixie.json
                                │                          │
                                └──▶ www/ (páginas) ◀───────┘
```

| Processo | Papel |
|---|---|
| `camera` | Olha a webcam procurando rostos e movimento. Ao detectar, manda um signal para o `nixie`. |
| `nixie` | O relógio. Processa os signals (e gera os seus), controla todo o hardware, busca hora e previsão. Roda como root — precisa do `pigpio`. |
| `nixie.cgi` | Atende o site: entrega a configuração para as páginas (sem senha nem chave), valida cada campo, grava no `nixie.json`, aplica fuso e NTP no sistema e avisa `nixie` e `camera` por signal. |
| `liblogger.so` | Log compartilhado pelos três, instalada em `/usr/local/lib`. |

**A decisão de rede** fica em `/usr/local/bin/check_wifi_or_hotspot.sh`, disparado
uma vez no boot pelo `wifi-check.service`: espera o `wlan0` pegar IP, testa a
internet com um ping e então ou segue em modo Wi-Fi, ou sobe `dnsmasq` +
`hostapd` e vira hotspot. Ele escreve o veredito em `/tmp/network_mode`, que é
o que o `nixie` lê para decidir se mostra o IP nos tubos. **No fim desse mesmo
script é que o lighttpd sobe** — por isso o `lighttpd-custom.service` vai
instalado mas **desabilitado**: habilitar os dois poria duas instâncias na
porta 80.

Em paralelo, o `check_ssid.service` roda `check_ssid.sh` num laço, procurando a
rede cadastrada no ar. Quando acha, cria `/tmp/wifi.txt` — é assim que um
relógio preso no hotspot descobre que o roteador voltou.

`hostapd` e `dnsmasq` ficam **desabilitados no boot** de propósito: quem os
sobe, sob demanda, é o script acima. Desabilitados, não mascarados — mascarar
impediria o próprio script de iniciá-los.

---

## Desenvolvimento

### Compilando no PC

```bash
cd sources/clock
./docker/build.sh                  # todos os targets
./docker/build.sh nixie camera     # só alguns
./docker/build.sh --shell          # um shell dentro do container
./docker/build.sh --clean          # descarta o diretório de build
```

Os artefatos saem em `sources/clock/dockerbuild/`. As pastas `build/` e
`rpibuild/` guardam caches de CMake gerados no próprio Pi e **não** são
reaproveitáveis no PC.

Targets: `nixie`, `camera`, `nixie.cgi`, `logger`.

### Atualizando um relógio que já está rodando

Sem regravar o cartão: sincronize o build e rode o instalador *no Pi*.

```bash
# no PC
rsync -av --delete sources/clock/dockerbuild/ pi@nixie.local:/home/pi/sources/clock/dockerbuild/

# no Pi
~/nixiepi/update_bins.sh
```

O `update_bins.sh` confere que os quatro artefatos estão lá **antes** de parar
os serviços, copia tudo, instala a `liblogger.so` em `/usr/local/lib`, roda
`ldconfig` e religa os serviços.

Para parar/subir à mão: `~/nixiepi/services.sh {enable|disable|status|restart}`.

### Compilando no próprio Pi

Os fontes vão sempre para `/home/pi/sources/clock`. O toolchain vem instalado
com `--with-devtools`; sem ela, instale no Pi (com internet, não em hotspot):

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config git \
                        libopencv-dev libcurl4-openssl-dev libpigpio-dev
```

Compilando e instalando:

```bash
cd ~/sources/clock
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j1
BUILD_DIR=~/sources/clock/build ~/nixiepi/update_bins.sh
```

Demora: o Zero tem um núcleo só. É por isso que o caminho normal é compilar no
PC e usar o `update_bins.sh`.

### Documentação do código

Gerada por Doxygen (precisa de `doxygen` e `graphviz`) a partir do
`sources/clock/Doxyfile`, rodado de dentro de `sources/clock`. Sai em
`sources/clock/docs/html/`: abra o `index.html`.

---

## Segredos: o que nunca pode entrar no git

Três coisas neste projeto são privadas e **não** podem ser commitadas:

| | Onde fica | |
|---|---|---|
| Senha do Wi-Fi de casa | `www/json/nixie.json` | ignorado pelo git |
| Coordenadas de onde o relógio mora | `www/json/nixie.json` | ignorado pelo git |
| Chave da API de previsão | `www/json/nixie.json` | ignorado pelo git |

O modelo publicável, com os campos vazios, é **`www/json/nixie.default.json`** —
esse sim é versionado, e é dele que o `make_image.sh` parte ao gravar o cartão.
O `nixie.json` de verdade nunca é lido pelo gerador de imagem, justamente para
que a configuração da sua casa não vaze para a imagem que você distribui.

No relógio, o `nixie.json` também não sai pela rede: o `lighttpd.conf` recusa
qualquer `.json`, e as páginas leem a configuração por
`cgi-bin/nixie.cgi?get=config`, que deixa de fora a senha do Wi-Fi e a chave.

Antes de qualquer commit:

```bash
./tools/check_secrets.sh
```

Ele varre o que o git versiona procurando chaves de API, senhas e SSIDs com
valor preenchido, e recusa `www/json/nixie.json` versionado. Para deixar isso
automático, crie `.git/hooks/pre-commit` com:

```bash
#!/usr/bin/env bash
exec "$(git rev-parse --show-toplevel)/tools/check_secrets.sh" --staged
```

e dê `chmod +x` nele. (Um `git commit --no-verify` pula a checagem, para o caso
raro em que ela erra.)

Se alguma linha for um falso positivo (um template, um exemplo), marque-a com
um comentário `check-secrets: ok`.

### O que mais fica fora do git

Além dos segredos, o `.gitignore` corta três categorias — nenhuma delas se
perde, todas são reproduzíveis:

| | Por quê |
|---|---|
| **Binários** e diretórios de build | `./docker/build.sh` os refaz |
| **`sources/clock/docs/`** | Doxygen gera: `cd sources/clock && doxygen Doxyfile` |
| **`sources/opencv/`** (347 MB) | Não participa de build nenhum — o OpenCV vem do apt, no Pi e no container |
| **`lighttpd-1.4.78/`** e o tarball | O `make_image.sh` baixa do site oficial e confere o SHA-256 |

Duas coisas de terceiros **ficam** versionadas, de propósito:

- **`sources/clock/src/logger/spdlog/`** — header-only compilado dentro da
  `liblogger.so`. O Bullseye só empacota o 1.8.1 e aqui o que funciona é o
  1.11.0; trocar seria mexer no que já está de pé por 1 MB.
- **`nixiepi/*.xml`** — vêm do OpenCV, mas são dados de runtime, não fonte:
  sem elas o relógio sobe mostrando `99   1`.

---

## Diagnóstico

O relógio mostra códigos de erro nos próprios tubos:

| Display | Significa |
|---|---|
| `99   1` | Falha de configuração da câmera — o detector de rosto escolhido (`~/nixiepi/*.xml`) faltando ou corrompido. Trocar de detector na aba *Detecção* também resolve |
| `99   2` | Falha de hardware da câmera — mau contato, ou queimou |
| `99   3` | Sem Wi-Fi. Definido mas não implementado; se aparecer, é novidade |

Logs:

```bash
tail -f /tmp/nixie.txt            # o log do relógio (também na aba "Logs" do site)
cat /tmp/hotspot_debug.log        # o que o script de rede decidiu no boot
cat /tmp/network_mode             # "WIFI" ou "HOTSPOT"
ls  /tmp/wifi.txt                 # existe = a rede cadastrada está no ar
journalctl -u nixie -u camera -f
```

---

## Onde cada coisa vai parar no cartão

| No cartão | Vem de |
|---|---|
| `/home/pi/nixiepi/{nixie,camera}` | `sources/clock/dockerbuild/` |
| `/home/pi/nixiepi/*.xml` | `nixiepi/` (cascatas; qual usar se escolhe na aba *Detecção*) |
| `/home/pi/nixiepi/{services,update_bins}.sh` | `nixiepi/` |
| `/home/pi/www/` | `www/` |
| `/home/pi/sources/clock/` | `sources/clock/` (sem os diretórios de build) |
| `/home/pi/projeto/` | `projeto/` |
| `/home/pi/leiame.txt` | `leiame.txt` — o manual de quem só tem o relógio na mão |
| `/home/pi/www/cgi-bin/nixie.cgi` | `sources/clock/dockerbuild/` |
| `/home/pi/www/json/nixie.json` | `www/json/nixie.default.json` + opções do script |
| `/usr/local/lib/liblogger.so` | `sources/clock/dockerbuild/` |
| `/usr/local/{sbin/lighttpd,lib/mod_*.so}` | compilado de `sources/lighttpd-1.4.78.tar.gz` |
| `/usr/local/bin/*.sh` | `sources/scripts/usr/local/bin/` |
| `/etc/systemd/system/*.service` | `sources/configs/etc/systemd/system/` |
| `/etc/{dnsmasq.conf,dhcpcd.conf,hostapd/,default/hostapd,network/interfaces}` | `sources/configs/etc/` |

Serviços habilitados no boot: `nixie`, `camera`, `wifi-check`, `check_ssid`.
Instalado e **desabilitado**: `lighttpd-custom` (veja
[Como o relógio é montado por dentro](#como-o-relógio-é-montado-por-dentro)).

O `config.txt` ganha um bloco `# --- nixie clock ---` com `dtparam=i2c_arm=on`,
`enable_uart=1`, `gpu_mem=128`, `start_x=1` e `dtoverlay=i2c-rtc,ds3231`, e o
módulo `i2c-dev` entra em `/etc/modules`.
