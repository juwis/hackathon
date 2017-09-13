/**********************************************/
// defines.h
// Author: Julian Wingert
// 9/2017
// Content: fixed values for hackathon
// transmitter controller
/**********************************************/

#ifndef __DEFINES__H
#define __DEFINES__H

//IO pin mapping - DO NOT CHANGE until you really know what you do...
#define THROTTLE_SENSE_PIN			35
#define THROTTLE_CONTROL_PIN		26
#define STEERING_SENSE_PIN		34
#define STEERING_CONTROL_PIN	25
#define BUTTON_SENSE_PIN		17

//calibration minimum offset from senter value to begin transmitting data
#define MINIMAL_ACCEPTED_CALIBRATION_RANGE	500

//analog out basevalues
#define THROTTLE_MIN_OUT		0
#define THROTTLE_CNT_OUT		60
#define THROTTLE_MAX_OUT		255

#define STEER_MIN_OUT		0
#define STEER_CNT_OUT		64
#define STEER_MAX_OUT		128

//low pass filter fraction, (1/FILTER_FRACTION) of the new ADC value is accounted for in the new value
#define	FILTER_FRACTION		2.0

//array defines
#define MAP_MIN_INDEX	0
#define MAP_CENTER_INDEX	1
#define MAP_MAX_INDEX	2
#define MAP_SIZE		3


//networking stuff
#define NETWORK_SSID	"hackathonRCAP"
#define NETWORK_PASS	"geheimpass123"
#define MASTER_IP		"192.168.0.172"
#define MASTER_PORT		3333
#define LISTEN_PORT		31337

//transmission of live data, update rate
#define PACKETS_PER_SECOND			100

//I dont trust you...
#if PACKETS_PER_SECOND > 500
#ERROR - too many packets per second!
#endif
//dont touch!
#define PACKET_DELAY_MS				1000 / PACKETS_PER_SECOND

//how long is a master overwrite valid in ms?
#define OVERWRITE_PACKAGE_VALID_MS	500

#endif
