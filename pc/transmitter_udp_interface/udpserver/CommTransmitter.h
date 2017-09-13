#pragma once
#include <map>
#include <vector>
#include <list>
#include <chrono>
#include <inttypes.h>
#include <thread>

#include "PracticalSocket.hpp" // For UDPSocket and SocketException

#ifdef __GNUC__
#define PACKED( class_to_pack ) class_to_pack __attribute__((__packed__))
#else
#define PACKED( class_to_pack ) __pragma( pack(push, 1) ) class_to_pack __pragma( pack(pop) )
#endif


PACKED(
struct s_transmitter_state_packet{
	uint8_t in_throttle;
	uint8_t in_steer;
	uint8_t out_throttle;
	uint8_t out_steer;
	uint32_t CRC;
};
);


PACKED(
struct s_transmitter_control_packet{
	uint8_t out_throttle;
	uint8_t out_steer;
	uint32_t CRC;
};
);


class Transmitter{
public:
	int monotonic_counter;
	string ip_address;
	unsigned int port;
	s_transmitter_state_packet ts_packet;
	chrono::system_clock::time_point last_packet_received;
	bool alive;
};


class TransmitterOverride{
public:
	string ip_address;
	unsigned int port;
	bool override_steer;
	bool override_throttle;
	s_transmitter_control_packet ts_ct_packet;
	bool sent;

	TransmitterOverride() : override_steer(false), override_throttle(false), sent(false) {};

};


class CommTransmitter {
private:
	CommTransmitter::CommTransmitter();

	map <string, Transmitter> connected_transmitters; //ipaddress is key
	vector<string> indexed_transmitters;
	std::list <TransmitterOverride> transmitter_override_queue;
	s_transmitter_state_packet ts_packet;
	unsigned short listen_port;
	bool running, stop;
	UDPSocket *sock;
	unsigned long monotonic_counter; //as stated, strictly monotonic for transmitter identification
	thread th;

	void CommTransmitter::cleanup_transmitter_list();

	static CommTransmitter* _pInstance;

public:

	CommTransmitter::CommTransmitter(const CommTransmitter&) = delete;

	static CommTransmitter& _getInstance();

	static void _destroyInstance();

	CommTransmitter::~CommTransmitter();

	std::list<string> CommTransmitter::get_connected_transmitter_ips();

	const int CommTransmitter::set_override_out_throttle(string transmitter_ip, unsigned short new_throttle);

	const int CommTransmitter::set_override_out_steer(string transmitter_ip, unsigned short new_steer);

	const int CommTransmitter::set_override_out_both(string transmitter_ip, unsigned short new_steer, unsigned short new_throttle);

	const int CommTransmitter::get_out_throttle(string transmitter_ip);

	const int CommTransmitter::get_out_steer(string transmitter_ip);

	const int CommTransmitter::get_in_steer(string transmitter_ip);

	const int CommTransmitter::get_in_throttle(string transmitter_ip);

	const int CommTransmitter::set_override_out_throttle_by_id(int transmitterId, int new_throttle);

	const int CommTransmitter::set_override_out_steer_by_id(int transmitterId, int new_steer);

	const int CommTransmitter::set_override_out_both_by_id(int transmitterId, int new_steer, int new_throttle);

	const int CommTransmitter::get_out_throttle_by_id(int transmitterId);

	const int CommTransmitter::get_out_steer_by_id(int transmitterId);

	const int CommTransmitter::get_in_steer_by_id(int transmitterId);

	const int CommTransmitter::get_in_throttle_by_id(int transmitterId);

	void CommTransmitter::run();


};
