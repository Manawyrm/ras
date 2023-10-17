#pragma once
#include <stdint.h>
#include <osmocom/core/gsmtap.h>

int gsmtap_init(const char *gsmtap_host);
int gsmtap_send_packet(uint8_t sub_type, bool network_to_user, const uint8_t *data, unsigned int len);
int gsmtap_send_rlp(bool network_to_user, const uint8_t *data, unsigned int len);