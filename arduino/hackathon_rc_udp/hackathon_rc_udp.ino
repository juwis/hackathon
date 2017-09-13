#include <esp_task_wdt.h>

#include "defines.h"
#include "comm.h"
#include "transmitter_hal.h"




void setup()
{
	Serial.begin(921600);
	start_transmitter_hal();
	init_comm(NETWORK_SSID, NETWORK_PASS, MASTER_IP, MASTER_PORT, LISTEN_PORT);
	start_comm();

}

void loop()
{

	printf("(%ld)Main Loop!\r\n", millis());
	printf("normalized: in/out steer: %d/%d\r\n", transmitter_state.in_steer, transmitter_state.out_steer);
	printf("normalized: in/out throttle: %d/%d\r\n", transmitter_state.in_throttle, transmitter_state.out_throttle);
	vTaskDelay(portTICK_PERIOD_MS * 1000);
}
