/*! \file functions.h */
#pragma once

#include <linux/types.h>



__u8 GetWeekPercent(__u8 _u8weekday, __u8 _u8hour);
__u8 GetTemperaturePercent(double _temperature);
__u32 u32get_ms();
double getLinearInterpolator(double xi, double xf, double yi, double yf, double _x);
__u8 max2digits(int _x);
//int Sha1Sum(const char *_fileName, char *_hashOut, int _sizeOfHashOut, char *_txtHashOut, int _sizeOfTxtHashOut, const char *_salt, int _skipBytes);


/** threads **/
void *gDimmerThread(void *_data);
void *gVuThread(void *_data);


//eof functions.h
