#include "CommTransmitter.h"

#include "PracticalSocket.h" // For UDPSocket and SocketException
#include <iostream>          // For cout and cerr

#include <inttypes.h>
#include "Crc32.h"
#include <string>
#include <chrono>
#include <map>
#include <list>
#include <mutex>

using namespace std;

mutex queue_mutex;

#define LISTEN_PORT		3333

#define TRANSMITTER_PORT		31337

#define TRANSMITTER_DELETE_AGE_MS	10
#define TRANSMITTER_DISABLE_AGE_MS	3

CommTransmitter* CommTransmitter::_pInstance = NULL;

CommTransmitter& CommTransmitter::_getInstance(){
	if (NULL == _pInstance){
		_pInstance = new CommTransmitter();
	}
	return *_pInstance;
}

void CommTransmitter::_destroyInstance(){
	delete _pInstance;
	_pInstance = NULL;
}

CommTransmitter::CommTransmitter():
	monotonic_counter(0),
	running(false),
	stop(true),
	listen_port(LISTEN_PORT),
	sock(new UDPSocket(this->listen_port)),
	th(thread(&CommTransmitter::run, this)){

}

CommTransmitter::~CommTransmitter(){
	delete this->sock;
}

list<string> CommTransmitter::get_connected_transmitter_ips(){
	list<string> connected_transmitter_ips;
	queue_mutex.lock();
	for (map <string, Transmitter>::iterator ct_iter = this->connected_transmitters.begin(); ct_iter != this->connected_transmitters.end(); ct_iter++){
		connected_transmitter_ips.push_back(ct_iter->first);
	}
	queue_mutex.unlock();
	return connected_transmitter_ips;
}

const int CommTransmitter::set_override_out_both(string transmitter_ip, unsigned short new_steer, unsigned short new_throttle){
	//adds a override packet to the queue
	queue_mutex.lock();

	if (this->connected_transmitters.find(transmitter_ip) != this->connected_transmitters.end()){
		if (this->connected_transmitters[transmitter_ip].alive){
			//new override request
			TransmitterOverride my_t_o;
			my_t_o.port = TRANSMITTER_PORT;
			my_t_o.override_throttle = true;
			my_t_o.override_steer = true;
			my_t_o.ip_address = transmitter_ip;
			my_t_o.ts_ct_packet.out_steer = new_steer;
			my_t_o.ts_ct_packet.out_throttle = new_throttle;
			this->transmitter_override_queue.push_back(my_t_o);
			queue_mutex.unlock();
			return 0;
		}
	}
	//not found...
	queue_mutex.unlock();
	return -1;
}

const int CommTransmitter::set_override_out_steer(string transmitter_ip, unsigned short new_steer){
	//adds a override packet to the queue
	queue_mutex.lock();

	if (this->connected_transmitters.find(transmitter_ip) != this->connected_transmitters.end()){
		if (this->connected_transmitters[transmitter_ip].alive){
			//new override request
			TransmitterOverride my_t_o;
			my_t_o.port = TRANSMITTER_PORT;
			my_t_o.override_steer = true;
			my_t_o.ip_address = transmitter_ip;
			my_t_o.ts_ct_packet.out_steer = new_steer;
			this->transmitter_override_queue.push_back(my_t_o);
			queue_mutex.unlock();
			return 0;
		}
	}
	//not found...
	queue_mutex.unlock();
	return -1;
}


const int CommTransmitter::set_override_out_throttle(string transmitter_ip, unsigned short new_throttle){
	//adds a override packet to the queue
	queue_mutex.lock();
	if (this->connected_transmitters.find(transmitter_ip) != this->connected_transmitters.end()){
		if (this->connected_transmitters[transmitter_ip].alive){
			//new override request
			TransmitterOverride my_t_o;
			my_t_o.port = TRANSMITTER_PORT;
			my_t_o.override_throttle = true;
			my_t_o.ip_address = transmitter_ip;
			my_t_o.ts_ct_packet.out_throttle = new_throttle;
			this->transmitter_override_queue.push_back(my_t_o);
			queue_mutex.unlock();
			return 0;
		}
	}
	//not found...
	queue_mutex.unlock();
	return -1;
}

const int CommTransmitter::get_out_throttle(string transmitter_ip){
	queue_mutex.lock();
	if (this->connected_transmitters.find(transmitter_ip) != this->connected_transmitters.end()){
		if (this->connected_transmitters[transmitter_ip].alive){
			int result = this->connected_transmitters[transmitter_ip].ts_packet.out_throttle;
			queue_mutex.unlock();
			return result;
		}
	}
	//not found...
	queue_mutex.unlock();
	return -1;
}

const int CommTransmitter::get_out_steer(string transmitter_ip){
	queue_mutex.lock();
	if (this->connected_transmitters.find(transmitter_ip) != this->connected_transmitters.end()){
		if (this->connected_transmitters[transmitter_ip].alive){
			int result = this->connected_transmitters[transmitter_ip].ts_packet.out_steer;
			queue_mutex.unlock();
			return result;
		}
	}
	//not found...
	return -1;
}

const int CommTransmitter::get_in_steer(string transmitter_ip){
	queue_mutex.lock();
	if (this->connected_transmitters.find(transmitter_ip) != this->connected_transmitters.end()){
		if (this->connected_transmitters[transmitter_ip].alive){
			int result = this->connected_transmitters[transmitter_ip].ts_packet.in_steer;
			queue_mutex.unlock();
			return result;
		}
	}
	//not found...
	queue_mutex.unlock();
	return -1;
}

const int CommTransmitter::get_in_throttle(string transmitter_ip){
	queue_mutex.lock();
	if (this->connected_transmitters.find(transmitter_ip) != this->connected_transmitters.end()){
		if (this->connected_transmitters[transmitter_ip].alive){
			int result = this->connected_transmitters[transmitter_ip].ts_packet.in_throttle;
			queue_mutex.unlock();
			return result;
		}
	}
	//not found...
	queue_mutex.unlock();
	return -1;
}


void CommTransmitter::cleanup_transmitter_list(){
	std::chrono::duration<double, std::nano> update_age;
	queue_mutex.lock();

	for (map <string, Transmitter>::iterator ct_iter = this->connected_transmitters.begin(); ct_iter != this->connected_transmitters.end(); ct_iter++){
		update_age = ct_iter->second.last_packet_received - chrono::high_resolution_clock::now();
		//cout << update_age.count() << endl;
		if (update_age.count() > TRANSMITTER_DELETE_AGE_MS){
			cout << "removing transmitter with IP: " << ct_iter->second.ip_address << endl;
			this->connected_transmitters.erase(ct_iter->second.ip_address);
			//we shortened the list, should not be a problem... but we are called soon again anyway so breaking here and wait for our next call is no problem.
			break;
		}
		if (update_age.count() > TRANSMITTER_DISABLE_AGE_MS){
			//seen no updates for TRANSMITTER_DISABLE_AGE_MS - disable and prevent showing up on public functions
			cout << "disabling transmitter with IP: " << ct_iter->second.ip_address << endl;
			ct_iter->second.alive = false;
		}
	}

	queue_mutex.unlock();
}

void CommTransmitter::run(){
	chrono::system_clock::time_point  start = chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::nano> duration;
	unsigned int count = 0;
	unsigned int crc;
	int recvMsgSize;                  // Size of received message
	string source_address;             // Address of datagram source
	unsigned short source_port;        // Port of datagram source
	this->stop = false;

	while (!this->stop){
		cout << "Hallo" << endl;
		this->running = true;
		this->cleanup_transmitter_list();

		duration = start - chrono::high_resolution_clock::now();
		try{
			recvMsgSize = this->sock->recvFrom(&this->ts_packet, sizeof(s_transmitter_state_packet), source_address, source_port);
		}
		catch (exception ex){
			cout << ex.what() << endl;
			continue;
		}
		crc = crc32_fast(&this->ts_packet, sizeof(s_transmitter_state_packet) - 4);
		if (crc == this->ts_packet.CRC){
			//cout << crc << ":" << this->ts_packet.CRC << endl;
			queue_mutex.lock();

			if (this->connected_transmitters.find(source_address) != this->connected_transmitters.end()){
				//already enlisted, update
				Transmitter &my_transmitter(this->connected_transmitters[source_address]);

				//set update time for cleanup
				my_transmitter.last_packet_received = chrono::steady_clock::now();

				//package is valid (crc checked) -> copy into live state
				memcpy(&my_transmitter.ts_packet, &this->ts_packet, sizeof(s_transmitter_state_packet));

				//in case this transmitter was sensed dead we set him back to alive
				my_transmitter.alive = true;
				//cout << my_transmitter.ip_address << ":" << (unsigned int)my_transmitter.ts_packet.in_steer << ":" << (unsigned int)my_transmitter.ts_packet.in_throttle << ":" << (unsigned int)my_transmitter.ts_packet.out_steer << ":" << (unsigned int)my_transmitter.ts_packet.out_throttle << endl;

			}
			else{
				//new transmitter showed up, add to list and create convinience pointer
				Transmitter &my_transmitter(this->connected_transmitters[source_address]);
				//set update time for cleanup
				my_transmitter.last_packet_received = chrono::steady_clock::now();

				//copy crc checked data into map
				memcpy(&my_transmitter.ts_packet, &this->ts_packet, sizeof(s_transmitter_state_packet));

				//init map data from udp package
				my_transmitter.ip_address = source_address;
				my_transmitter.last_packet_received = chrono::steady_clock::now();
				my_transmitter.monotonic_counter = this->monotonic_counter++;
				my_transmitter.port = source_port;
				//ITS ALIVE (HOHOHOHOHOHOHO)
				my_transmitter.alive = true;
				cout << "New Transmitter: " << my_transmitter.ip_address << endl;
			}

			if (!this->transmitter_override_queue.empty()){
				//override queue has stuff to do...
				TransmitterOverride &my_t_O(this->transmitter_override_queue.front());
				
				//check whether steering or throttle shall NOT be overridden - replace the unset value with the last read live value
				if (my_t_O.override_steer == false){
					//it is IN_STEER - NOT OUT_STEER - elsewise we would fix up the last sent value!!!
					//in_steer is the value read from the ADC, out_steer would be the value we sent now and from there on to forever...
					//if you don't understand this, ask. 
					my_t_O.ts_ct_packet.out_steer = this->connected_transmitters[my_t_O.ip_address].ts_packet.in_steer;
				}
				//...
				if (my_t_O.override_throttle == false){
					//as above with steer, use tha transmitters IN value
					//if you don't understand this, ask. 
					my_t_O.ts_ct_packet.out_throttle = this->connected_transmitters[my_t_O.ip_address].ts_packet.in_throttle;
				}

				my_t_O.ts_ct_packet.CRC = crc32_fast(&my_t_O.ts_ct_packet, sizeof(s_transmitter_control_packet) - 4);
				
				this->sock->sendTo(&my_t_O.ts_ct_packet, sizeof(s_transmitter_control_packet), my_t_O.ip_address, TRANSMITTER_PORT);
				//cout << (unsigned short)my_t_O.ts_ct_packet.out_steer << ":" << (unsigned short)my_t_O.ts_ct_packet.out_throttle << "(" << my_t_O.ts_ct_packet.CRC << ")" << endl;
				this->transmitter_override_queue.pop_front();


			}

			queue_mutex.unlock();
			cout << "Received packet from " << source_address << ":" << source_port << endl;
			//cout << ts_packet.in_steer << " : " << ts_packet.in_throttle << " : " << ts_packet.out_steer << " : " << ts_packet.out_throttle << endl;
		}
	}
}

