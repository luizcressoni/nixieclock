/*! \file weather.cpp */
#include "weather.h"
#include "clientsocket.h"
#include <stdio.h>

void *ThreadNexWeather(void *_cNixieWeather)
{
    if (_cNixieWeather != NULL)
    {
        __u32 sleepseconds = 1;
        do
        {
            sleep(sleepseconds);
            sleepseconds = ((cNixieWeather *)_cNixieWeather)->Ask4Climate();
        }while(sleepseconds > 0);
    }
    printf("quiting weather thread\n");
    return NULL;
}


bool cNixieWeather::ProcessReply(tinyxml2::XMLDocument *_doc)
{
    if (_doc == NULL)
        return false;

    tinyxml2::XMLHandle doc(_doc);
    tinyxml2::XMLElement *eForecast = doc.FirstChildElement("weatherdata").FirstChildElement("forecast").FirstChildElement("time").FirstChildElement("temperature").ToElement();

    if(eForecast != NULL)
    {
        mdblCurrentTemp = eForecast->DoubleAttribute("value");
        return true;
    }
    return false;
}


cNixieWeather::cNixieWeather()
{
    bzero(mStrQuery, 256);
    mu32threadsleep = 60;
    if (pthread_create(&pth_Thread, NULL, ThreadNexWeather, this) == 0)
    {
        mu32threadsleep = 0;
    }
}

cNixieWeather::~cNixieWeather()
{
    if(mu32threadsleep != 0)
    {
        mu32threadsleep = 0;
        pthread_join(pth_Thread, NULL);
    }
}

bool cNixieWeather::SetConfig(tinyxml2::XMLElement *_configWeather)
{
    if(_configWeather == NULL)
        return false;

    const char *appId = _configWeather->Attribute("appid");
        if(appId == NULL)
            return false;

    const char *units = _configWeather->Attribute("units");
    if(units == NULL)
        return false;

    mu32threadsleep = _configWeather->IntAttribute("updateIntervalMinutes") * 60;   //in minutes to seconds

    double latit = 0.0, longit = 0.0;

    _configWeather = _configWeather->FirstChildElement("location");

    if(_configWeather == NULL)
        return false;

    latit = _configWeather->DoubleAttribute("lat");
    longit = _configWeather->DoubleAttribute("long");

    return(SetConfig(latit, longit, appId, units));
}

bool cNixieWeather::SetConfig(double _latit, double _longit, const char *_appid, const char *_units)
{
    return (snprintf(mStrQuery, QUERYBUFFERSIZE, "GET /data/2.5/forecast?lat=%.2f&lon=%.2f&APPID=%s&mode=xml&units=%s\n\n", _latit, _longit, _appid, _units) < QUERYBUFFERSIZE);
}

__u32 cNixieWeather::Ask4Climate()
{
    if(strlen(mStrQuery) == 0)
    {
        return 0;
    }

    char *mstrBigBuffer = new char[REPLYBUFFERSIZE];
    if(mstrBigBuffer == NULL)
    {
        return 0;
    }

    getIPAddress("api.openweathermap.org", mstrBigBuffer);
    struct ClientSocket *socket = socket_create(mstrBigBuffer, 80);
    if (socket_connect(socket) != 0)
    {
        socket_destroy(socket);
        delete []mstrBigBuffer;
        return 0;
    }

    socket_send(socket, mStrQuery, strlen(mStrQuery));

    if (socket_receive(socket, mstrBigBuffer, REPLYBUFFERSIZE) <= 0)
    {
        socket_destroy(socket);
        delete []mstrBigBuffer;
        return 0;
    }
    socket_destroy(socket);

    int ret;
    tinyxml2::XMLDocument doc;
    FILE *arquivo = fopen("/tmp/weather.xml", "wb");
    if(arquivo != NULL)
    {
        fprintf(arquivo, "%s", mstrBigBuffer);
        fclose(arquivo);
    }

    if ((ret = doc.Parse(mstrBigBuffer)) == tinyxml2::XML_SUCCESS)
    {
        delete []mstrBigBuffer;
        return ProcessReply(&doc)?mu32threadsleep:0;
    }
    delete []mstrBigBuffer;
    return 0;
}

//eof weather.cpp
