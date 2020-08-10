/*! \file functions.cpp */

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "functions.h"

#include "pwm.h"

/**
    Just some usefull functions
**/


/*! given the day of the week and the hour,
    return how much of the week is done in percentage,
    to set the VU value for the weekday.
    Sunday = 0 */
__u8 GetWeekPercent(__u8 _u8weekday, __u8 _u8hour)
{
    double totalhours = _u8weekday * 24 + _u8hour;
    totalhours /= 1.68;
    return totalhours;
}

/*! Given a temperature value from zero to 45ºC,
    return a percentage to use
    on the VU display
*/
__u8 GetTemperaturePercent(double _temperature)
{
    if(_temperature > 45.0) return 100;
    if(_temperature < 0.0)  return 0;

    _temperature *= 100.0;
    _temperature /= 45.0;   //max temp on display
    return (__u8)_temperature;
}


__u32 u32get_ms()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}


double getLinearInterpolator(double xi, double xf, double yi, double yf, double _x)
{
    double delta = (yf - yi) / (xf - xi);
    double b = yf - delta * xf;
    return _x * delta + b;
}

__u8 max2digits(int _x)
{
    return _x%100;
}


/******** threads **************/

void *gDimmerThread(void *_data)
{
    cNixiePwm *aux = (cNixiePwm *)_data;
    while(aux->Task());
    return NULL;
}

void *gVuThread(void *_data)
{
    cNixiePwm *aux = (cNixiePwm *)_data;
    while(aux->Task());
    return NULL;
}

//eof functions.cpp
