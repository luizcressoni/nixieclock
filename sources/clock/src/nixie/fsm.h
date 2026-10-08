/*!\ file fs.h */
#pragma once
#include "hardware/hardware.h"
#include "weather.h"
#include "../utils/defines.h"

/*! \class cNixieFsm
    \brief Finite State Machine for Nixie clock
    \details This class implements a finite state machine to manage the states of the Nixie clock.
    It handles transitions between states such as initialization, network connection, sleeping, awake,
    error handling, hotspot mode, temporary states, and rebooting.
*/
class cNixieFsm
{

protected:
    cNixieHardware  *mcNixieHardware = nullptr;
    cWeatherReport  *mcWeatherReport = nullptr;
    enum enumFsmState
    {
        STATE_INIT,                 //just after boot
        STATE_NETWORK,              //waiting for network    
        STATE_SLEEPING,             //just idling
        STATE_AWAKE,                //up
        STATE_ERROR,                //error state
        STATE_HOTSPOT,              //hotspot mode
        STATE_TEMPORARY,            //temporary state
        STATE_REBOOT,               //rebooting
        STATE_REGEN                 //cathode regeneration ("slot machine") on a single tube
    };
    enumFsmState m_state = STATE_INIT;
    enumFsmState m_previousState = STATE_INIT;
    enum enumDisplayMode
    {
        enumDisplayModeTime,
        enumDisplayModeDate,
        enumDisplayModeTempLimit,
        enumDisplayModeCloud
    };
    enumDisplayMode menumDisplayMode;
    const char *get_display_mode_name(enumDisplayMode _mode) const;
    bool CycleDisplayMode(bool _forward);

    /*! \brief Which of the two regeneration routines STATE_REGEN is currently running */
    enum enumRegenMode
    {
        enumRegenModeManual,    //!< repair: one tube, overdriven, until the web page says stop
        enumRegenModeNight      //!< prevention: all tubes, normal brightness, a counted dose
    };

    uint32_t m_seconds = 0;
    uint32_t m_seconds_on = 0;
    uint32_t m_seconds_lock = 0;
    uint32_t m_awake_seconds = 0;       //time lit since the clock last woke, see AWAKE_TOUR_AFTER_SECONDS
    bool m_touring = false;             //showing every mode once before going to sleep
    uint8_t m_ipBytes[4] = {0, 0, 0, 0};
    uint32_t m_hotspot_seconds = 0;     //time spent in hotspot mode since entering it
    uint32_t m_motion_blank = 0;        //seconds undirected motion is ignored for, see MOTION_BLANK_SECONDS
    uint32_t m_network_retry_wait = NETWORK_RETRY_FIRST_SECONDS;   //see NETWORK_RETRY_*
    uint8_t m_testvalue = 9;
    //whether the ambient light has the tubes dimmed right now. A member rather than a local
    //static so the paths that reset the brightness themselves can clear it, instead of leaving
    //the clock convinced it is still dimmed when it no longer is.
    bool m_dimmed = false;
    uint8_t m_regen_position = REGEN_TUBE_DEFAULT - 1;  //internal index, 0 being the leftmost tube
    uint32_t m_regen_ticks = 0;         //ticks elapsed on the digit being shown
    uint8_t  m_regen_digit = 0;         //digit currently lit, 0 to 9
    enumRegenMode m_regen_mode = enumRegenModeManual;
    uint32_t m_regen_cycles = 0;        //full 0..9 sweeps still owed in a night session
    int  m_regen_last_minute = -1;      //minute of day and day of the last automatic session, so
    int  m_regen_last_yday = -1;        //the trigger window cannot fire the same session twice
    //bool m_showdate = false;


    const char *GetStateName(enumFsmState _state) const;
    void SetState(enumFsmState _state);
    enumFsmState GetState() const;
    void SetDisplay(enumDisplayMode _mode) const;
    void SetStateInit();
    void SetStateNetwork();
    void SetStateSleeping();
    void SetStateAwake(uint32_t _seconds_on, enumDisplayMode _mode);
    void SetStateHotspot();
    void SetStateTemporary(uint32_t _seconds_on);
    void SetStateError(int _action);
    void SetStateReboot();
    void RetryNetwork();
    void SetStateRegen(uint8_t _position);
    void SetStateRegenNight();
    void StopStateRegen();

    bool IsDaytimeHour(int _hour) const;
    bool CheckNightRegen();
    void RefreshAstro() const;

    bool ProcessStateInit(int _action);
    bool ProcessStateNetwork(int _action);
    bool ProcessStateSleeping(int _action);
    bool ProcessStateAwake(int _action);
    bool ProcessStateError(int _action) const;
    bool ProcessStateDefault(int _action);
    bool ProcessStateHotspot(int _action);
    bool ProcessStateTemporary(int _action);
    bool ProcessStateReboot(int _action) const;
    bool ProcessStateRegen(int _action);

    void ProcessCgiSignals(int _action);

    tm   mTmTime;
    void UpdateNumbers();
    bool PreprocessAction(int _action);
public:
    cNixieFsm();
    ~cNixieFsm();


    bool ProcessAction(int _action);

};


extern cNixieFsm *gNixieFsm;
//eof fsm.h