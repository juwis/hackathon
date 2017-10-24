// comm.h

#ifndef _COMM_h
#define _COMM_h

#include <WiFi.h>
#include <WiFiUdp.h>

#include "Arduino.h"

//master override data
struct s_transmitter_override{
	uint8_t out_throttle;
	uint8_t out_steer;
	uint64_t millis_last_update;
	uint32_t CRC;
	uint8_t ready;
} __attribute__((packed));

struct s_transmitter_control_packet{
	uint8_t out_throttle;
	uint8_t out_steer;
	uint32_t CRC;
} __attribute__((packed));

//master updates
extern volatile s_transmitter_override transmitter_output_override;

int init_comm(const char * ssid, const char * pwd, const char *server_ip, uint16_t server_port, uint16_t listen_port);
void start_comm(void);

#endif

