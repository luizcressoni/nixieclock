/*! \file dimmer.h */
#pragma once
#include "pwm.h"

cNixiePwm *init_dimmer(int _max);
void deinit_dimmer();

//eof dimmer.h