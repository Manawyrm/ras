#pragma once
#include <osmocom/core/select.h>

void yate_codec_slin_to_alaw(uint16_t *in, uint8_t *out, uint32_t numSamples);
void yate_codec_alaw_to_slin(uint8_t *in, uint16_t *out, uint32_t numSamples);
void yate_codec_slin_to_f32(uint16_t *in, float *out, uint32_t numSamples);
void yate_codec_f32_to_slin(float *in, uint16_t *out, uint32_t numSamples);

int yate_sample_input_cb(struct osmo_fd *fd, unsigned int what);
void yate_osmo_fd_register(void *handle_sample_buffer_cb, void *handle_yate_parameters_cb);