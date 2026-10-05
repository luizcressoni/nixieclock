/*! \file pins.h */
#pragma once

#define PIN_PWM_T               GPIO_24_GEN5        //Valvula IN-9 para temperatura e dia da semana (PWM)

#define PIN_PWM_LAMPS           GPIO_23_GEN4        //Dimmer da alta voltagem para as valvulas
#define PIN_LAMP_TEMP           GPIO_25_GEN6        //Lampada indicadora da temperatura
#define PIN_LAMP_WDAY           GPIO_08_CE0         //Lampada indicadora de dia da semana
#define PIN_LAMP_HMS            GPIO_07_CE1         //Lampadas Dois Pontos separador de hora, minuto e segundos
//#define PIN_RESET               GPIO_12           

#define PIN_RGB_R               GPIO_16             //RGB canal R PWM
#define PIN_RGB_G               GPIO_20             //RGB canal G PWM
#define PIN_RGB_B               GPIO_26             //RGB canal B PWM

#define PIN_N_DATA              GPIO_17_GEN0        //Data line dos 74HC595 pino 14
#define PIN_N_CLOCK             GPIO_27_GEN2        //shift Clock line pino 11
#define PIN_N_BCLOCK            GPIO_05             //latch clock pino 12
#define PIN_N_RESET             GPIO_22_GEN3        //shift register Reset pino 10
//eof pins.h
