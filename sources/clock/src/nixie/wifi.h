/*! \file wifi.h */
#pragma once
#include <string>


enum networkStatus
{
    NETWORK_STATUS_DISCONNECTED = 0,
    NETWORK_STATUS_WIFI,
    NETWORK_STATUS_HOTSPOT
};


networkStatus getNetworkStatus();
bool save_wifiConfig(const char* ssid, const char* password);
bool getIPAddressBytes(const std::string& interfaceName, uint8_t ipBytes[4]);

//eof wifi.h