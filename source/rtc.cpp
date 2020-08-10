/*! \file rtc.cpp */
#include "rtc.h"

//#include <neolibs/neolog-2.5.h>
//#include <neolibs/neoconfigxml-2.5.h>

#include <stdio.h>
#include <string.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define DS1307_OFFSET       0x08
#define DS1307_MAXRAM       52


#define DS3232_OFFSET       0x14
#define DS3232_MAXRAM       232


#define DS3232_CONTROL_REGISTER     0x0e
#define DS3232_EOSC                 0x80
#define DS3232_BBSQW                0x40
#define DS3232_CONV                 0x20
#define DS3232_INTCN                0x04
#define DS3232_A2IE                 0x02
#define DS3232_A1IE                 0x01

#define DS3232_STATUS_REGISTER      0x0f
#define DS3232_OSF                  0x80
#define DS3232_BB32KHZ              0x40
#define DS3232_EN32KHZ              0x08
#define DS3232_BSY                  0x04
#define DS3232_A2F                  0x02
#define DS3232_A1F                  0x01


pthread_mutex_t gmutexRtcCrc = PTHREAD_MUTEX_INITIALIZER;

cDS3231rtc::cDS3231rtc( __u8 *_chiptype, __u8 _i2cbus, __u8 _address)
{
    char mbus[32];
    mu8_Bus = _i2cbus;
    mu8_Address = _address;

    mu8offset = DS1307_OFFSET;
    mu8maxRam = DS1307_MAXRAM;

    sprintf(mbus, "/dev/i2c-%d", mu8_Bus);
    mcNeoI2c = new cNeoI2c(mbus, mu8_Address);


    /** RTC chip detection ******************
    Address 0x07 on the DS1307 is the CONTROL register.
    Bits 2, 3, 5 and 6 are not defined and read as zero
    On the DS3232M, address 7 is ALARM 1 seconds register - all bits available.
    So, I write 0x6c (0b01101100) and read it back.
    If it returns zero, it is the DS1307. Otherwise, it is the DS3232M
    *****************************************/

    __u8 u8chiptype = 0x6c;
    mcNeoI2c->send(0x07, &u8chiptype, 1);
    mcNeoI2c->receive(0x07, &u8chiptype, 1);

    if(u8chiptype == 0x6c)
    {
        mu8offset = DS3232_OFFSET;
        mu8maxRam = DS3232_MAXRAM;
    }

    if(_chiptype != NULL)
        *_chiptype = u8chiptype;


    if(VerifyRam() == eRtcStatus_OK)
    {
        char buffer[32];
        tm *tmnow = GetDateTime(buffer);
////        SYSLOGPRINTF(LOG_TYPE_TERMINAL,"cDS3231rtc","Date & time: %s", buffer);
        if(tmnow != NULL)
        {
            time_t time_recebido = mktime(tmnow);
            stime(&time_recebido);
        }
    }

    if(mu8offset == DS3232_OFFSET)
    {
        __u8 u8data = (DS3232_INTCN);
        mcNeoI2c->send(DS3232_CONTROL_REGISTER, &u8data, 1);   //Control register

        u8data = 0;
        mcNeoI2c->send(0x0f, &u8data, 1);   //control / status register
    }
}


cDS3231rtc::~cDS3231rtc(void)
{
    delete mcNeoI2c;
}

int cDS3231rtc::Read(void *_data, __u8 _u8startAddress, __u8 _size)
{
    if(_size > mu8maxRam) return -1;
    __u8 u8tries = 0;
    int retorno = -1;
    while(retorno < 0 && u8tries++ < 100)
    {
        retorno = mcNeoI2c->receive(mu8offset + _u8startAddress, _data, _size);
        if(retorno < 0)
            usleep(1000);
    }
    return retorno;
}

int cDS3231rtc::Write(void *_data, __u8 _u8startAddress, __u8 _size)
{
    __u8 u8tries = 0;
    int retorno = -1;
    while(retorno < 0 && u8tries++ < 100)
    {
        retorno = mcNeoI2c->send(mu8offset + _u8startAddress, _data, _size);
        if(retorno < 0)
            usleep(1000);
    }
    return retorno;
}

int cDS3231rtc::RamRead(void *_data, __u8 _u8startAddress, __u8 _size)
{
    return Read(_data, _u8startAddress, _size);
}

int cDS3231rtc::RamWrite(void *_data, __u8 _u8startAddress, __u8 _size)
{
    pthread_mutex_lock(&gmutexRtcCrc);
    if(Write(_data, _u8startAddress, _size) == -1)
    {
        pthread_mutex_unlock(&gmutexRtcCrc);
        return -1;
    }
    pthread_mutex_unlock(&gmutexRtcCrc);
    return 1;
}

int cDS3231rtc::ClearRam()
{
    __u8 *u8bufer = new __u8[mu8maxRam];
    bzero(u8bufer, mu8maxRam);
    if(RamWrite(u8bufer, 0, mu8maxRam) == -1) return -1;
    VerifyRam();
    return 1;
}

eRtcStatus cDS3231rtc::VerifyRam()
{
    __u32 u32crc = 0;
    pthread_mutex_lock(&gmutexRtcCrc);
    if(RamRead(&u32crc, 0, 4) == -1)    meRtcStatus = eRtcStatus_NoChip;
    else                                meRtcStatus = eRtcStatus_OK;
    pthread_mutex_unlock(&gmutexRtcCrc);
    return meRtcStatus;
}

//const char *cDS3231rtc::GetErrorMessage()
//{
//    return mcNeoI2c->getErrorMessage();
//}

eRtcStatus cDS3231rtc::GetRamStatus()
{
    return VerifyRam();
}

tm *cDS3231rtc::GetDateTime(char *_buffer)
{
    if(_buffer == NULL) return NULL;
    char datetime[7];
    if(mcNeoI2c->receive(0x00, datetime, 7) != 1)
    {
        strcpy(_buffer, "00/00/00 00:00:00");
        return NULL;
    }
    int day, month, year, hour, min, sec;
    sec = u8bcd2bin(datetime[0]);
    min = u8bcd2bin(datetime[1]);
    if(datetime[2] & 0b01000000)
    {
        //24h
        hour = 10 * ((datetime[2] >> 4) & 0x03) + (datetime[2] & 0x0f);
    }
    else
    {
        //12h
        hour = 10 * ((datetime[2] >> 4) & 0x01) + (datetime[2] & 0x0f);
        hour += (datetime[2] & 0b00100000)?12:0;
    }
    day = 10 * ((datetime[4] >> 4) & 0x03) + (datetime[4] & 0x0f);
    month = 10 * ((datetime[5] >> 4) & 0x01) + (datetime[5] & 0x0f);
    year = 10 * (datetime[6] >> 4) + (datetime[6] & 0x0f);

    sprintf(_buffer, "%02d/%02d/20%02d %02d:%02d:%02d", month, day, year, hour, min, sec);

    tm_gmt.tm_gmtoff = 0;
    tm_gmt.tm_hour = hour;
    tm_gmt.tm_isdst = 0;
    tm_gmt.tm_mday = day;
    tm_gmt.tm_min = min;
    tm_gmt.tm_mon = month - 1;
    tm_gmt.tm_sec = sec;
    tm_gmt.tm_wday = datetime[3] - 1;
    tm_gmt.tm_yday = 0;
    tm_gmt.tm_year = year + 100;
    tm_gmt.tm_zone = NULL;
    return &tm_gmt;
}

__u8 cDS3231rtc::u8bin2bcd(__u8 _u8value)
{
    __u8 u8retorno = 0;
    __u8 u8dezenas = _u8value / 10;
    __u8 u8unidades = _u8value % 10;
    u8retorno = (u8dezenas << 4) + u8unidades;
    return u8retorno;
}

__u8 cDS3231rtc::u8bcd2bin(__u8 _u8value)
{
    return 10 * ((_u8value >> 4) & 0x07) + (_u8value & 0x0f);
}

int cDS3231rtc::SetGmtDateTime(tm *_tm)
{
    __u8 u8datetime[7];
    if(_tm == NULL)
    {
        time_t gmt_t;
        time(&gmt_t);
        _tm = gmtime(&gmt_t);
    }
    u8datetime[0] = u8bin2bcd(_tm->tm_sec);
    u8datetime[1] = u8bin2bcd(_tm->tm_min);
    u8datetime[2] = u8bin2bcd(_tm->tm_hour);
    u8datetime[3] = _tm->tm_wday + 1;
    u8datetime[4] = u8bin2bcd(_tm->tm_mday);
    u8datetime[5] = u8bin2bcd(_tm->tm_mon) + 1;
    u8datetime[6] = u8bin2bcd(_tm->tm_year - 100);

    //u8datetime[7] = 0x00;
    return mcNeoI2c->send(0x00, u8datetime, 7);
}

double cDS3231rtc::GetChipTemperature()
{
    if(mu8offset == DS1307_OFFSET)
        return 0.0;

    __u8 u8data[2];
    if(mcNeoI2c->receive(0x11, u8data, 2) != 1)
        return 0.0;

    double retorno;
    retorno = u8data[0];
    retorno += 0.25 * (u8data[1] >> 6);
    return retorno;
}

//eof rtc.cpp

