/*! \file structs.h */
#pragma once
#include "defines.h"

enum enumCameraDetectModel{
    CAM_DETECT_FACE,
    CAM_DETECT_MOTION,
    //The camera always ran both detectors regardless of what this said -- it read the setting
    //and never looked at it again. Now that it is honoured, "both" has to exist, or turning on
    //the setting would be the same as turning one of the two detectors off.
    CAM_DETECT_BOTH,
    CAM_DETECT_NONE
};

/*! \brief Which OpenCV cascade looks for faces */
enum enumFaceCascade{
    FACE_CASCADE_LBP_IMPROVED,  //!< the default: 45x45 window, the best of the three on this camera
    FACE_CASCADE_LBP,
    FACE_CASCADE_HAAR           //!< the most thorough and by far the slowest on a Pi Zero
};

struct sCameraConfig
{
    enumCameraDetectModel detection_model;
    int threshold;          //!< motion: pixel difference that counts as change
    enumFaceCascade faceCascade;
    double faceScaleFactor; //!< face: step between search sizes, above 1; smaller is slower and finer
    int faceMinNeighbors;   //!< face: overlapping hits a candidate needs; more means fewer false alarms
    int faceMinConsecutive; //!< face: frames in a row before a face wakes the clock
    int faceSizeMin;        //!< face: smallest face searched for, in pixels
    int faceSizeMax;        //!< face: largest face searched for, in pixels
    int on_timeout;         //!< seconds the tubes stay lit after a detection
    int motion_timeout;     //!< seconds undirected motion alone keeps them lit, at most on_timeout
    int fps;
};

struct sNixieConfig
{
    int vu_min;
    int vu_max;
    int brightness;
};

/*! \brief NTP servers handed to systemd-timesyncd
    \note Only the CGI reads these. The clock takes its time from the system, never from here.
*/
struct sNtpSettings
{
    char ntp_server[4][64];
};

struct sLocation
{
    double latitude;
    double longitude;
    //The weather service key used to be a default argument in weather.h, which put a working
    //credential into every copy of the source. It rides along with the coordinates because it
    //is the same web page that sets both and the same request that spends both.
    char apikey[64];
};

struct sWifiConfig
{
    char ssid[33];          //!< 802.11 allows 32 bytes, plus the terminator
    char password[64];      //!< WPA passphrase, 8 to 63 characters
};

/*! \brief When the clock is allowed to light the tubes on its own */
struct sScheduleConfig
{
    bool use_astro;         //!< ask the weather service for the real sunrise and sunset
    int  day_start;         //!< fallback window, whole hours, used when no astro is available
    int  day_end;           //!< both ends inclusive
    bool gate_half_hour;    //!< also hold back the half hour date display at night
};

/*! \brief The automatic cathode regeneration */
struct sRegenConfig
{
    bool enabled;
    int  count;                         //!< sessions per day, 1 to REGEN_SESSIONS_MAX
    int  minutes[REGEN_SESSIONS_MAX];   //!< start of each session, minutes past midnight, ascending
    int  cycles;                        //!< full 0..9 sweeps per session
};


// struct sNixieAction
// {
//     int action;
//     int value;
// };

//eof structs.h