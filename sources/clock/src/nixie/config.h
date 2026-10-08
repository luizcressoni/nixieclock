/*! \file config.h */
#pragma once
#include "../utils/structs.h"

void load_config_file();

extern sNixieConfig    gNixieConfig;
extern sCameraConfig   gCameraConfig;   //!< the clock only reads on_timeout and motion_timeout out of this one
extern sLocation       gLocation;
extern sWifiConfig     gWifiConfig;
extern sScheduleConfig gScheduleConfig;
extern sRegenConfig    gRegenConfig;
//eof config.h