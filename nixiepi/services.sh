#!/bin/bash

SERVICES=("nixie.service" "camera.service")

case "$1" in
    enable)
        echo "Enabling and starting services..."
        for s in "${SERVICES[@]}"; do
            sudo systemctl enable "$s"
            sudo systemctl start "$s"
        done
        ;;
    disable)
        echo "Stopping and disabling services..."
        for s in "${SERVICES[@]}"; do
            sudo systemctl stop "$s"
            sudo systemctl disable "$s"
        done
        ;;
    status)
        echo "Checking services status:"
        for s in "${SERVICES[@]}"; do
            sudo systemctl status --no-pager "$s"
        done
        ;;
    restart)
        echo "Restarting services..."
        for s in "${SERVICES[@]}"; do
            sudo systemctl restart "$s"
        done
        ;;
    *)
        echo "Usage: $0 {enable|disable|status|restart}"
        exit 1
        ;;
esac
