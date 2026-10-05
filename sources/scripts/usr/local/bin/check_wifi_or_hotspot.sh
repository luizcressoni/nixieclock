#!/bin/bash

WLAN_INTERFACE="wlan0"
PING_TARGET="8.8.8.8"
MAX_WAIT=90
WAITED=0
STATUS_FILE="/tmp/network_mode"

echo "[DEBUG] Script iniciado as $(date)" >> /tmp/hotspot_debug.log
echo "[INFO] Aguardando conexao WiFi..." >> /tmp/hotspot_debug.log

# Aguarda até que o IP seja atribuído
while ! ip addr show $WLAN_INTERFACE | grep -q "inet " && [ $WAITED -lt $MAX_WAIT ]; do
  sleep 1
  WAITED=$((WAITED+1))
done

# Testa conectividade com a internet
ping -c 2 -W 2 $PING_TARGET > /dev/null 2>&1

if [ $? -eq 0 ]; then
  echo "[INFO] Conectado a internet. Rodando modo normal (WIFI)." >> /tmp/hotspot_debug.log
  echo "WIFI" > "$STATUS_FILE"

  systemctl stop hostapd
  systemctl stop dnsmasq
#  ip link set $WLAN_INTERFACE down
#  ip addr flush dev $WLAN_INTERFACE
#  systemctl start dhcpcd

else
  echo "[INFO] Sem conexao. Ativando modo hotspot (HOTSPOT)." >> /tmp/hotspot_debug.log
  # Uma linha, um redirecionamento: "> arquivo >> log" manda a saida para o ULTIMO deles,
  # entao o $STATUS_FILE ficava vazio e o relogio nunca descobria que estava em hotspot.
  echo "HOTSPOT" > "$STATUS_FILE"

  systemctl stop dhcpcd

  ip link set $WLAN_INTERFACE down
  ip addr flush dev $WLAN_INTERFACE
  ip addr add 192.168.4.1/24 dev $WLAN_INTERFACE
  ip link set $WLAN_INTERFACE up

  systemctl start dnsmasq
  systemctl start hostapd
fi

echo "[DEBUG] Script finalizado as $(date)" >> /tmp/hotspot_debug.log

lighttpd -D -f /home/pi/www/lighttpd.conf
