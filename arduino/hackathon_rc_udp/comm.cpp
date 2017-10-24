/************************************************************************************/
// comm.cpp
// Author: Julian Wingert
// 9/2017
// Content: communication thread
// UDP Transmitter, continously sending live rc-transmitter state
// UDP Receiver can override user controls (throttle / steering)
/************************************************************************************/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <CRC32.h>
#include <esp_task_wdt.h>

#include "comm.h"
#include "transmitter_hal.h"

struct s_transmitter_state_packet{
	uint8_t in_throttle;
	uint8_t in_steer;
	uint16_t in_button;
	uint8_t out_throttle;
	uint8_t out_steer;
	uint16_t battery_voltage;
	uint32_t CRC;
}ts_packet __attribute__((packed));

//updates from master
volatile s_transmitter_override transmitter_output_override;

char *send_buffer;
char *recv_buffer;

bool connected; //event method is static

WiFiUDP udp;

uint16_t server_port;
const char *server_address;
uint16_t listen_port;
const char *station_ssid;
const char *station_pwd;

CRC32 crc;

void TASK_comm_run(void *param);


void wifi_event(WiFiEvent_t event){
	switch (event) {
	case SYSTEM_EVENT_STA_CONNECTED:
		connected = true;
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		connected = false;
		break;
	}
}


void wifi_connect(){
	// delete old config
	WiFi.disconnect(true);
	//register event handler
	WiFi.onEvent(wifi_event);
	//listen for incoming packets
	udp.begin(listen_port);
	//Initiate connection
	WiFi.config(IPAddress(192, 168, 0, 103), IPAddress(192, 168, 0, 172), IPAddress(255, 255, 255, 0));
	WiFi.begin(station_ssid, station_pwd);

}



void TASK_comm_work(void *param){

}

void start_comm(void){
	xTaskCreate(
		TASK_comm_run,          /* Task function. */
		"TASK_COMM_RUN",        /* String with name of task. */
		16384,            /* Stack size in words. */
		NULL,             /* Parameter passed as input of the task */
		1,                /* Priority of the task. */
		NULL);			/* Task handle. */
}



int init_comm(const char * ssid, const char * pwd, const char *server_ip, uint16_t srv_port, uint16_t local_port){
	connected == false;
	station_ssid = ssid;
	station_pwd = pwd;
	server_address = server_ip;
	server_port = srv_port;
	listen_port = local_port;

	wifi_connect();
}


//The comm task is continously reporting the live transmitter state via UDP
//and checking for responses to overwrite the transmitter settings
#define INCOMING_PACKET_BUFSIZE		2 + 4	//steer(1), throttle(1) and CRC32(4)...

void TASK_comm_run(void *param){
	int packet_size = 0;
	uint64_t last_send_packet_millis = millis();
	uint64_t last_millis = millis();

	while (true){

		packet_size = udp.parsePacket(); //returns 0 in case there is none...
		if (packet_size != 0)
		{
			// receive incoming UDP packets
			//Serial.println("New Packet...");
			if (packet_size > INCOMING_PACKET_BUFSIZE){
				char incoming_packet_buffer[INCOMING_PACKET_BUFSIZE];
				//this is something that obviously must be an error - discard!
				while (0 != udp.readBytes(incoming_packet_buffer, INCOMING_PACKET_BUFSIZE));
				//Serial.printf("DEBUG: ERROR in COMM, received oversized package. MAX(%d) - RCVD(%d)\r\n", INCOMING_PACKET_BUFSIZE, packet_size);

			}
			else{
				//seems to be a new packet from the master...
				s_transmitter_control_packet inc_packet;
				memset(&inc_packet, 0, sizeof(s_transmitter_control_packet));
				//bad bad and ugly, but maybe it will work...
				int rcv_len = udp.read((char *)&inc_packet, sizeof(s_transmitter_control_packet));
				uint8_t steer_received = inc_packet.out_steer;
				uint8_t throttle_received = inc_packet.out_throttle;
				uint32_t crc_received = inc_packet.CRC;
				crc.reset();
				uint32_t crc_calculated = crc.calculate(&inc_packet, 2);

				//Serial.printf("S: %d / T: %d (%u/%u)\r\n", steer_received, throttle_received, crc_received, crc_calculated);

				if (crc_received == crc_calculated){
					transmitter_output_override.ready = false; //semaphore, 8 bit shall be atomic... Only necessary for millis, therefore not this important...
					transmitter_output_override.out_steer = steer_received;
					transmitter_output_override.out_throttle = throttle_received;
					transmitter_output_override.CRC = crc_received;
					transmitter_output_override.millis_last_update = millis();
					transmitter_output_override.ready = true;
				}
				else{
					//crc error - zero struct - also sets ready to false :)
					Serial.println("CRC ERROR");
					Serial.printf("R: %u / C: %u\r\n", crc_received, crc_calculated);
					memset((void *)&transmitter_output_override, 0, sizeof(s_transmitter_override));
				}

			}


		}

		//send packet every PACKET_DELAY_MS milliseconds
		if (last_send_packet_millis + PACKET_DELAY_MS <= millis()){
			if (connected){
				//Send a packet
				udp.beginPacket(server_address, server_port);

				//copy state in transmission packet, omitting CRC
				memcpy(&ts_packet, (const void*)&transmitter_state, sizeof(s_transmitter_state));
				//calculate CRC
				crc.reset();
				//Attention! CRC is calculated using the size -4 to omit crc itself being adressed for calculation
				ts_packet.CRC = crc.calculate((const void*)&ts_packet, sizeof(s_transmitter_state_packet) - 4);
				udp.write((const uint8_t*)&ts_packet, sizeof(s_transmitter_state_packet));
				udp.endPacket();
				udp.flush();
			}
		}

		vTaskDelay(portTICK_PERIOD_MS);

	}
	//unreachable!

	vTaskDelete(NULL); //cleanup for Task
}