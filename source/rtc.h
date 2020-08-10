/*! \file rtc.h */
#pragma once

#include <time.h>

#include "i2c.h"

enum eRtcStatus
{
    eRtcStatus_OK,
    eRtcStatus_NoChip
};

class cDS3231rtc
{
protected:
    tm              tm_gmt;
    eRtcStatus      meRtcStatus;
    cNeoI2c         *mcNeoI2c;
    __u8            mu8_Bus;
    __u8            mu8_Address;
    __u8            mu8offset;
    __u8            mu8maxRam;

    virtual __u8        u8bin2bcd(__u8 _u8value);
    virtual __u8        u8bcd2bin(__u8 _u8value);
    virtual int         Read(void *_data, __u8 _u8startAddress, __u8 _size);
    virtual int         Write(void *_data, __u8 _u8startAddress, __u8 _size);
public:
    cDS3231rtc(__u8 *_chiptype = NULL, __u8 _i2cbus=1, __u8 _address=0x68);
    virtual ~cDS3231rtc(void);

    virtual eRtcStatus  GetRamStatus();
    virtual int         RamRead(void *_data, __u8 _u8startAddress, __u8 _size);
    virtual int         RamWrite(void *_data, __u8 _u8startAddress, __u8 _size);

    virtual int         ClearRam();
    virtual eRtcStatus  VerifyRam();

    virtual tm          *GetDateTime(char *_buffer);
    virtual int         SetGmtDateTime(tm *_tm = NULL);
    virtual double      GetChipTemperature();
};

//eof rtc.h

