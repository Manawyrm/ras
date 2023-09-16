#include <stdint.h>

#include "yate_codec.h"
#include "a2s.h"

static unsigned char s2a[65536] = {0};
void yate_codec_alaw_slin_init()
{
    // yate is sadly assuming our data call is audio and is
    // converting our nice bits from "8bit alaw" into "16bit signed linear"
    // this is not helpful, but at least it's lossless, so we can
    // just revert this process by using the exact same code yate is
    // using.
    int i;
    unsigned char val;
    unsigned char v;
    // positive side of A-Law
    for (i = 0, v = 0, val = 0xd5; i <= 32767; i++) {
        if ((v < 0x7f) && ((i - 8) >= (int)(unsigned int)a2s[val]))
            val = (++v) ^ 0xd5;
        s2a[i] = val;
    }
    // negative side of A-Law
    for (i = 32768, v = 0xff, val = 0x2a; i <= 65535; i++) {
        if ((v > 0x80) && ((i - 8) >= (int)(unsigned int)a2s[val]))
            val = (--v) ^ 0xd5;
        s2a[i] = val;
    }
}

void yate_codec_slin_to_alaw(uint16_t *inSampleBuf, uint8_t *outSampleBuf, uint32_t numSamples)
{
    // generate lookup table on first conversion
    if (s2a[0] == 0)
    {
        yate_codec_alaw_slin_init();
    }

    // convert the "signed linear PCM" back to "alaw" (so we get our real bits back)
    for (uint32_t i = 0; i < numSamples; i++)
    {
        outSampleBuf[i] = s2a[ inSampleBuf[i] ];
    }
}

void yate_codec_alaw_to_slin(uint8_t *inSampleBuf, uint16_t *outSampleBuf, uint32_t numSamples)
{
    for (uint32_t i = 0; i < numSamples; i++)
    {
        ((int16_t *)outSampleBuf)[i] = a2s[ inSampleBuf[i] ];
    }
}