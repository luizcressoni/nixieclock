#!/bin/bash

# Wi-Fi interface (e.g. wlan1)
IFACE="wlan0"

WPA_CONF="/etc/wpa_supplicant/wpa_supplicant.conf"

# Created when the SSID is in range
OUTPUT_FILE="/tmp/wifi.txt"

# First SSID in wpa_supplicant.conf
get_ssid() {
    grep -oP 'ssid="\K[^"]+' "$WPA_CONF" | head -n 1
}

while true; do
    SSID=$(get_ssid)

    if [ -z "$SSID" ]; then
        echo "No SSID in $WPA_CONF"
        sleep 30
        continue
    fi

    SCAN_OUTPUT=$(sudo iwlist "$IFACE" scan 2>/dev/null)

    if echo "$SCAN_OUTPUT" | grep -q "ESSID:\"$SSID\""; then
        touch "$OUTPUT_FILE"
        echo "$(date): SSID '$SSID' found. File created." >> /tmp/wifi_log.txt
    else
        echo "$(date): SSID '$SSID' not found." >> /tmp/wifi_log.txt
    fi

    sleep 30
done
