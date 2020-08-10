/*! \file nixie_clock.h */
#pragma once

#include "pwm.h"
#include "rgb.h"
#include "numbers.h"
#include "ctimer.h"
#include "rtc.h"
#include "weather.h"
#include <tinyxml2.h>

#define DIMMER_THREAD_SLEEP     10000   //50
#define VU_THREAD_SLEEP         10000   //50  //10



/**
    Clock components:

    - 6 Nixie tubes driven by 3 74HC595 shift registers
        a 24 bit word is used to set the tube digits
    - 1 Neon lamp for the ':' symbols
    - 1 Neon lamp for Temperature indicator
    - 1 Neon lamp for Day of the week indicator
    - 1 bargraph tube for both temperature & weekday
    - 1 RGB led under each tube (all driven together), PWM controlled
    - 1 RESET push-button

    Optional:
    - USB mic for voice commands
    - integrated webcam for face detection

    Software:
    - web server for configuration
    - webcam viewer



    curl -L https://raw.github.com/pageauc/opencv3-setup/master/setup.sh | bash

    curl comand will run the GitHub setup.sh script that will install files and configure into the ~/opencv3-setup folder.

The cv3-install.menu.sh script and menu picks will

* Validate that OPENCV_VER variable setting is correct
* Create cv3-log.txt If It Does Not Exist. The log will record system information, date/time and details of various steps including execution times
* Update/upgrade Raspbian for Raspberry Pi
* Install build dependencies
* Download opencv3 source zip files and unzip
* Auto Detect RPI3 and set NEON compile directive for cmake to enhance cv3 performance
* Run cmake to configure build
* Temporarily Increase Swap memory to 1024 MB During make
* Automatically Sets number of make cores. -j2 cores if 1 GB RAM detected, otherwise -j1 core directive is set.
* Run make to Compile opencv3 source code
* Run optional make clean to clear build directory to force full recompile.
* DELETE menu pick to optionally recover disk space by deleting the cv3-tmp folder containing opencv source and build files and folders.


*/



/** FSM

    1 - boot
            out: --> wifi
    2 - check for wifi config
        if OK   3
            NOK 4
    3 - get IP from DHCP    RGB=blue
            show IP on display
            5
    4 - start wifi hotspot
        set static IP - show on display, RGB=red
        5
    5 - start web server
        get time from ntp / chip
        6
    6 - main loop
            check for button
            scan event triggers
            run events

*/


/** EVENTS
    event   type: time changed
            condition: sec ++    (every second)
            condition: min ++    (every minute)
            condition: hour ++   (every hour)
            condition: day++     (every day)
            condition: month++   (every month)
            condition: year++    (every year)
            condition:   time mask match
            data: current timestamp
            actions:
                    glow value
                    glow ramp
                    glow anim
                    rgb color
                    rgb ramp
                    rgb anim
                    show time
                    show date
                    show temperature
                    show weekday
            type: button

            type: temp_changed

*/


/** anim types
    dry              = replace instantly
    flip(delay_ms)   = replace adjusting digits one unit at a time
    scroll(delay_ms) = replace scrolling bits

*/

/*!
    stores hardware configuration values

*/
struct  sNixieClockHardConfig
{
    __u8    u8DimmerPwmMin;
    __u8    u8DimmerPwmMax;

    __u8    u8VuMin;
    __u8    u8VuMax;

    __u8    u8RgbMax;
    bool    rtc_present;
    bool    rgbthreadEnable;
    bool    brightnessthreadEnable;
};

enum eClockFsmState
{
    eClockFsmStateBoot  = 0,
    eClockFsmStateLoop,
    eClockFsmStateFatal
};


class cNixieClock
{
  protected:
    cNixiePwm               *mcNixieDimmer,
                            *mNixieVu;
    cNeoRpiRgbh             *mcNeoRpiRgbh;
    cNixieNumbers           *mcNixieNumbers;

    cNeoGpioRaspi           *mGpioHms,
                            *mGpioTemp,
                            *mGpioWeekday;

    cDS3231rtc              *mcDS3231rtc;
    cNixieWeather           mcNixieWeather;
    void                    SetWeatherData();

    tinyxml2::XMLElement    *mXmlNixie;
    bool                    LoadXml(tinyxml2::XMLElement *_xmlnixie);

    sNixieClockHardConfig   msNixieClockHardConfig;
    void                    SetHardwareConfig();

    void                    FreeStuff();
    void                    LoadStuff();

    //fsm
    eClockFsmState          meClockFsmState;
    eClockFsmState          GetFsmState();
    void                    SetFsmState(eClockFsmState _state);
    __u32                   FsmTask();
    void                    FsmBootIni();
    __u32                   FsmBoot();

    void                    FsmLoopIni();
    __u32                   FsmLoop();
    //events
    time_t                  *mpTm;
    tm                      mOldTm, mNewTm;
    void                    ProcessEvents();
    tinyxml2::XMLElement    *GetEventbyName(const char *_eventname);
    void                    ExecuteEvent(tinyxml2::XMLElement *_event);
    tinyxml2::XMLElement    *FindNewEvent(tm *_now);
    bool                    MatchConditionTime(tinyxml2::XMLElement *_event, tm *_now);

    void                    ProcessTubeEvent(tinyxml2::XMLElement *_event);
    void                    ProcessBarGraphEvent(tinyxml2::XMLElement *_event);
    void                    ProcessRgbEvent(tinyxml2::XMLElement *_event);
    void                    ProcessIndicatorEvent(tinyxml2::XMLElement *_event);
    void                    ProcessBrightnessEvent(tinyxml2::XMLElement *_event);

    //aux
    bool                    CheckWpaFile(const char *_ssid, const char *_password);
    void                    SaveWpaFile(const char *_ssid, const char *_password, const char *_country);
    bool                    CheckIp();

    cTimer                  mTimerAux, mTimerFaceDetect;
    //display
    void                    UpdateDisplayTxt(tm *now);
    char                    mTxtTime[7],
                            mTxtDate[7],
                            mTxtIp[7],
                            *mpTxt;
    double                  mDblCurrentTemperature;
    bool                    mDisplayLock;
  public:
    cNixieClock();
    ~cNixieClock();

    bool    Init(tinyxml2::XMLElement *_xmlnixie);

    __u32   Task(time_t *_tm = NULL);
    void    SetFaceDetected(__u32 _u32timeoutms);
};



/**
App ID
FvDXZg30
Client ID (Consumer Key)
dj0yJmk9ZG05Nm9YYncxYjdmJmQ9WVdrOVJuWkVXRnBuTXpBbWNHbzlNQS0tJnM9Y29uc3VtZXJzZWNyZXQmc3Y9MCZ4PWUy
Client Secret (Consumer Secret)
c97712a75b345d9a40937d5169543446706e1959

**/

//eof nixie_clock.h
