/*! \file fsm.cpp */
#include "fsm.h"
#include "../utils/defines.h"
#include "../utils/structs.h"
#include "../logger/logger.h"
#include "config.h"
#include "hardware/hardware.h"
#include "wifi.h"
#include "actions.h"
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <sys/reboot.h>
#include <linux/reboot.h>

namespace fs = std::filesystem;

/******************************************************************************************
 * 
 *     INIT --> NETWORK
 * 
 *    NETWORK --> WIFI: SLEEPING
 *            --> HOTSPOT: 
 * 
 *     SLEEPING --> AWAKE at xx:59:58
 * 
 * 
 * 
 ******************************************************************************************/

cNixieFsm *gNixieFsm = nullptr;

/*! \brief Constructor
    \note Initializes the Nixie hardware and weather report objects.
    \note Sets the initial state to STATE_INIT.
    \note Registers the global pointer gNixieFsm to this instance.
    \note Initializes the weather report with the location from gLocation.
*/
cNixieFsm::cNixieFsm()
{
    gNixieFsm = this;
    mcNixieHardware = new cNixieHardware();
    mcWeatherReport = new cWeatherReport(gLocation.latitude, gLocation.longitude, gLocation.apikey);
    SetStateInit();
}

/*! \brief Destructor
    \note Cleans up the Nixie hardware and weather report objects.
    \note Sets the global pointer gNixieFsm to nullptr.
    \note Deletes the mcNixieHardware and mcWeatherReport objects.
*/
cNixieFsm::~cNixieFsm()
{
    delete mcNixieHardware;
    delete mcWeatherReport;
    mcNixieHardware = nullptr;
    gNixieFsm = nullptr;
}

/*! \brief Process signals that came from the www cgi
    \param _action The action signal received from the CGI.
    \note This function processes various CGI signals such as reboot, exit, WiFi configuration, VU min/max settings, reload, and brightness settings.
    \note Depending on the action, it may set the state to reboot or temporary, or save WiFi configuration.
    \note It also updates the VU min/max settings and brightness.
*/
void cNixieFsm::ProcessCgiSignals(int _action)
{
    if((_action & SIG_CGI_REGEN_MASK) == SIG_CGI_REGEN_ON)
    {
        //the low nibble carries the tube the user picked on the web page, 1 being the leftmost
        uint8_t tube = _action & 0x000f;
        if(tube < REGEN_TUBE_MIN || tube > REGEN_TUBE_MAX)
            tube = REGEN_TUBE_DEFAULT;
        SetStateRegen(tube - 1);
        return;
    }

    switch(_action)
    {
        case SIG_CGI_REBOOT:
            SetStateReboot();
            break;;
        case SIG_CGI_EXIT:
            add_action(SIG_CGI_EXIT);
            break;;
        case SIG_CGI_WIFI:
            if(save_wifiConfig(gWifiConfig.ssid, gWifiConfig.password))
                SetStateReboot();
            else
                LOGGER_ERROR("NixieFsm: wifi config refused, wpa_supplicant.conf left alone");
            break;
        case SIG_CGI_VU_MIN:
            mcNixieHardware->SetVuMin(gNixieConfig.vu_min);
            SetStateTemporary(10);
            break;
        case SIG_CGI_VU_MAX:
            mcNixieHardware->SetVuMax(gNixieConfig.vu_max);
            SetStateTemporary(10);
            break;
        case SIG_CGI_RELOAD:
            //main() already reloaded the config, but the weather report holds its own copy
            if(mcWeatherReport->SetLocation(gLocation.latitude, gLocation.longitude, gLocation.apikey)
               && getNetworkStatus() == NETWORK_STATUS_WIFI)
            {
                mcWeatherReport->UpdateForecast();
                RefreshAstro();
            }
            break;
        case SIG_CGI_BRIGHNESS:
            mcNixieHardware->SetMaxBrightness(gNixieConfig.brightness, false);
            //we just forced the configured level, so the ambient light is free to dim again
            m_dimmed = false;
            SetStateTemporary(10);
            break;
        case SIG_CGI_REGEN_OFF:
            StopStateRegen();
            break;
    }
}

/*! \brief Process the action signal before the FSM processing
    \param _action The action signal to be processed.
    \return Returns true we're god to go, false otherwise.
*/
bool cNixieFsm::PreprocessAction(int _action)
{
    if(_action & (SIG_ERROR | SIG_CONFIG))
    {
        //TODO handle error
        glogger->debug("NixieFsm: Error action received: 0x{:04x}", _action);
        SetStateError(_action);
        return false;
    }
    else if(_action & SIG_CGI)
    {
        ProcessCgiSignals(_action);
        return false;
    }
    if(_action == SIG_TIME_CHANGED)
    {
        UpdateNumbers();
        m_seconds++;
        if(m_seconds_on)
            m_seconds_on--;
    }
    if(_action & SIG_BRIGHTNESS && GetState() != STATE_REGEN)
    {
        //Either regeneration routine owns the brightness while it runs, so ambient light is
        //ignored: the manual one is holding full current on purpose, and the night one is
        //running at the configured level and would be dimmed to thirty percent otherwise,
        //which is the room being dark at three in the morning rather than a reason to back off.
        const uint32_t brightness = _action & 0x00ff;
        if(brightness <= BRIGHTNESS_DIM_BELOW && !m_dimmed)
        {
            glogger->debug("NixieFsm: Ambient light at {:d}, dimming the tubes", brightness);
            mcNixieHardware->SetMaxBrightness(BRIGHTNESS_DIM_PERCENT);
            m_dimmed = true;
        }
        else if(brightness >= BRIGHTNESS_RESTORE_ABOVE && m_dimmed)
        {
            glogger->debug("NixieFsm: Ambient light at {:d}, back to the configured brightness",
                           brightness);
            mcNixieHardware->SetMaxBrightness(gNixieConfig.brightness);
            m_dimmed = false;
        }
    }
    return true;
}

/*! \brief Processes the actions received by the main task 
    \return Returns true if the action was processed successfully, false otherwise.
    \note Apparently, we're not caring about the return value anyway.     
*/
bool cNixieFsm::ProcessAction(int _action)
{
    if(PreprocessAction(_action))
    {       
        switch(GetState())
        {
            case STATE_INIT:      return ProcessStateInit(_action);
            case STATE_NETWORK:   return ProcessStateNetwork(_action);
            case STATE_SLEEPING:  return ProcessStateSleeping(_action);
            case STATE_AWAKE:     return ProcessStateAwake(_action);
            case STATE_ERROR:     return ProcessStateError(_action);
            case STATE_HOTSPOT:   return ProcessStateHotspot(_action);
            case STATE_TEMPORARY: return ProcessStateTemporary(_action);
            case STATE_REBOOT:    return ProcessStateReboot(_action);
            case STATE_REGEN:     return ProcessStateRegen(_action);
            default:              return ProcessStateDefault(_action);
        }
    }
    return false;
}

/*! \brief Get the name of the current state
    \param _state The state to get the name for.
    \return Returns a string representation of the state name.
    \note This function maps the enumFsmState values to their corresponding string names.
*/
const char *cNixieFsm::GetStateName(enumFsmState _state) const {
    switch(_state)
    {
        case STATE_INIT:      return "STATE_INIT";
        case STATE_NETWORK:   return "STATE_NETWORK";
        case STATE_SLEEPING:  return "STATE_SLEEPING";
        case STATE_AWAKE:     return "STATE_AWAKE";
        case STATE_ERROR:     return "STATE_ERROR";
        case STATE_HOTSPOT:   return "STATE_HOTSPOT";
        case STATE_TEMPORARY: return "STATE_TEMPORARY";
        case STATE_REBOOT:    return "STATE_REBOOT";
        case STATE_REGEN:     return "STATE_REGEN";
        default:              return "Unknown";
    }
}

/*! \brief Set the current state of the FSM
    \param _state The new state to set.
    \note This function updates the current state and resets the seconds counter.
*/
void cNixieFsm::SetState(enumFsmState _state)
{
    m_state = _state;
    glogger->debug("NixieFsm: State set to {:s}", GetStateName(m_state));
    m_seconds = 0;
}

/*! \brief Gets the FSM mode */
cNixieFsm::enumFsmState cNixieFsm::GetState() const {
    return m_state;
}

void cNixieFsm::SetStateError(int _action)
{
    mcNixieHardware->SetDimmerPercent(100);
    mcNixieHardware->SetRgb(255, 0, 0);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationType::enuModulationTypeSinusoidal, 500);
    //error indicators
    switch(_action){
        case SIG_ERROR + SIG_CONFIG + SIG_FACE_DETECTED:
            LOGGER_ERROR("FACE_DETECTION");
            mcNixieHardware->ShowNumber("01    ", enumNumberAnimNone);
        break;
        case SIG_ERROR + SIG_NO_CAMERA:
            LOGGER_ERROR("NO CAMERA");
            mcNixieHardware->ShowNumber("02    ", enumNumberAnimNone);
        break;
        case SIG_ERROR + SIG_NO_WIFI:
            LOGGER_ERROR("NO WIFI");
            mcNixieHardware->ShowNumber("03    ", enumNumberAnimNone);
        break;
    }
    mcNixieHardware->SetVuPercent(0);
    SetState(STATE_ERROR);
}


/* \brief Sets the FSM state to INIT
   \note Here we set the various hardware parameters to their initial values.
*/
void cNixieFsm::SetStateInit()
{
    mcNixieHardware->SetVuPercent(100);
    mcNixieHardware->SetModulation(enumHardwareTypeVu, enuModulationType::enuModulationTypeSinusoidal, 3000);
    mcNixieHardware->ShowNumber("000000");
    mcNixieHardware->SetLamp(enumLampAll, false);
    mcNixieHardware->SetRgb(180, 255, 255);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationType::enuModulationTypeSinusoidal, 1000);
    mcNixieHardware->SetDimmerPercent(100);
    m_seconds_on = 7;
    m_testvalue = 10;
    SetState(STATE_INIT);
}

/*! \brief Processes the INIT state
    \param _action The action signal received.
    \return Returns true. That's why we don't care about the return value.
    \note Transitions from INIT to NETWORK state after testing the tubes
*/
bool cNixieFsm::ProcessStateInit(int _action)
{
    if(m_seconds_on == 0)
    {
        SetStateNetwork();
        add_action(SIG_NONE);
    }
    else if(_action == SIG_TIME_CHANGED)
    {
        if(m_seconds_on > 6) return false;
        char txt[7] = "      ";
        uint8_t pos = m_seconds_on - 1;
        for(int i = 10; i >= 0; i--)
        {
            txt[pos] = i ==0?' ':'0' - 1 + i;
            mcNixieHardware->ShowNumber(txt);
            usleep(90 * 1000);
        }
    } 
    return true;
}

/*! \brief Sets FSM to NETWORK state
    \note This function sets the FSM to the NETWORK state, where it will wait for a network connection.
*/
void cNixieFsm::SetStateNetwork()
{
    SetState(STATE_NETWORK);
}

/*! \brief Processes the NETWORK state
    \param _action The action signal received.
    \return Returns true
    \note In this state, the FSM checks for network connectivity and transitions to either HOTSPOT or SLEEPING state.
    \note If a network connection is established, it updates the weather report and sets the state to SLEEPING.
    \note Otherwise, it checks the network status and may transition to HOTSPOT state if no WiFi is available.
*/
bool cNixieFsm::ProcessStateNetwork(int _action)
{
    switch(_action)
    {
        case SIG_NETWORK_CONNECTED:
           LOGGER_DEBUG("Connected. Cheking for weather report");
            mcWeatherReport->UpdateForecast();
            //straight away, so the first day after a reboot does not run on the fallback window
            RefreshAstro();
            SetStateSleeping();
            break;
        case SIG_NETWORK_HOTSPOT:
            LOGGER_DEBUG("Setting up HOTSPOT");
            SetStateHotspot();
            break;
        default:
            switch(getNetworkStatus())
            {
                case NETWORK_STATUS_WIFI: 
                    LOGGER_DEBUG("We got a wifi connection!");
                    add_action(SIG_NETWORK_CONNECTED);
                break;
                case NETWORK_STATUS_HOTSPOT: 
                    LOGGER_DEBUG("No Wifi... let's turno to hotspot");
                    add_action(SIG_NETWORK_HOTSPOT);
                    break;
                default:
                    usleep(500 * 1000); //500ms
                    add_action(SIG_NONE);
                    break;
            }
            break;
    }
    return true;
}

/*! \brief Sets the FSM state to SLEEPING
    \note This function sets the FSM to the SLEEPING state, where it will idle and wait for events.
    \note It also sets the hardware parameters for the SLEEPING state, turning everything off
    \note except for pulsing redish RGBs.
*/
void cNixieFsm::SetStateSleeping()
{
    mcNixieHardware->SetVuPercent(0);
    mcNixieHardware->SetRgb(255, 128, 127);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationType::enuModulationTypeSinusoidal, 5000);
    mcNixieHardware->SetModulation(enumHardwareTypeDimmer, enuModulationType::enuModulationTypeRampOneShotDown, 2000);
    mcNixieHardware->SetModulation(enumHardwareTypeVu, enuModulationType::enuModulationTypeNone);
    SetState(STATE_SLEEPING);
}

/*! \brief Processes the SLEEPING state
    \param _action The action signal received.
    \return Returns true 
    \note In this state, the FSM checks for various actions such as time changes, face detection, and motion detection.
    \note If a full hour or 30 minutes is detected, it transitions to the AWAKE state with the appropriate display mode.
    \note If a face or motion is detected, it also transitions to the AWAKE state.
*/
bool cNixieFsm::ProcessStateSleeping(int _action)
{
    switch (_action)
    {
    case SIG_TIME_CHANGED:
        if(CheckNightRegen())
            break;
        if(mTmTime.tm_min == 59 && mTmTime.tm_sec == 55)
        {
            //The gate is asked about the hour being announced, never about now. We wake five
            //seconds early, so asking about now would put the question at xx:59:55 and a window
            //opening at 07:00 would swallow the seven o'clock chime every single morning.
            if(const int announced = (mTmTime.tm_hour + 1) % 24; IsDaytimeHour(announced))
            {
                LOGGER_DEBUG("Full hour detected, showing time");
                SetStateAwake(30, enumDisplayModeTime);
            }
        }
        else if(mTmTime.tm_min == 30 && mTmTime.tm_sec == 0)
        {
            if(!gScheduleConfig.gate_half_hour || IsDaytimeHour(mTmTime.tm_hour))
            {
                LOGGER_DEBUG("30 min detected... showing date");
                SetStateAwake(15, enumDisplayModeDate);
            }
        }
        break;
    case SIG_FACE_DETECTED:
        LOGGER_DEBUG("Face detected... waking up");
        //on_timeout is what the web page calls "display time". It was read out of the
        //configuration file and then never used anywhere: this 90 was hardcoded, and happened
        //to match the shipped value, so the setting looked like it worked.
        SetStateAwake(gCameraConfig.on_timeout, enumDisplayModeTime);
        break;
    case SIG_MOTION_DETECTED_ANY:
        LOGGER_DEBUG("Motion detected... Waking up");
        //Deliberately not on_timeout: movement is weaker evidence that somebody is actually
        //looking at the clock than a face is, and it gets a short glance rather than a session.
        SetStateAwake(MOTION_WAKE_SECONDS, enumDisplayModeTime);
        break;
    case SIG_MOTION_DETECTED_LEFT:
    case SIG_MOTION_DETECTED_RIGHT:
        //A sweep while the tubes are dark wakes them straight onto the mode it asked for,
        //rather than spending the first gesture just switching the clock on and making the
        //user repeat it. The base is reset to Time first, so the result depends only on the
        //gesture and not on whatever the clock happened to be showing when it fell asleep
        //hours earlier. The lock keeps the half minute date/time rotation from overwriting
        //the mode a second later, which it otherwise would.
        menumDisplayMode = enumDisplayModeTime;
        CycleDisplayMode(_action == SIG_MOTION_DETECTED_RIGHT);
        glogger->debug("Gesture {:s} while asleep... waking up on {:s}",
                       (_action == SIG_MOTION_DETECTED_RIGHT) ? "RIGHT" : "LEFT",
                       get_display_mode_name(menumDisplayMode));
        SetStateAwake(MOTION_GESTURE_SECONDS, menumDisplayMode);
        m_seconds_lock = 5;
        break;

    default:
        break;
    }
    return true;
}

/*! \brief Sets mode to AWAKE, setting the display accordinly
    \param _seconds_on The number of seconds to keep the display awake.
    \param _mode The display mode to set (time, date, etc.).
    \note It also sets the RGB color based on the weather forecast and applies modulation to the dimmer.
*/
void cNixieFsm::SetStateAwake(uint32_t _seconds_on, enumDisplayMode _mode)
{
    menumDisplayMode = _mode;
    //Every extension while awake adds to whatever was left, and the camera can report a face
    //several times a second. Without a ceiling, somebody sitting in front of the clock would
    //push the remaining time up faster than it counts down and the tubes would never sleep.
    const uint32_t ceiling = (gCameraConfig.on_timeout > 0)
                             ? static_cast<uint32_t>(gCameraConfig.on_timeout) : _seconds_on;
    m_seconds_on = (_seconds_on > ceiling) ? ceiling : _seconds_on;
    if(GetState() == STATE_AWAKE)
        return;
    mcNixieHardware->SetLamp(enumLampAll, false);
    mcNixieHardware->SetVuPercent(0);
    auto forecast = mcWeatherReport->GetForecast();
    mcNixieHardware->SetRgb(forecast->clouds.r, forecast->clouds.g, forecast->clouds.b);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationType::enuModulationTypeNone);
    mcNixieHardware->SetModulation(enumHardwareTypeDimmer, enuModulationType::enuModulationTypeRampOneShotUp, 3000);
    SetDisplay(menumDisplayMode);        
    SetState(STATE_AWAKE);
}

/*! \brief Steps the display mode one place forward or back, wrapping at both ends.
    \param _forward True to advance, false to go back.
    \note Both sweep directions cycle now, in opposite directions. A leftward sweep used to
    \note put the clock to sleep, and that could not be made safe: a person simply walking
    \note past the clock traces a clean one way path across the band and is indistinguishable
    \note from a deliberate sweep -- no amount of work on the detector changes that, because
    \note the two really do look the same to one camera. So the cost of a wrong reading had to
    \note come down instead of its probability. Getting the direction wrong now shows the date
    \note instead of the time; it no longer turns the clock off in somebody's face. The tubes
    \note sleep on their own timer, which is the only thing that was ever reliable about it.
*/
void cNixieFsm::CycleDisplayMode(bool _forward)
{
    auto aux = static_cast<int>(menumDisplayMode);
    aux += _forward ? 1 : -1;
    if(aux > enumDisplayModeCloud)
        aux = enumDisplayModeTime;
    else if(aux < enumDisplayModeTime)
        aux = enumDisplayModeCloud;
    menumDisplayMode = static_cast<enumDisplayMode>(aux);
}

const char *cNixieFsm::get_display_mode_name(enumDisplayMode _mode) const
{
    switch(_mode)
    {
        case enumDisplayModeTime:       return "Time";
        case enumDisplayModeDate:       return "Date";
        case enumDisplayModeTempLimit:  return "Temp Limit";
        case enumDisplayModeCloud:      return "Clouds";
        default:                        return "Unknown";
    }
}

/*! \brief Processes the AWAKE state
    \param _action The action signal received.
    \return Returns true 
    \note In this state, the FSM checks for various actions such as time changes, face detection, and motion detection.
    \note If a motion is detected, cycles the display mode or turns off.
    \note If a face or motion is detected, it keeps the display awake
*/
bool cNixieFsm::ProcessStateAwake(int _action)
{
    switch (_action)
    {
    case SIG_TIME_CHANGED:
        if(m_seconds_lock)
            m_seconds_lock--;
        else{
            if(mTmTime.tm_sec == 30)
                menumDisplayMode = enumDisplayModeDate;
            else if(mTmTime.tm_sec == 45)
                menumDisplayMode = enumDisplayModeTime;
        }

        SetDisplay(menumDisplayMode);

        if(m_seconds_on == 0)
        {
            SetStateSleeping();
            return true;
        }
        break;
    case SIG_FACE_DETECTED:
        LOGGER_DEBUG("Face detected... keeping up");
        SetStateAwake(m_seconds_on + 30, menumDisplayMode);
        m_seconds_lock = 5;
        break;
    case SIG_MOTION_DETECTED_ANY:
        SetStateAwake(m_seconds_on + 5, menumDisplayMode);
        break;
    case SIG_MOTION_DETECTED_LEFT:
    case SIG_MOTION_DETECTED_RIGHT:
        CycleDisplayMode(_action == SIG_MOTION_DETECTED_RIGHT);
        glogger->debug("Gesture {:s}... cycling display to {:s}",
                       (_action == SIG_MOTION_DETECTED_RIGHT) ? "RIGHT" : "LEFT",
                       get_display_mode_name(menumDisplayMode));
        SetStateAwake(m_seconds_on + MOTION_GESTURE_SECONDS, menumDisplayMode);
        m_seconds_lock = 5;
        break;
    default:
        break;
    }
    return true;
}

/*! \brief Processes the ERROR state
    \param _action The action signal received.
    \return Returns true
    \note In this state, the FSM flashes both temperature and weekday lamps
*/
bool cNixieFsm::ProcessStateError(int _action) const {
    if(_action != SIG_TIME_CHANGED)
        return false;
    mcNixieHardware->SetLamp(enuLampTemp, m_seconds & 1);
    mcNixieHardware->SetLamp(enuLampWeekday,(m_seconds & 1) == 0);
    return true;
}

/*! \brief Processes the default state
    \param _action The action signal received.
    \return Returns true
    \note This function is a placeholder for any default processing that might be needed.
*/
bool cNixieFsm::ProcessStateDefault(int _action)
{
    return true;
}

/*! \brief Sets the FSM state to HOTSPOT
    \note This function sets the FSM to the HOTSPOT state, where it will display the IP address in a cycling manner.
    \note It also sets the hardware parameters for the HOTSPOT state, including VU and RGB modulation.
    \note The IP address is displayed in a cycling manner every 2 seconds.
*/
void cNixieFsm::SetStateHotspot()
{
    getIPAddressBytes("wlan0", m_ipBytes);
    mcNixieHardware->SetLamp(enuLampHms, false);
    mcNixieHardware->SetVuPercent(100);
    mcNixieHardware->SetModulation(enumHardwareTypeVu, enuModulationType::enuModulationTypeSinusoidal, 5000);
    mcNixieHardware->SetRgb(0, 0, 255);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationType::enuModulationTypeSinusoidal, 1000);
    mcNixieHardware->SetDimmerPercent(100);
    SetState(STATE_HOTSPOT);
    m_seconds = 8; // to cycle the IP to index zero
}

/*! \brief Processes the HOTSPOT state
    \param _action The action signal received.
    \return Returns true
    \note In this state, the FSM displays the IP address in a cycling manner every 2 seconds.
    \note It also checks for the existence of a specific file (SSID_CHECK_FILE) to trigger a reboot.
    \note The lamps for temperature and weekday are toggled every second.
*/
bool cNixieFsm::ProcessStateHotspot(int _action)
{
    if(_action != SIG_TIME_CHANGED)
        return false;
    
    if(m_seconds >= 8)
        m_seconds = 0;
    uint8_t ipindex = m_seconds / 2;
    char txt[8];
    snprintf(txt, sizeof(txt), "%6d", m_ipBytes[ipindex % 4]);
    mcNixieHardware->SetLamp(enuLampTemp, m_seconds & 1);
    mcNixieHardware->SetLamp(enuLampWeekday,m_seconds & 1);
    mcNixieHardware->ShowNumber(txt, enumNumberAnimFlip, 10);

    if(fs::exists(SSID_CHECK_FILE))
    {
        LOGGER_DEBUG("SSID check file found");
        SetStateReboot();
    }
    return true;
}

/*! \brief Sets the FSM state to TEMPORARY
    \param _seconds_on The number of seconds to keep the display awake in this temporary state.
    \note This function sets the FSM to the TEMPORARY state,used by the CGI state
*/
void cNixieFsm::SetStateTemporary(uint32_t _seconds_on)
{
    m_seconds_on = _seconds_on;
    SetState(STATE_TEMPORARY);
}

/*! \brief Processes the TEMPORARY state
    \param _action The action signal received.
    \return Returns true
    \note In this state, the FSM checks for time changes and transitions to SLEEPING if m_seconds_on is zero.
*/
bool cNixieFsm::ProcessStateTemporary(int _action)
{
    if(_action != SIG_TIME_CHANGED)
        return false;
    if(m_seconds_on == 0)
        SetStateSleeping();
    return true;
}

/*! \brief Sets the FSM state to REBOOT
    \note This function sets the FSM to the REBOOT state, where it will display a countdown before rebooting.
    \note It also sets the hardware parameters for the REBOOT state, including VU and RGB modulation.
    \note The display shows a countdown from 10 seconds before rebooting.
    \note This state is used when a critical configuration is changed and only a fresh start will apply the changes.
*/
void cNixieFsm::SetStateReboot()
{
    SetState(STATE_REBOOT);
    m_seconds_on = 11;
    mcNixieHardware->SetVuPercent(0);
    mcNixieHardware->SetModulation(enumHardwareTypeVu, enuModulationType::enuModulationTypeNone);
    mcNixieHardware->SetRgb(255, 0, 0);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationType::enuModulationTypeNone);
    mcNixieHardware->SetLamp(enumLampAll, false);
}

/*! \brief Processes the REBOOT state
    \param _action The action signal received.
    \return Returns true
    \note In this state, the FSM displays a countdown from 10 seconds before rebooting.
    \note If m_seconds_on reaches zero, it reboots the system.
*/
bool cNixieFsm::ProcessStateReboot(int _action) const {
    if(_action != SIG_TIME_CHANGED)
        return false;

    char txt[8];
    snprintf(txt, sizeof(txt), "  %02d  ", m_seconds_on);
    mcNixieHardware->ShowNumber(txt, enumNumberAnimNone);
    if(m_seconds_on == 0)
    {
        sleep(1);
        mcNixieHardware->SetDimmerPercent(0);
        reboot(LINUX_REBOOT_CMD_RESTART);
    }
    return true;
}

/*! \brief Sets the FSM state to REGEN, the cathode regeneration routine
    \param _position The tube to regenerate as an internal index, 0 being the leftmost and 5 the
    \param _position seconds units. The web page numbers them 1 to 6, so the caller subtracts one.
    \note Nixie cathodes that are seldom lit get poisoned and stop glowing properly. This state
    \note cycles 0..9 on a single tube at full brightness, indefinitely, to burn the deposit off.
    \note Everything else is turned off so the whole high voltage budget goes to the tube under repair.
    \note The routine only ends when the web page asks for it, or on a reboot.
*/
void cNixieFsm::SetStateRegen(uint8_t _position)
{
    if(_position > REGEN_TUBE_MAX - 1)
        _position = REGEN_TUBE_DEFAULT - 1;
    m_regen_position = _position;
    m_regen_mode = enumRegenModeManual;
    m_regen_ticks = 0;
    m_regen_digit = 0;

    mcNixieHardware->SetAllOff();
    mcNixieHardware->SetModulation(enumHardwareTypeVu, enuModulationTypeNone);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationTypeNone);
    mcNixieHardware->SetModulation(enumHardwareTypeDimmer, enuModulationTypeNone);
    mcNixieHardware->SetVuPercent(0);
    mcNixieHardware->SetRgb(0, 0, 0);
    //full current on the tubes, ignoring the configured brightness cap, is what burns the poisoning off
    mcNixieHardware->SetMaxBrightness(100);

    glogger->debug("NixieFsm: Cathode regeneration started on tube {:d}", m_regen_position + 1);
    SetState(STATE_REGEN);
}

/*! \brief Sets the FSM to the automatic night time regeneration
    \note This is prevention, and it deliberately looks nothing like the manual routine above.
    \note All six tubes run at once, at the brightness the user configured, for a counted number
    \note of sweeps. Overdriving healthy tubes every night would spend exactly the life the
    \note routine exists to preserve, so the sobrecorrente stays in the on demand repair mode.
    \note A full 0..9 sweep of every cathode on every tube costs ten seconds, which is what makes
    \note several short sessions a night cheaper than one long one.
*/
void cNixieFsm::SetStateRegenNight()
{
    m_regen_mode = enumRegenModeNight;
    m_regen_ticks = 0;
    m_regen_digit = 0;
    m_regen_cycles = static_cast<uint32_t>(gRegenConfig.cycles);

    mcNixieHardware->SetAllOff();
    mcNixieHardware->SetModulation(enumHardwareTypeVu, enuModulationTypeNone);
    mcNixieHardware->SetModulation(enumHardwareTypeRgb, enuModulationTypeNone);
    mcNixieHardware->SetModulation(enumHardwareTypeDimmer, enuModulationTypeNone);
    mcNixieHardware->SetVuPercent(0);
    mcNixieHardware->SetRgb(0, 0, 0);
    mcNixieHardware->SetMaxBrightness(gNixieConfig.brightness);

    glogger->debug("NixieFsm: Night regeneration started, {:d} sweeps on all tubes", m_regen_cycles);
    SetState(STATE_REGEN);
}

/*! \brief Starts an automatic regeneration session when the clock reaches one of the scheduled times
    \return true when a session was started, so the caller leaves the rest of the tick alone.
    \note The window is two seconds wide rather than an exact second on purpose. The time thread
    \note rides on steady_clock, so an ntp step can make the wall clock skip a second outright,
    \note and an exact match would silently drop a whole session. The minute and day of the last
    \note session keep the wider window from firing the same one twice.
*/
bool cNixieFsm::CheckNightRegen()
{
    if(!gRegenConfig.enabled)
        return false;
    if(mTmTime.tm_sec > 1)
        return false;

    const int now = mTmTime.tm_hour * 60 + mTmTime.tm_min;
    bool scheduled = false;
    for(int i = 0; i < gRegenConfig.count && !scheduled; i++)
        scheduled = gRegenConfig.minutes[i] == now;
    if(!scheduled)
        return false;

    if(m_regen_last_minute == now && m_regen_last_yday == mTmTime.tm_yday)
        return false;
    m_regen_last_minute = now;
    m_regen_last_yday = mTmTime.tm_yday;

    SetStateRegenNight();
    return true;
}

/*! \brief Leaves the cathode regeneration routine
    \note Restores the brightness configured by the user and drops back to SLEEPING.
    \note Serves both routines: the manual one restores a brightness it had overridden, the night
    \note one restores the very same value it was already using, which costs nothing.
*/
void cNixieFsm::StopStateRegen()
{
    if(GetState() != STATE_REGEN)
        return;
    mcNixieHardware->SetMaxBrightness(gNixieConfig.brightness);
    m_dimmed = false;
    LOGGER_DEBUG("Cathode regeneration stopped");
    SetStateSleeping();
}

/*! \brief Tells whether a scheduled wake up may light the tubes at the given hour
    \param _hour The hour being announced, 0 to 23, not the instant of the wake up.
    \return true when the tubes may come on.
    \note Uses the real sunrise and sunset while the weather service has given us usable ones,
    \note and the fixed window from the config otherwise. Anything unexpected resolves to daytime:
    \note a clock that wrongly stays dark is far harder to notice than one that wrongly lights up.
    \note Face and motion never come through here. Someone standing in front of the clock at three
    \note in the morning wants to see the time, and that is the whole point of the camera.
*/
bool cNixieFsm::IsDaytimeHour(int _hour) const
{
    if(_hour < 0 || _hour > 23)
        return true;

    if(gScheduleConfig.use_astro)
    {
        if(const sAstro *sun = mcWeatherReport->GetAstro(); sun != nullptr)
        {
            const int minutes = _hour * 60;
            return minutes >= sun->sunrise && minutes <= sun->sunset;
        }
    }
    //load_schedule_config() already rejected a window that made no sense, so this is safe
    return _hour >= gScheduleConfig.day_start && _hour <= gScheduleConfig.day_end;
}

/*! \brief Fetches sunrise and sunset when the configuration asks for them and we have a network */
void cNixieFsm::RefreshAstro() const
{
    if(!gScheduleConfig.use_astro)
        return;
    if(getNetworkStatus() != NETWORK_STATUS_WIFI)
        return;
    mcWeatherReport->UpdateAstro();
}

/*! \brief Processes the REGEN state
    \param _action The action signal received.
    \return Returns true if the action was handled.
    \note Advances one digit every REGEN_TICKS_PER_DIGIT ticks and returns at once. The one
    \note second tick is what paces the sweep, so the handler never blocks and no action ever
    \note piles up in the queue, however long the routine runs.
    \note In the manual mode face and motion are ignored on purpose: a repair must not be
    \note interrupted by someone walking past the clock, and only SIG_CGI_REGEN_OFF ends it,
    \note which PreprocessAction handles before we ever get here. The night mode gives way
    \note instead, because whoever is up at that hour would rather have the time back, and the
    \note dose simply resumes at the next scheduled session.
*/
bool cNixieFsm::ProcessStateRegen(int _action)
{
    if(m_regen_mode == enumRegenModeNight)
    {
        switch(_action)
        {
            case SIG_FACE_DETECTED:
            case SIG_MOTION_DETECTED_ANY:
            case SIG_MOTION_DETECTED_LEFT:
            case SIG_MOTION_DETECTED_RIGHT:
                LOGGER_DEBUG("Night regeneration interrupted by the camera");
                StopStateRegen();
                return true;
            default:
                break;
        }
    }

    if(_action != SIG_TIME_CHANGED)
        return false;

    if(m_regen_ticks == 0)
    {
        if(m_regen_mode == enumRegenModeNight)
        {
            //Every tube shows a different digit, each one a step ahead of its neighbour. The
            //display rolls like a slot machine, and because the offsets are fixed every cathode
            //still gets exactly the same time, which random digits would not guarantee.
            char txt[REGEN_TUBES + 1];
            for(uint8_t i = 0; i < REGEN_TUBES; i++)
                txt[i] = static_cast<char>('0' + (m_regen_digit + i) % 10);
            txt[REGEN_TUBES] = '\0';
            mcNixieHardware->ShowNumber(txt, enumNumberAnimNone);
        }
        else
            mcNixieHardware->ShowSingleDigit(m_regen_position, m_regen_digit);
    }

    if(++m_regen_ticks >= REGEN_TICKS_PER_DIGIT)
    {
        m_regen_ticks = 0;
        m_regen_digit = (m_regen_digit + 1) % 10;

        //wrapping back to zero means a full sweep just finished, so the night dose is one short
        if(m_regen_digit == 0 && m_regen_mode == enumRegenModeNight)
        {
            if(m_regen_cycles > 0)
                m_regen_cycles--;
            if(m_regen_cycles == 0)
            {
                LOGGER_DEBUG("Night regeneration session finished");
                StopStateRegen();
            }
        }
    }
    return true;
}

/*! \brief Updates the current date and time
    \note This function retrieves the current time and updates the mTmTime member variable.
    \note It also checks if the weather report needs to be updated every hour at 56 minutes past the hour.
    \note If the network status is WIFI, it updates the weather report.
    \note This function is called periodically to keep the time and weather report up to date.
*/
void cNixieFsm::UpdateNumbers()
{
    time_t now = time(nullptr);
    if(const struct tm *tm = localtime(&now))
        mTmTime = *tm;

    if(mTmTime.tm_min == 56 && mTmTime.tm_sec == 0 && getNetworkStatus() == NETWORK_STATUS_WIFI)
    {
        //once every hour
        LOGGER_DEBUG("Cheking for weather report");
        mcWeatherReport->UpdateForecast();

        //and catch up on the sunrise if we have never had it or it went stale, so a network
        //that was down at the daily slot does not cost us a whole day on the fallback window
        if(gScheduleConfig.use_astro && mcWeatherReport->GetAstro() == nullptr)
            RefreshAstro();
    }

    //the daily fetch, kept well away from the hourly forecast call
    if(mTmTime.tm_hour == ASTRO_REFRESH_HOUR && mTmTime.tm_min == ASTRO_REFRESH_MIN &&
       mTmTime.tm_sec == 0)
    {
        LOGGER_DEBUG("Cheking for sunrise and sunset");
        RefreshAstro();
    }
}

/*! \brief Sets the display mode
    \param _mode The display mode to set (time, date, temperature limits).
    \note This function updates the Nixie hardware to show the current time, date, or temperature limits based on the specified mode.
    \note It also sets the VU temperature based on the current weather forecast.
    \note when displaying date, the VU is updated by the hardware object itself
*/
void cNixieFsm::SetDisplay(enumDisplayMode _mode) const {
    auto forecast = mcWeatherReport->GetForecast();
    switch(_mode)
    {
        case enumDisplayModeTime:
            mcNixieHardware->ShowTime(mTmTime, enumNumberAnimFlip, 10);
            mcNixieHardware->SetVuTemperature(forecast->current);
            break;
        case enumDisplayModeDate:
            mcNixieHardware->ShowDate(mTmTime, enumNumberAnimFlip, 10);
            break;
        case enumDisplayModeTempLimit:
            mcNixieHardware->ShowTempertaureLimits(forecast->minimum, forecast->maximum);
            mcNixieHardware->SetVuTemperature(forecast->current);
            break;
        case enumDisplayModeCloud:
            mcNixieHardware->ShowCloudPercentage(forecast->cloudPercentage);
            mcNixieHardware->SetVuTemperature(forecast->current);
            break;
    }
}
//eof fsm.cpp