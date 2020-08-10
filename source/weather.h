/*! \file weather.h */
#pragma once
#include <pthread.h>
#include <tinyxml2.h>
#include <linux/types.h>

#define QUERYBUFFERSIZE     256
#define REPLYBUFFERSIZE     100000
class cNixieWeather
{
protected:
    pthread_t                           pth_Thread;
    char                                mStrQuery[QUERYBUFFERSIZE];
    virtual bool                        ProcessReply(tinyxml2::XMLDocument *_doc);
    double                              mdblCurrentTemp;
    __u32                               mu32threadsleep;
public:
    cNixieWeather(void);
    virtual ~cNixieWeather(void);

    virtual bool                        SetConfig(double _latit, double _longit, const char *_appid, const char *_units);
    virtual bool                        SetConfig(tinyxml2::XMLElement *_configWeather);
    virtual double                      GetCurrentTemperature(){return mdblCurrentTemp;};
    virtual __u32                       Ask4Climate();
};


//eof weather.h
