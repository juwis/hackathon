/************************************************************************************/
// transmitter_hal.cpp
// Author: Julian Wingert
// 9/2017
// Content: Hardware Abstraction Layer
// offers a global struct with live values read from and sent to the rc-transmitter
// reads the global override struct from the comm library
/************************************************************************************/
#include <esp_task_wdt.h>

#include "defines.h"
#include "transmitter_hal.h"
#include "comm.h"

volatile s_transmitter_state transmitter_state;

void TASK_transmitter_hal_run(void *param);

//LUT based linear interpolation
//stolen from https://playground.arduino.cc/Main/MultiMap
//gently modified...
uint16_t multiMap(uint16_t val, uint16_t* _in, uint16_t* _out, uint8_t size)
{
	// take care the value is within range
	// val = constrain(val, _in[0], _in[size-1]);
	if (val <= _in[0]) return _out[0];
	if (val >= _in[size - 1]) return _out[size - 1];

	// search right interval
	uint8_t pos = 1;  // _in[0] allready tested
	while (val > _in[pos]) pos++;

	// this will handle all exact "points" in the _in array
	if (val == _in[pos]) return _out[pos];

	// interpolate in the right segment for the rest
	return (val - _in[pos - 1]) * (_out[pos] - _out[pos - 1]) / (_in[pos] - _in[pos - 1]) + _out[pos - 1];
}

void start_transmitter_hal(void){
	xTaskCreate(
		TASK_transmitter_hal_run,          /* Task function. */
		"TASK_TRANSMITTER_HAL_RUN",        /* String with name of task. */
		16384,            /* Stack size in words. */
		NULL,             /* Parameter passed as input of the task */
		1,                /* Priority of the task. */
		NULL);			/* Task handle. */
}


void TASK_transmitter_hal_run(void *param){
	bool calibrated = false;
	uint64_t last_millis = millis();

	//interpolation maps for throttle...
	uint16_t map_throttle_in[MAP_SIZE];
	uint16_t map_throttle_out[MAP_SIZE] = { THROTTLE_MIN_OUT, THROTTLE_CNT_OUT, THROTTLE_MAX_OUT };
	//...and steering, three point linear interpolation
	uint16_t map_steer_in[MAP_SIZE];
	uint16_t map_steer_out[MAP_SIZE] = { STEER_MIN_OUT, STEER_CNT_OUT, STEER_MAX_OUT };

	//ADC Values read and filtered are put into theese
	uint16_t in_throttle = 0;
	uint16_t in_steer = 0;
	uint16_t min_throttle = UINT16_MAX;
	uint16_t max_throttle = 0;
	uint16_t min_steer = UINT16_MAX;
	uint16_t max_steer = 0;

	//ADC analog resolution to maximum HW supported (ESP32)
	analogReadResolution(12);

	//DAC output values, DAC maps 0-255 to 0-3.3V
	uint8_t out_throttle = THROTTLE_CNT_OUT;
	uint8_t out_steer = STEER_CNT_OUT;

	//pin settings, io configuration for input/output/dac/adc
	pinMode(THROTTLE_SENSE_PIN, INPUT);
	pinMode(STEERING_SENSE_PIN, INPUT);
	pinMode(BUTTON_SENSE_PIN, INPUT);

	pinMode(THROTTLE_CONTROL_PIN, OUTPUT);
	pinMode(STEERING_CONTROL_PIN, OUTPUT);

	//just send the defined mid-level to both dacs until they are calibrated
	dacWrite(STEERING_CONTROL_PIN, STEER_CNT_OUT);
	dacWrite(THROTTLE_CONTROL_PIN, THROTTLE_CNT_OUT);

	//wait 3 seconds for transmitter to adapt the sent mid-levels
	vTaskDelay(portTICK_PERIOD_MS * 3000);

	//start reading input values
	in_throttle = analogRead(THROTTLE_SENSE_PIN);
	in_steer = analogRead(STEERING_SENSE_PIN);

	//if transmitter is switched off, input values are zero/close to zero
	//wait until transmitter is switched on to continue
	while (in_throttle + in_steer < 10){
		vTaskDelay(portTICK_PERIOD_MS * 1000);
		in_throttle = analogRead(THROTTLE_SENSE_PIN);
		in_steer = analogRead(STEERING_SENSE_PIN);
		Serial.print("Transmitter is switched off, waiting...\r\n");
	}

	//filtered value for zero point
	for (int i = 0; i < (FILTER_FRACTION * FILTER_FRACTION); i++){
		in_throttle = in_throttle + (((double)analogRead(THROTTLE_SENSE_PIN) - (double)in_throttle) / FILTER_FRACTION);
		in_steer = in_steer + (((double)analogRead(STEERING_SENSE_PIN) - (double)in_steer) / FILTER_FRACTION);
		vTaskDelay(portTICK_PERIOD_MS * 10);
	}
	//now we have a center value
	map_throttle_in[MAP_CENTER_INDEX] = in_throttle;
	map_steer_in[MAP_CENTER_INDEX] = in_steer;

	while (true){
		//first read live values, filter them by FILTER_FRACTION
		in_throttle = in_throttle + (((double)analogRead(THROTTLE_SENSE_PIN) - (double)in_throttle) / FILTER_FRACTION);
		in_steer = in_steer + (((double)analogRead(STEERING_SENSE_PIN) - (double)in_steer) / FILTER_FRACTION);

		//continous calibration
			if (in_throttle < min_throttle){
				min_throttle = in_throttle;
				map_throttle_in[MAP_MIN_INDEX] = min_throttle;
			}
			if (in_throttle > max_throttle){
				max_throttle = in_throttle;
				map_throttle_in[MAP_MAX_INDEX] = max_throttle;
			}

			if (in_steer < min_steer){
				min_steer = in_steer;
				map_steer_in[MAP_MIN_INDEX] = min_steer;
			}
			if (in_steer > max_steer){
				max_steer = in_steer;
				map_steer_in[MAP_MAX_INDEX] = max_steer;
			}

		//calibrate is false until min value is MINIMAL_ACCEPTED_CALIBRATION_RANGE
		//lower than center value, max value is MINIMAL_ACCEPTED_CALIBRATION_RANGE higher than center value
		//center value is MAP_CENTER_INDEX
		if (
			min_throttle + MINIMAL_ACCEPTED_CALIBRATION_RANGE < map_throttle_in[MAP_CENTER_INDEX]
			&& max_throttle - MINIMAL_ACCEPTED_CALIBRATION_RANGE > map_throttle_in[MAP_CENTER_INDEX]
			&& min_steer + MINIMAL_ACCEPTED_CALIBRATION_RANGE < map_steer_in[MAP_CENTER_INDEX]
			&& max_steer - MINIMAL_ACCEPTED_CALIBRATION_RANGE > map_steer_in[MAP_CENTER_INDEX]
			&& !calibrated
			){
			calibrated = true;
			Serial.printf("Calibration minimum reached\r\n");
		}

		if (calibrated){
			//live updates from finished calibration on...

			//first check if we need to overwrite the output
			//is semaphore ready & packet not too old
			if (
				transmitter_output_override.ready == true
				&& transmitter_output_override.millis_last_update + OVERWRITE_PACKAGE_VALID_MS > millis()
				){
				//replace values by master values
				//inverse map from 0-255 linear to real dac values (both values)
				out_steer = transmitter_output_override.out_steer;//map(transmitter_output_override.out_steer, 0, 255, STEER_MIN_OUT, STEER_MAX_OUT);
				out_throttle = map(transmitter_output_override.out_throttle, 0, 255, THROTTLE_MIN_OUT, THROTTLE_MAX_OUT);

			}
			else{
				//linear mapping of input to output values, based on three point LUT
				out_steer = multiMap(in_steer, map_steer_in, map_steer_out, MAP_SIZE);
				out_throttle = multiMap(in_throttle, map_throttle_in, map_throttle_out, MAP_SIZE);
			}
			//update global transmitter state
			transmitter_state.in_steer = multiMap(in_steer, map_steer_in, map_steer_out, MAP_SIZE);
			transmitter_state.in_throttle = multiMap(in_throttle, map_throttle_in, map_throttle_out, MAP_SIZE);
			transmitter_state.out_steer = out_steer;
			transmitter_state.out_throttle = out_throttle;


		}

		if (last_millis + 1000 < millis()){
			last_millis = millis();
			printf("in_steer: %d;in_throttle %d;out_steer: %d; out_throttle: %d\r\n", transmitter_state.in_steer, transmitter_state.in_throttle, transmitter_state.out_steer, transmitter_state.out_throttle);
		}

		dacWrite(STEERING_CONTROL_PIN, transmitter_state.out_steer);
		dacWrite(THROTTLE_CONTROL_PIN, transmitter_state.out_throttle);

		//release task focus for scheduler, delay for 1ms
		vTaskDelay(portTICK_PERIOD_MS);

	}
	//unreachable!
	vTaskDelete(NULL); //cleanup for Task
}