// transmitter_hal.h

#ifndef _TRANSMITTER_HAL_h
#define _TRANSMITTER_HAL_h

#include "Arduino.h"
#include "defines.h"

struct s_transmitter_state{
	uint8_t in_throttle;
	uint8_t in_steer;
	uint16_t in_button;
	uint8_t out_throttle;
	uint8_t out_steer;
	uint16_t battery_voltage;
} __attribute__((packed));

//realtime updated state of transmitter, gets written from transmitter_hal
extern volatile s_transmitter_state transmitter_state;

void start_transmitter_hal(void);

#endif

