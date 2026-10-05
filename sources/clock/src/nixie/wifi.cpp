/*! \file wifi.cpp */
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <cstring>
#include <string>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "wifi.h"
#include "../utils/defines.h"

/*! \brief Gets the network connection status from an external file
    \note there is a bash script that updates this file
    \note the file contains either "WIFI" or "HOTSPOT"
*/
networkStatus getNetworkStatus()
{
    networkStatus status = NETWORK_STATUS_DISCONNECTED;

    std::ifstream networkCheckFile(NETWORK_CHECK_FILE);
    if (networkCheckFile.is_open())
    {
        std::string line;
        if (std::getline(networkCheckFile, line))
        {
            if (line.substr(0, 4) == "WIFI")
            {
                status = NETWORK_STATUS_WIFI;
            }
            else if (line.substr(0, 7) == "HOTSPOT")
            {
                status = NETWORK_STATUS_HOTSPOT;
            }
        }
        networkCheckFile.close();
    }

    return status;
}

/*! \brief Tells whether a string can go between quotes in wpa_supplicant.conf
    \note A quote, a backslash or a line break would end the value early and let the rest of the
    \note string be read as configuration lines of its own, written by a process running as root.
*/
static bool is_wpa_safe(const char *_text)
{
    for(const unsigned char *c = reinterpret_cast<const unsigned char *>(_text); *c; c++)
        if(*c < 0x20 || *c == 0x7f || *c == '"' || *c == '\\')
            return false;
    return true;
}

/*! \brief Saves the WiFi configuration to the wpa_supplicant.conf file
    \param ssid The SSID of the WiFi network
    \param password The password for the WiFi network
    \return true if the configuration was saved successfully, false otherwise
    \note The CGI already refuses unsafe values; this check covers a config file edited by hand.
*/
bool save_wifiConfig(const char* ssid, const char* password)
{
    if(ssid[0] == 0 || !is_wpa_safe(ssid) || !is_wpa_safe(password))
        return false;

    std::ofstream configFile("/etc/wpa_supplicant/wpa_supplicant.conf", std::ios::out | std::ios::trunc);
    if (!configFile.is_open())
    {
        configFile.open("/etc/wpa_supplicant/wpa_supplicant.conf", std::ios::out | std::ios::trunc);
    }
    if (configFile.is_open())
    {
        configFile << "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n";
        configFile << "ap_scan=1\n\n";
        configFile << "update_config=1\n\n";
        configFile << "network={\n";
        configFile << "    ssid=\"" << ssid << "\"\n";
        configFile << "    psk=\"" << password << "\"\n";
        configFile << "}\n";
        configFile.close();
        //struct stat st{};
        int result = chmod("/etc/wpa_supplicant/wpa_supplicant.conf", S_IRUSR | S_IWUSR);

        return result == 0;
    }
    return false;
}

/*! \brief Gets the interface IP as a 4-byte array
    \param interfaceName The name of the network interface (e.g., "wlan0")
    \param ipBytes A pointer to a 4-byte array where the IP address will be stored
    \return true if the IP address was retrieved successfully, false otherwise
    \note This function uses ioctl to get the IP address of the specified network interface
*/
bool getIPAddressBytes(const std::string& interfaceName, uint8_t ipBytes[4])
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
    {
        close(fd);
        return false;
    }

    close(fd);

    auto* ipaddr = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
    const uint32_t ip = ipaddr->sin_addr.s_addr;

    ipBytes[0] = ip & 0xFF;
    ipBytes[1] = (ip >> 8) & 0xFF;
    ipBytes[2] = (ip >> 16) & 0xFF;
    ipBytes[3] = (ip >> 24) & 0xFF;

    return true;
}

//eof wifi.cpp