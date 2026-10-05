/*! \file modulation.h */
#pragma once
#include <cstdint>

enum enuModulationType
{
    enuModulationTypeRamp,
    enuModulationTypeRampOneShotDown,
    enuModulationTypeRampOneShotUp,
    enuModulationTypeSinusoidal,
    enuModulationTypeFlash,
    enuModulationTypeNone
};

/*! \brief Creates a modulation object for the PWM controller */
class cModulation
{
  protected:
    uint32_t   mu32resettime{};
    double  delta{}, b{}, w{}, mdblperiodMs{};
    double  mdblvalue, mdbltarget{};
    bool    rampUp{};
    uint32_t u32get_ms();
  public:
    cModulation();
    virtual ~cModulation();
    virtual void    Reset();
    virtual void    SetPeriod(uint32_t _u32periodMs);
    virtual void    RampTo(double _value);
    virtual double  GetValue();
};

/*! \brief Creates a RAMP modulation object for the PWM controller
  \note a Ramp is a linear interpolation
*/
class cRamp : public cModulation
{
 public:
    void    RampTo(double _value) override;
    double  GetValue() override;
};

/*! \brief Creates a Sinusoidal modulation object for the PWM controller
  \note This is a cyclic modulation that oscillates between 0 and 1
  \note The period is set in milliseconds and defines how long it takes to complete one full cycle
*/
class cSinusoidal : public cModulation
{
 protected:

 public:
    void    SetPeriod(uint32_t _u32periodMs) override;
    double  GetValue() override;
};


/*! \brief Creates a Flash modulation object for the PWM controller
  \note This is a binary modulation that toggles between 0 and 1
  \note The period is set in milliseconds and defines how long it takes to complete one full cycle
*/
class cFlash : public cModulation
{
 public:
    double  GetValue() override;
};

//eof modulation.h

