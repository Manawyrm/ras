#pragma once
#include <stdint.h>

int telnet_init(void *telnet_rx_cb, char* hostname, uint16_t port);
