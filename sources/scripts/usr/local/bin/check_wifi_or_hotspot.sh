#!/bin/bash

# Picks Wi-Fi or hotspot. Runs once at boot through wifi-check.service, and again whenever
# nixie, sitting in hotspot mode, sees the home network in range (check_ssid.sh) and restarts
# that service to try it.

WLAN_INTERFACE="wlan0"
PING_TARGET="8.8.8.8"
MAX_WAIT=90
WAITED=0
STATUS_FILE="/tmp/network_mode"
SSID_FILE="/tmp/wifi.txt"
LOG="/tmp/hotspot_debug.log"

echo "[DEBUG] Script started at $(date)" >> "$LOG"

# Whatever is in there describes a previous run. nixie must not read an old verdict while this
# one is still being decided, and an old wifi.txt would send a clock that ends up in hotspot
# mode straight back here.
rm -f "$STATUS_FILE" "$SSID_FILE"

# Run again from hotspot mode: give wlan0 back to wpa_supplicant (dhcpcd starts it on Bullseye)
# and drop the static 192.168.4.1, which would otherwise pass for a DHCP lease below.
if systemctl is-active --quiet hostapd; then
  echo "[INFO] Leaving hotspot mode." >> "$LOG"
  systemctl stop hostapd
  systemctl stop dnsmasq
  ip addr flush dev $WLAN_INTERFACE
  systemctl restart dhcpcd
fi

# Associated with the stored network and holding a real lease. Deliberately not "the internet
# answers": after a power cut the router's Wi-Fi is back in seconds and its uplink takes minutes,
# and judging by a ping put the clock in hotspot mode, check_ssid.sh then saw the network, and
# the clock rebooted, over and over until the uplink came up. 169.254.x.x is dhcpcd's own
# fallback address when no DHCP server answered, so it does not count.
connected() {
  wpa_cli -i $WLAN_INTERFACE status 2>/dev/null | grep -q "^wpa_state=COMPLETED" || return 1
  ip -4 addr show $WLAN_INTERFACE | grep "inet " | grep -vq "inet 169\.254\."
}

echo "[INFO] Waiting for Wi-Fi..." >> "$LOG"
while ! connected && [ $WAITED -lt $MAX_WAIT ]; do
  sleep 1
  WAITED=$((WAITED+1))
done

if connected; then
  echo "[INFO] Connected after ${WAITED}s. Running in normal mode (WIFI)." >> "$LOG"
  # Only informative: nixie retries the forecast and timesyncd the clock on their own.
  if ! ping -c 2 -W 2 $PING_TARGET > /dev/null 2>&1; then
    echo "[INFO] No internet yet, staying on Wi-Fi anyway." >> "$LOG"
  fi
  echo "WIFI" > "$STATUS_FILE"

  systemctl stop hostapd
  systemctl stop dnsmasq

else
  echo "[INFO] Offline. Switching to hotspot mode (HOTSPOT)." >> "$LOG"
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

echo "[DEBUG] Script finished at $(date)" >> "$LOG"

# Foreground, so it lives and dies with wifi-check.service: when nixie restarts the service to
# try Wi-Fi again, systemd stops this lighttpd along with the old run before starting the new one.
lighttpd -D -f /home/pi/www/lighttpd.conf
