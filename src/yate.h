#pragma once
#include <osmocom/core/select.h>

void yate_codec_slin_to_alaw(uint16_t *inSampleBuf, uint8_t *outSampleBuf, uint32_t numSamples);
void yate_codec_alaw_to_slin(uint8_t *inSampleBuf, uint16_t *outSampleBuf, uint32_t numSamples);
int yate_sample_input_cb(struct osmo_fd *fd, unsigned int what);
void yate_osmo_fd_register(void *handle_sample_buffer_cb);
