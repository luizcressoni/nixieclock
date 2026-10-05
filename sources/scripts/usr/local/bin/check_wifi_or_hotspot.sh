#!/bin/bash

WLAN_INTERFACE="wlan0"
PING_TARGET="8.8.8.8"
MAX_WAIT=90
WAITED=0
STATUS_FILE="/tmp/network_mode"

echo "[DEBUG] Script started at $(date)" >> /tmp/hotspot_debug.log
echo "[INFO] Waiting for Wi-Fi..." >> /tmp/hotspot_debug.log

# Wait for an IP address
while ! ip addr show $WLAN_INTERFACE | grep -q "inet " && [ $WAITED -lt $MAX_WAIT ]; do
  sleep 1
  WAITED=$((WAITED+1))
done

ping -c 2 -W 2 $PING_TARGET > /dev/null 2>&1

if [ $? -eq 0 ]; then
  echo "[INFO] Online. Running in normal mode (WIFI)." >> /tmp/hotspot_debug.log
  echo "WIFI" > "$STATUS_FILE"

  systemctl stop hostapd
  systemctl stop dnsmasq
#  ip link set $WLAN_INTERFACE down
#  ip addr flush dev $WLAN_INTERFACE
#  systemctl start dhcpcd

else
  echo "[INFO] Offline. Switching to hotspot mode (HOTSPOT)." >> /tmp/hotspot_debug.log
  # One redirect per line: "> file >> log" writes only to the LAST one, which left
  # $STATUS_FILE empty and the clock unaware it was in hotspot mode.
  echo "HOTSPOT" > "$STATUS_FILE"

  systemctl stop dhcpcd

  ip link set $WLAN_INTERFACE down
  ip addr flush dev $WLAN_INTERFACE
  ip addr add 192.168.4.1/24 dev $WLAN_INTERFACE
  ip link set $WLAN_INTERFACE up

  systemctl start dnsmasq
  systemctl start hostapd
fi

echo "[DEBUG] Script finished at $(date)" >> /tmp/hotspot_debug.log

lighttpd -D -f /home/pi/www/lighttpd.conf
