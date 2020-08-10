/*! \file sinusoidal.h */
#pragma once
#include <linux/types.h>

class cSinusoidal
{
 protected:
    __u32 mu32resettime;
    double w;
 public:
    cSinusoidal();
    void Reset();
    void SetPeriod(__u32 _u32periodMs);
    double GetValue();
};

//eof sinusoidal.h
