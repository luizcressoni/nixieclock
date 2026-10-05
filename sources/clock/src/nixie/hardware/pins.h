/*! \file pins.h */
#pragma once

#define PIN_PWM_T               GPIO_24_GEN5        //IN-9 tube for temperature and weekday (PWM)

#define PIN_PWM_LAMPS           GPIO_23_GEN4        //High voltage dimmer for the tubes
#define PIN_LAMP_TEMP           GPIO_25_GEN6        //Temperature indicator lamp
#define PIN_LAMP_WDAY           GPIO_08_CE0         //Weekday indicator lamp
#define PIN_LAMP_HMS            GPIO_07_CE1         //Colon lamps separating hours, minutes and seconds
//#define PIN_RESET               GPIO_12           

#define PIN_RGB_R               GPIO_16             //RGB channel R PWM
#define PIN_RGB_G               GPIO_20             //RGB channel G PWM
#define PIN_RGB_B               GPIO_26             //RGB channel B PWM

#define PIN_N_DATA              GPIO_17_GEN0        //74HC595 data line, pin 14
#define PIN_N_CLOCK             GPIO_27_GEN2        //shift clock line, pin 11
#define PIN_N_BCLOCK            GPIO_05             //latch clock, pin 12
#define PIN_N_RESET             GPIO_22_GEN3        //shift register reset, pin 10
//eof pins.h
