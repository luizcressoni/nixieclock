/*! \file config.cpp*/
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unistd.h>
#include "config.h"
#include "../utils/json_parser.h"

sNixieConfig    gNixieConfig;
sCameraConfig   gCameraConfig;
sLocation       gLocation;
sWifiConfig     gWifiConfig;
sScheduleConfig gScheduleConfig;
sRegenConfig    gRegenConfig;

/*! \brief Makes localtime() follow a timezone changed while the clock is running
    \note glibc reads /etc/localtime once and caches it for as long as TZ stays the same, so a
    \note zone set from the web page would only show up after a reboot. Pointing TZ at the file
    \note the link resolves to changes TZ exactly when the zone changes, which forces the reload.
*/
static void refresh_timezone()
{
    char target[256];
    const ssize_t n = readlink("/etc/localtime", target, sizeof(target) - 1);
    if(n <= 0)
        return;
    target[n] = 0;

    std::string tz = ":";
    if(target[0] != '/')
        tz += "/etc/";      //a relative link resolves from the directory that holds it
    tz += target;

    //setenv() is not safe against a concurrent getenv(), so it only runs when there is news
    if(const char *current = getenv("TZ"); current != nullptr && tz == current)
        return;
    setenv("TZ", tz.c_str(), 1);
    tzset();
}

/*! \brief Loads the JSON configuration file and populates the data structures */
void load_config_file()
{
    free_jsonfile();
    load_nixie_config(&gNixieConfig);
    load_camera_config(&gCameraConfig);
    load_location(&gLocation);
    load_wifi_config(&gWifiConfig);
    load_schedule_config(&gScheduleConfig);
    load_regen_config(&gRegenConfig);
    refresh_timezone();
}

//eof config.cpp
