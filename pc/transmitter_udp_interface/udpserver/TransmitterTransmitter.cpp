/*
 *   C++ sockets on Unix and Windows
 *   Copyright (C) 2002
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>          // For cout and cerr
#include <string>


#include "CommTransmitter.h"
#include "threadpool.h"



int main(int argc, char *argv[]) {

	// create thread pool with 4 worker threads
	ThreadPool pool(4);




	CommTransmitter &myTransmitter(CommTransmitter::_getInstance());
	// enqueue and store future
	std::future<void> result;
	result = pool.enqueue([&myTransmitter]() {
		myTransmitter.run();
	});
	uint8_t new_steer = 0;
	while (true){

		cout << "|  102 get_in_steer: " << myTransmitter.get_in_steer("192.168.0.102") ;
		cout << "|  102 get_in_throttle: " << myTransmitter.get_in_throttle("192.168.0.102");
		cout << "|  103 get_in_steer: " << myTransmitter.get_in_steer("192.168.0.103") ;
		cout << "|  103 get_in_throttle: " << myTransmitter.get_in_throttle("192.168.0.103") << endl;
		_sleep(50);
	}

	return 0;
}
