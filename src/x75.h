#pragma once
#include <stdint.h>

void x75_init(void *isdn_packet_tx_cb);
void x75_recv(uint8_t *buf, int len);
