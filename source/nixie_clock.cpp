/*! \file nixie_clock.cpp */
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include "nixie_clock.h"
#include "functions.h"
#include "clientsocket.h"

enuModulationType get_modulation_type(tinyxml2::XMLElement *_event)
{
    if(_event->Attribute("modulation", "sinusoidal"))
    {
        return enuModulationTypeSinusoidal;
    }
    else if(_event->Attribute("modulation", "ramp"))
    {
        return enuModulationTypeRamp;
    }
    else if(_event->Attribute("modulation", "flash"))
    {
        return enuModulationTypeFlash;
    }
    return enuModulationTypeNone;
}


void cNixieClock::SetHardwareConfig()
{
    __u8 hits=0;
    if(mXmlNixie)
    {
        tinyxml2::XMLElement *hw = mXmlNixie->FirstChildElement("hardware");
        if(hw != NULL)
        {
            tinyxml2::XMLElement *element = hw->FirstChildElement("vu");
            if(element)
            {
                msNixieClockHardConfig.u8VuMin = element->IntAttribute("min");
                msNixieClockHardConfig.u8VuMax = element->IntAttribute("max");
                hits++;
            }
            element = hw->FirstChildElement("rgb");
            if(element)
            {
                msNixieClockHardConfig.u8RgbMax = element->IntAttribute("max");
                msNixieClockHardConfig.rgbthreadEnable = element->IntAttribute("threadEnable");

                hits++;
            }
            element = hw->FirstChildElement("brightness");
            if(element)
            {
                msNixieClockHardConfig.u8DimmerPwmMin = element->IntAttribute("min");
                msNixieClockHardConfig.u8DimmerPwmMax = element->IntAttribute("max");
                msNixieClockHardConfig.brightnessthreadEnable = element->IntAttribute("threadEnable");
                hits++;
            }

            element = hw->FirstChildElement("rtc");
            if(element)
            {
                msNixieClockHardConfig.rtc_present = element->IntAttribute("enable");
                hits++;
            }


        }
    }

    if(hits != 4)
    {
        msNixieClockHardConfig.u8DimmerPwmMax           = 0;
        msNixieClockHardConfig.u8DimmerPwmMin           = 100;
        msNixieClockHardConfig.brightnessthreadEnable   = false;
        msNixieClockHardConfig.u8RgbMax                 = 50;
        msNixieClockHardConfig.rgbthreadEnable          = false;
        msNixieClockHardConfig.u8VuMax                  = 90;
        msNixieClockHardConfig.u8VuMin                  = 15;
        msNixieClockHardConfig.rtc_present              = false;

    }
}


void cNixieClock::FreeStuff()
{
    if(mcDS3231rtc != NULL)
    {
        delete mcDS3231rtc;
        mcDS3231rtc = NULL;
    }

    if(mcNixieDimmer != NULL)
    {
        delete mcNixieDimmer;
        mcNixieDimmer = NULL;
    }

    if(mNixieVu != NULL)
    {
        delete mNixieVu;
        mNixieVu = NULL;
    }

    if(mcNeoRpiRgbh != NULL)
    {
        delete mcNeoRpiRgbh;
        mcNeoRpiRgbh = NULL;
    }

    if(mcNixieNumbers != NULL)
    {
        delete mcNixieNumbers;
        mcNixieNumbers = NULL;
    }

    if(mGpioHms != NULL)
        delete mGpioHms;
    if(mGpioTemp != NULL)
        delete mGpioTemp;
    if(mGpioWeekday != NULL)
        delete mGpioWeekday;

}

void cNixieClock::LoadStuff()
{
    //Nixie tube dimmer
    mcNixieDimmer = new cNixiePwm(PIN_PWM_LAMPS);
    mcNixieDimmer->SetLimits(msNixieClockHardConfig.u8DimmerPwmMin, msNixieClockHardConfig.u8DimmerPwmMax);
    if(msNixieClockHardConfig.brightnessthreadEnable)
        mcNixieDimmer->StartThread(gDimmerThread, DIMMER_THREAD_SLEEP);
    mcNixieDimmer->SetPercent(100);

    //Nixie VU
    mNixieVu = new cNixiePwm(PIN_PWM_T);
    mNixieVu->SetLimits(msNixieClockHardConfig.u8VuMin, msNixieClockHardConfig.u8VuMax);
    mNixieVu->SetModulator(enuModulationTypeRamp);
    mNixieVu->GetModulatorPtr()->SetPeriod(5000);
//    mNixieVu->StartThread(gVuThread, VU_THREAD_SLEEP);

    //RGB LEDs
    mcNeoRpiRgbh = new cNeoRpiRgbh(PIN_RGB_R, PIN_RGB_G, PIN_RGB_B);
    //mcNeoRpiRgbh->SetRgbMax(msNixieClockHardConfig.u8RgbMax);
    if(msNixieClockHardConfig.rgbthreadEnable)
        mcNeoRpiRgbh->StartThread();
    mcNeoRpiRgbh->SetRgb(0,0,0);    //all off

    //lamps
//    printf("LAMPS***********\n");
    mGpioHms        = new cNeoGpioRaspi(PIN_LAMP_HMS, OUTPUT_PIN);
    mGpioTemp       = new cNeoGpioRaspi(PIN_LAMP_TEMP, OUTPUT_PIN);
    mGpioWeekday    = new cNeoGpioRaspi(PIN_LAMP_WDAY, OUTPUT_PIN);

    mGpioHms->SetValue(PIN_LOW);
    mGpioTemp->SetValue(PIN_LOW);
    mGpioWeekday->SetValue(PIN_LOW);

    //Nixie numbers
    mcNixieNumbers = new cNixieNumbers(PIN_N_CLOCK, PIN_N_DATA, PIN_N_BCLOCK, PIN_N_RESET);
    mcNixieNumbers->SetValue((__u32)0);

    //rtc chip
    if(msNixieClockHardConfig.rtc_present)
        mcDS3231rtc = new cDS3231rtc();
}

cNixieClock::cNixieClock()
{
    mcDS3231rtc     = NULL;
    mcNixieDimmer   = NULL;
    mNixieVu        = NULL;
    mcNeoRpiRgbh    = NULL;
    mcNixieNumbers  = NULL;
    mGpioHms        = NULL;
    mGpioTemp       = NULL;
    mGpioWeekday    = NULL;
    mpTm            = NULL;

    mDblCurrentTemperature = 0.0;
    mDisplayLock = false;
    mTimerFaceDetect.Enable(false);
}

cNixieClock::~cNixieClock()
{
    FreeStuff();
}

bool cNixieClock::Init(tinyxml2::XMLElement *_xmlnixie)
{
    FreeStuff();
    if(LoadXml(_xmlnixie))
    {
        SetHardwareConfig();
        LoadStuff();
        FsmBootIni();
    }
    else
        return false;
    time_t now;
    time(&now);
    mOldTm = *localtime(&now);
    return true;
}

bool cNixieClock::LoadXml(tinyxml2::XMLElement *_xmlnixie)
{
    if(_xmlnixie == NULL)
    {
        fprintf(stderr, "cNixieClock::LoadXml - ERROR\n");
        SetFsmState(eClockFsmStateFatal);
        return false;
    }
    mXmlNixie = _xmlnixie;
    return true;
}

__u32 cNixieClock::Task(time_t *_tm)
{
    if(_tm != NULL && mpTm == NULL)
    {
        //event
        mpTm = _tm;
    }
    __u32 u32tasksleepUs = FsmTask();

    if(mTimerFaceDetect.IsEnabled() && mTimerFaceDetect.IsTimeOut())
    {
        tinyxml2::XMLElement *aux = mXmlNixie->FirstChildElement("face_detection");
        if(aux)
        {
//            printf("Face OFF\n");
            ExecuteEvent(aux->FirstChildElement("off"));
        }
        //printf("FaceTimer OFF\n");
        if(mcNixieDimmer)
            mcNixieDimmer->SetPercent(0);
        mTimerFaceDetect.Enable(false);
    }

    return u32tasksleepUs;
}

eClockFsmState cNixieClock::GetFsmState()
{
    return meClockFsmState;
}

void cNixieClock::SetFsmState(eClockFsmState _state)
{
    meClockFsmState = _state;
}

__u32 cNixieClock::FsmTask()
{
    __u32 u32returnValue = 0;
    switch(GetFsmState())
    {
    case eClockFsmStateBoot:
        u32returnValue = FsmBoot();
        break;
    case eClockFsmStateLoop:
        u32returnValue = FsmLoop();
        break;
    case eClockFsmStateFatal:
        u32returnValue = 1000000;
        break;
    }
    return u32returnValue;
}

bool cNixieClock::CheckWpaFile(const char *_ssid, const char *_password)
{
    __u8 u8hits = 0;
    if(_ssid == NULL || _password == NULL)
        return false;
    FILE *wpafile = fopen("/etc/wpa_supplicant/wpa_supplicant.conf","rb");
    if(wpafile == NULL)
        return false;

    while(!feof(wpafile))
    {
        char line[255];
        if(fgets(line, 254, wpafile) != NULL)
        {
            if(strstr(line,"ssid") != NULL)
            {
                //ssid line found. test
                if(strstr(line,_ssid) == NULL)
                {
                    //different ssid
                    fclose(wpafile);
                    return false;
                }
                else
                {
                    u8hits++;
                }
            }
            else if(strstr(line,"psk") != NULL)
            {
                //ssid line found. test
                if(strstr(line,_password) == NULL)
                {
                    //different password
                    fclose(wpafile);
                    return false;
                }
                else
                {
                    u8hits++;
                }
            }
        }
    }
    fclose(wpafile);
    return u8hits == 2;
}

void cNixieClock::SaveWpaFile(const char *_ssid, const char *_password, const char *_country)
{
    FILE *wpafile = fopen("/etc/wpa_supplicant/wpa_supplicant.conf","wb");
    if(wpafile == NULL)
        return;

    fprintf(wpafile,"ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n");
    fprintf(wpafile,"update_config=1\n");
    fprintf(wpafile,"country=%s\n", _country);
    fprintf(wpafile,"network={\n");
    fprintf(wpafile,"    ssid=\"%s\"\n", _ssid);
    fprintf(wpafile,"    psk=\"%s\"\n", _password);
    fprintf(wpafile,"}\n");
    fclose(wpafile);
}

void cNixieClock::FsmBootIni()
{
    //check for wlan config...
    tinyxml2::XMLElement *ewifi = mXmlNixie->FirstChildElement("wifi");
    if(ewifi != NULL)
    {
        tinyxml2::XMLElement *essid = ewifi->FirstChildElement("ssid");
        if(essid != NULL)
        {
            //see if is the same as in wpa_supplicant.conf
            if(!CheckWpaFile(essid->Attribute("name"), essid->Attribute("password")))
            {
                //different. set and reboot.
                SaveWpaFile(essid->Attribute("name"), essid->Attribute("password"), essid->Attribute("country"));
                system("reboot");
            }
        }
    }

    SetWeatherData();

    mTimerAux.SetTimeOut(1);

    ExecuteEvent(GetEventbyName("boot"));
    SetFsmState(eClockFsmStateBoot);
}

bool cNixieClock::CheckIp()
{
    system("ifconfig wlan0 >/tmp/test.txt");
    FILE *fTest = fopen("/tmp/test.txt", "rb");
    if(fTest != NULL)
    {
        while(!feof(fTest))
        {
            char line[255];
            if(fgets(line, 255, fTest) != NULL)
            {
                if(strstr(line, "inet ") != NULL)
                {
                    if(strstr(line, "broadcast ") != NULL)
                    {
                        fclose(fTest);
                        return true;
                    }
                }
            }
        }
        fclose(fTest);
    }
    return false;
}

__u32 cNixieClock::FsmBoot()
{
    if(mTimerAux.IsTimeOut())
    {
        if(CheckIp())
        {
            FsmLoopIni();   //FsmNtpIni();
        }
        else
        {
            mTimerAux.SetTimeOut(5000);
        }
    }
    return 10000;
}

void cNixieClock::SetWeatherData()
{
    if(!mcNixieWeather.SetConfig(mXmlNixie->FirstChildElement("weather")))
    {
        printf("Weather config failed!!\n");
    }
    else
        mcNixieWeather.Ask4Climate();
}

//void cNixieClock::FsmNtpIni()
//{
//    if(mcDS3231rtc != NULL)
//    {
//        char txt[32];
//        tm *chiptime = mcDS3231rtc->GetDateTime(txt);
//        int fd = open("/dev/rtc", O_RDONLY);
//        ioctl(fd, RTC_SET_TIME, chiptime);
//        close(fd);
//    }
//    mTimerAux.SetTimeOut(1);
//    mXmlElementNtpServer = NULL;
//    SetFsmState(eClockFsmStateNtp);
//}
//
//__u32 cNixieClock::FsmNtp()
//{
//    if(mTimerAux.IsTimeOut())
//    {
//        tinyxml2::XMLElement *xmltz = NULL;
//        if(mXmlElementNtpServer == NULL)
//        {
//            mXmlElementNtpServer = mXmlNixie->FirstChildElement("clock_settings");
//            if(mXmlElementNtpServer != NULL)
//            {
//                xmltz = mXmlElementNtpServer->FirstChildElement("timezone");
//                mXmlElementNtpServer = mXmlElementNtpServer->FirstChildElement("ntp");
//            }
//        }
//        else
//        {
//            mXmlElementNtpServer = mXmlElementNtpServer->NextSiblingElement("ntp");
//        }
//        if(mXmlElementNtpServer != NULL)
//        {
//            int gmtoffsecs = 0;
//            if(xmltz != NULL)
//            {
//                gmtoffsecs = xmltz->IntAttribute("value") * 3600;
//            }
//            cNtpClient ntp;
//
//            const char *ntpserver = mXmlElementNtpServer->Attribute("server");
//
//            mTimerAux.SetTimeOut(1000);
//
//            if(ntpserver != NULL)
//            {
//                const tm *now = ntp.GetTimeFrom(ntpserver, gmtoffsecs);
//                if(now != NULL)
//                {
////                    printf("we got a valid time from NTP. lets set the clock\n");
//                    int fd = open("/dev/rtc", O_RDONLY);
//                    ioctl(fd, RTC_SET_TIME, now);
//                    close(fd);
//                    //and the hardware clock too
//                    if(mcDS3231rtc != NULL)
//                        mcDS3231rtc->SetGmtDateTime();
//                    //setup is done. lets work.
//                    FsmLoopIni();
//                }
//            }
//        }
//    }
//    return 10000;
//}

void cNixieClock::FsmLoopIni()
{
    SetFsmState(eClockFsmStateLoop);
}

__u32 cNixieClock::FsmLoop()
{
    if(mpTm != NULL)
    {
        ProcessEvents();
        mpTm = NULL;
    }
    return 300000;
}

tinyxml2::XMLElement *cNixieClock::GetEventbyName(const char *_eventname)
{
    if(mXmlNixie != NULL)
    {
        tinyxml2::XMLElement *event = mXmlNixie->FirstChildElement("event");
        while(event != NULL)
        {
            if(event->Attribute("type", _eventname))
            {
                return event;
            }
            event = event->NextSiblingElement("event");
        }
    }
    return NULL;
}

void cNixieClock::ExecuteEvent(tinyxml2::XMLElement *_event)
{
    if(_event != NULL)
    {
        ProcessTubeEvent(_event->FirstChildElement("tube"));
        ProcessBarGraphEvent(_event->FirstChildElement("bargraph"));
        ProcessRgbEvent(_event->FirstChildElement("rgb"));
        ProcessIndicatorEvent(_event->FirstChildElement("indicator"));
        ProcessBrightnessEvent(_event->FirstChildElement("brightness"));

        if(_event->Attribute("lock"))
        {
            mDisplayLock = _event->IntAttribute("lock");
        }
        int t = _event->IntAttribute("delay");
        if(t)
        {
            mTimerAux.SetTimeOut(t*1000);
        }
    }
}


char *getIP(char *_ipchar)
{
    setenv("LANG","C",1);
    FILE * fp = popen("ifconfig", "r");
    if (fp)
    {
        char *p=NULL, *e;
        size_t n;
        while ((getline(&p, &n, fp) > 0) && p)
        {
            if (strstr(p, "wlan0:"))
            {
                while ((getline(&p, &n, fp) > 0) && p)
                {
                    if (p = strstr(p, "inet "))
                    {
                        p += 5;
                        e = strstr(p+1, " ");
                        if(e)   *e=0;
                        strcpy(_ipchar, p);
                        pclose(fp);
                        return _ipchar;
                    }
                }
            }
        }
    }
    else
    {
        printf("Error\n");
    }
    pclose(fp);
    return NULL;
}

void cNixieClock::ProcessTubeEvent(tinyxml2::XMLElement *_event)
{
    if(_event != NULL && mcNixieNumbers != NULL && !mDisplayLock)
    {
        if(_event->Attribute("action", "showtime"))
        {
            mpTxt = mTxtTime;
        }
        else if(_event->Attribute("action", "showdate"))
        {
            mpTxt = mTxtDate;
        }
        else if(_event->Attribute("action", "IP"))
        {
            char ipchar[32];
            strcpy(mTxtIp, "000000");
            if(getIP(ipchar) != NULL)
            {
                printf("IP: %s\n", ipchar);
                int a, b, c, d;
                if(sscanf(ipchar, "%d.%d.%d.%d", &a, &b, &c, &d) == 4)
                {
                    sprintf(mTxtIp, "   %03d", d);
                }
            }
            mpTxt = mTxtIp;
        }
        else if(_event->Attribute("action", "fixed"))
        {
            mcNixieNumbers->SetValue(_event->Attribute("action", "value"));
            return;
        }
        else    return;
        if(_event->Attribute("anim", "flip"))
        {
            mcNixieNumbers->SetValue(mpTxt, enumNumberAnimFlip, _event->IntAttribute("delay"));
        }
        else if(_event->Attribute("anim", "scroll"))
        {
            mcNixieNumbers->SetValue(mpTxt, enumNumberAnimScroll, _event->IntAttribute("delay"));
        }
        else if(!mDisplayLock)
        {
            mcNixieNumbers->SetValue(mpTxt, enumNumberAnimNone, 0);
        }
    }
}

void cNixieClock::ProcessBarGraphEvent(tinyxml2::XMLElement *_event)
{
    //printf("bargraph...(%s)\n", _event->Attribute("action"));
    if(_event != NULL && mNixieVu != NULL && !mDisplayLock)
    {
        if(_event->Attribute("modulation") != NULL)
        {
            //no real data, just an animation
            int iperiod = _event->IntAttribute("period");
            enuModulationType etype = get_modulation_type(_event);
            if(iperiod > 0)
            {
                mNixieVu->SetModulator(etype, NULL);
                mNixieVu->GetModulatorPtr()->SetPeriod(iperiod);
            }
        }
        else
        {
            mNixieVu->SetModulator(enuModulationTypeNone, NULL);
            if(_event->Attribute("action", "showtemperature"))
            {
                mDblCurrentTemperature = mcNixieWeather.GetCurrentTemperature();
                printf("Current temperature: %0.1fºC\n", mDblCurrentTemperature);
                mNixieVu->SetPercent(GetTemperaturePercent(mDblCurrentTemperature));
            }
            else if(_event->Attribute("action", "showweekday"))
            {
                printf("Weekday: %d\n", GetWeekPercent(mNewTm.tm_wday, mNewTm.tm_hour));
                mNixieVu->SetPercent(GetWeekPercent(mNewTm.tm_wday, mNewTm.tm_hour));
            }
        }
    }
}



void cNixieClock::ProcessRgbEvent(tinyxml2::XMLElement *_event)
{
//    printf("rgb...\n");
    int r=0, g=0, b=0, r1=0, g1=0, b1=0, t=0, t1=0;

    if(_event != NULL && mcNeoRpiRgbh != NULL && !mDisplayLock)
    {
        if(_event->Attribute("value", "rgbtemp"))
        {
//            printf("rgb...A\n");
            tinyxml2::XMLElement *eRgb = mXmlNixie->FirstChildElement("rgb");
            if(eRgb != NULL)
            {
//                printf("rgb...B\n");
                eRgb = eRgb->FirstChildElement("temperature");
                if(eRgb != NULL)
                {
//                    printf("rgb...C\n");
                    t = eRgb->IntAttribute("value");
//                    printf("rgb...D %d %s\n", eRgb->IntAttribute("value"), eRgb->Attribute("color"));
                    if(eRgb->Attribute("color"))
                        sscanf(eRgb->Attribute("color"), "%d,%d,%d", &r, &g, &b);
//                    printf("rgb...E\n");
                    eRgb->NextSiblingElement("temperature");
//                    printf("rgb...F\n");
                    if(eRgb != NULL)
                    {
//                        printf("rgb...G %d %s\n", eRgb->IntAttribute("value"), eRgb->Attribute("color"));
                        t1 = eRgb->IntAttribute("value");
                        if(eRgb->Attribute("color"))
                            sscanf(eRgb->Attribute("color"), "%d,%d,%d", &r1, &g1, &b1);
                        mDblCurrentTemperature = mcNixieWeather.GetCurrentTemperature();
                        r = getLinearInterpolator(t, t1, r, r1, mDblCurrentTemperature);
                        g = getLinearInterpolator(t, t1, g, g1, mDblCurrentTemperature);
                        b = getLinearInterpolator(t, t1, b, g1, mDblCurrentTemperature);
                        mcNeoRpiRgbh->SetRgb(r,g,b);
//                        printf("rgb...H\n");
                    }
                }
            }
        }
        else if(_event->Attribute("value") != NULL)
        {
//            printf("rgb...I\n");
            //fixed rgb value. just parse and set
            if(sscanf(_event->Attribute("value"), "%d,%d,%d", &r, &g, &b) == 3)
            {
                mcNeoRpiRgbh->SetRgb(r,g,b);
            }
            else
            {
                mcNeoRpiRgbh->SetRgb(0,0,0);
                mcNeoRpiRgbh->SetModulator(enuModulationTypeNone, NULL);
            }

            if(_event->Attribute("modulation"))
            {
                //printf("mod\n");
                enuModulationType etype = get_modulation_type(_event);
                int iperiod = _event->IntAttribute("period");

                if(etype != enuModulationTypeNone && iperiod > 0)
                {
                    //printf("mod set\n");
                    mcNeoRpiRgbh->SetModulator(etype, NULL);
                    mcNeoRpiRgbh->GetModulatorPtr()->SetPeriod(iperiod);
                }
            }
        }
    }
}

void cNixieClock::ProcessIndicatorEvent(tinyxml2::XMLElement *_event)
{
//    printf("indicator...(%d)\n", mDisplayLock);
    if(_event != NULL  && !mDisplayLock)
    {
        if(_event->Attribute("value", "on"))
        {
            if(mGpioHms)     mGpioHms->SetValue(PIN_HIGH);
            if(mGpioTemp)    mGpioTemp->SetValue(PIN_HIGH);
            if(mGpioWeekday) mGpioWeekday->SetValue(PIN_HIGH);
        }
        else if(_event->Attribute("value", "day"))
        {
            if(mGpioHms)     mGpioHms->SetValue(PIN_LOW);
            if(mGpioTemp)    mGpioTemp->SetValue(PIN_LOW);
            if(mGpioWeekday) mGpioWeekday->SetValue(PIN_HIGH);
        }
        else if(_event->Attribute("value", "temp"))
        {
            if(mGpioHms)     mGpioHms->SetValue(PIN_HIGH);
            if(mGpioTemp)    mGpioTemp->SetValue(PIN_HIGH);
            if(mGpioWeekday) mGpioWeekday->SetValue(PIN_LOW);
        }
        else
        {
            if(mGpioHms)     mGpioHms->SetValue(PIN_LOW);
            if(mGpioTemp)    mGpioTemp->SetValue(PIN_LOW);
            if(mGpioWeekday) mGpioWeekday->SetValue(PIN_LOW);
        }
    }
}

void cNixieClock::ProcessBrightnessEvent(tinyxml2::XMLElement *_event)
{
//    printf("bright...\n");
    if(_event != NULL)
    {
        if(_event->Attribute("value"))
        {
            int ivalue = _event->IntAttribute("value");
            if(ivalue > 100)
                ivalue = 100;
            if(ivalue < 0)
                ivalue = 0;
            mcNixieDimmer->SetPercent(ivalue);
        }
        else if(_event->Attribute("modulation"))
        {
            enuModulationType etype = get_modulation_type(_event);
            int iperiod = _event->IntAttribute("period");

            if(etype != enuModulationTypeNone && iperiod > 0)
            {
                mcNixieDimmer->SetModulator(etype, NULL);
                mcNixieDimmer->GetModulatorPtr()->SetPeriod(iperiod);
            }
        }
    }
}

void cNixieClock::UpdateDisplayTxt(tm *now)
{
    //printf("updatedisplay...\n");
    int hour = now->tm_hour;
    sprintf(mTxtDate,"%02d%02d%02d", max2digits(now->tm_mday), max2digits(now->tm_mon+1), max2digits(now->tm_year));
    if(mXmlNixie != NULL)
    {
        tinyxml2::XMLElement *esettings = mXmlNixie->FirstChildElement("clock_settings");
        if(esettings != NULL)
        {
            esettings = esettings->FirstChildElement("format");
            if(esettings != NULL)
            {
                if(esettings->IntAttribute("time") == 12 && hour > 12)
                    hour -= 12;
                if(strncmp(esettings->Attribute("date","ddmmaa"), "aammdd",6) == 0)
                {
                    //american format
                    sprintf(mTxtDate,"%02d%02d%02d", max2digits(now->tm_year), max2digits(now->tm_mon+1), max2digits(now->tm_mday));
                }
                if(strncmp(esettings->Attribute("date","ddmmaa"), "aaddmm",6) == 0)
                {
                    //weird format
                    sprintf(mTxtDate,"%02d%02d%02d", max2digits(now->tm_year), max2digits(now->tm_mday), max2digits(now->tm_mon+1));
                }
            }
        }
    }
    sprintf(mTxtTime,"%02d%02d%02d", max2digits(hour), max2digits(now->tm_min), max2digits(now->tm_sec));
}

bool cNixieClock::MatchConditionTime(tinyxml2::XMLElement *_event, tm *_now)
{
    if(_event == NULL || _now == NULL)
    {
        return false;
    }
//printf("MC A\n");
    if(_event->Attribute("type","timechange") == NULL)
        return false;
//printf("MC B\n");
    if(_event->Attribute("condition","hour") != NULL)
    {
//        printf("MC C\n");
        return (mOldTm.tm_hour !=_now->tm_hour);
    }
//    printf("MC D\n");
    if(_event->Attribute("condition","minute") != NULL)
    {
//        printf("MC E\n");
        return (mOldTm.tm_min !=_now->tm_min);
    }
//    printf("MC F\n");
    if(_event->Attribute("condition","second") != NULL)
        return true;
//printf("MC G\n");
    if(_event->Attribute("condition","mask") != NULL)
    {
//        printf("MC H\n");
        const char *mask = _event->Attribute("mask");
        char itime[7];
//        printf("MC I\n");
        sprintf(itime, "%02d%02d%02d", _now->tm_hour, _now->tm_min, _now->tm_sec);
        __u8 u8hits = 0;
//        printf("MC J\n");
        for(__u8 i=0; i<6 && mask; i++)
        {
            if(mask[i] == 'x' || mask[i] == itime[i])
                u8hits++;
        }
//        printf("MC K\n");
        return (u8hits == 6);
    }
//    printf("MC L\n");
    return false;
}


tinyxml2::XMLElement *cNixieClock::FindNewEvent(tm *_now)
{
    tinyxml2::XMLElement *event = mXmlNixie->FirstChildElement("event");
    //printf("FindNewEvent...\n");
    while(event != NULL)
    {
        if(MatchConditionTime(event, _now))
        {
//            printf("Event found '%s' - %s\n", event->Attribute("type"), event->Attribute("condition"));
            return event;
        }

        event = event->NextSiblingElement("event");
    }
    return NULL;
}

void cNixieClock::ProcessEvents()
{
    if(mpTm != NULL)
    {
        tm *now = localtime(mpTm);
        UpdateDisplayTxt(now);
        mNewTm = *now;

        tinyxml2::XMLElement *event = FindNewEvent(now);
        if(event != NULL)
        {
            ExecuteEvent(event);
        }

        mOldTm = *now;
        mpTm = NULL;
    }
}

void cNixieClock::SetFaceDetected(__u32 _u32timeoutms)
{
    printf("Set face detect timeout %d\n", _u32timeoutms);
    tinyxml2::XMLElement *aux = mXmlNixie->FirstChildElement("face_detection");
    if(aux)
    {
//        printf("Face ON\n");
        ExecuteEvent(aux->FirstChildElement("on"));
    }
//    if(mcNixieDimmer)
//        mcNixieDimmer->SetPercent(100);
    mTimerFaceDetect.SetTimeOut(_u32timeoutms);

    if(mNixieVu != NULL && mcNixieDimmer != NULL && mcNixieDimmer->GetPercent() == 0)
    {
        mNixieVu->GetModulatorPtr()->RampTo(100.0);
        //mNixieVu->SetPercent(0);
    }
}

//eof nixie_clock.cpp
