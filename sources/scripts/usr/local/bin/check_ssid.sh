#!/bin/bash

# Interface Wi-Fi (ajuste se necessário, ex: wlan1)
IFACE="wlan0"

# Caminho para o arquivo wpa_supplicant.conf
WPA_CONF="/etc/wpa_supplicant/wpa_supplicant.conf"

# Arquivo temporário a ser criado se SSID for encontrado
OUTPUT_FILE="/tmp/wifi.txt"

# Extrai o SSID do arquivo wpa_supplicant.conf
get_ssid() {
    grep -oP 'ssid="\K[^"]+' "$WPA_CONF" | head -n 1
}

while true; do
    SSID=$(get_ssid)

    if [ -z "$SSID" ]; then
        echo "SSID não encontrado em $WPA_CONF"
        sleep 30
        continue
    fi

    # Escaneia redes disponíveis
    SCAN_OUTPUT=$(sudo iwlist "$IFACE" scan 2>/dev/null)

    if echo "$SCAN_OUTPUT" | grep -q "ESSID:\"$SSID\""; then
        touch "$OUTPUT_FILE"
        echo "$(date): SSID '$SSID' encontrado. Arquivo criado." >> /tmp/wifi_log.txt
    else
        echo "$(date): SSID '$SSID' não encontrado." >> /tmp/wifi_log.txt
    fi

    sleep 30
done
