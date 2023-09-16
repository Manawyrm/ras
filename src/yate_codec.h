#pragma once

void yate_codec_slin_to_alaw(uint16_t *inSampleBuf, uint8_t *outSampleBuf, uint32_t numSamples);
void yate_codec_alaw_to_slin(uint8_t *inSampleBuf, uint16_t *outSampleBuf, uint32_t numSamples);
