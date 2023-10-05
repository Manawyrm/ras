#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "yate.h"
#include "yate_message.h"
#include "a2s.h"

#define FD_YATE_STDIN 0
#define FD_YATE_SAMPLE_INPUT 3
#define FD_YATE_SAMPLE_OUTPUT 4

char yate_slin_samples[4096] = {0}; // buffer for the raw signed linear audio samples
uint8_t inSampleBuf[sizeof(yate_slin_samples) / 2];  // buffer for data coming from the ISDN line
uint8_t outSampleBuf[sizeof(yate_slin_samples) / 2]; // buffer for data going out to the ISDN line

static struct osmo_fd stdin_ofd;
static struct osmo_fd yate_sample_input_ofd;

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

void yate_codec_slin_to_alaw(uint16_t *in, uint8_t *out, uint32_t numSamples)
{
    // generate lookup table on first conversion
    if (s2a[0] == 0)
    {
        yate_codec_alaw_slin_init();
    }

    // convert the "signed linear PCM" back to "alaw" (so we get our real bits back)
    for (uint32_t i = 0; i < numSamples; i++)
    {
        out[i] = s2a[ in[i] ];
    }
}

void yate_codec_alaw_to_slin(uint8_t *in, uint16_t *out, uint32_t numSamples)
{
    for (uint32_t i = 0; i < numSamples; i++)
    {
        ((int16_t *)out)[i] = a2s[ in[i] ];
    }
}

void yate_codec_slin_to_f32(uint16_t *in, float *out, uint32_t numSamples)
{
    for (uint32_t i = 0; i < numSamples; i++)
    {
        out[i] = (float) in[i] / 32768.0f;
    }
}

void yate_codec_f32_to_slin(float *in, uint16_t *out, uint32_t numSamples)
{
    for (uint32_t i = 0; i < numSamples; i++)
    {
        ((int16_t *)out)[i] = (in[i] * 32768.0f);
    }
}

void yate_osmo_fd_register(void *handle_sample_buffer_cb, void *handle_yate_parameters_cb)
{
    // stdin is used for Yates message/event bus
    osmo_fd_setup(&stdin_ofd, FD_YATE_STDIN, OSMO_FD_READ | OSMO_FD_EXCEPT, yate_message_read_cb, handle_yate_parameters_cb, 0);
    osmo_fd_register(&stdin_ofd);

    // sample input from Yate
    osmo_fd_setup(&yate_sample_input_ofd, FD_YATE_SAMPLE_INPUT, OSMO_FD_READ | OSMO_FD_EXCEPT, yate_sample_input_cb, handle_sample_buffer_cb, 0);
    osmo_fd_register(&yate_sample_input_ofd);
}

int yate_sample_input_cb(struct osmo_fd *fd, unsigned int what)
{
    ssize_t len;
    ssize_t numSamples;
    len = read(fd->fd, yate_slin_samples, sizeof(yate_slin_samples));
    if (len <= 0) {
        return -1;
    }
    if (len % 2) {
        fprintf(stderr, "read an odd number of bytes as samples from yate!! (%ld)\n", len);
    }
    numSamples = len / 2;

    yate_codec_slin_to_alaw((uint16_t*) yate_slin_samples, inSampleBuf, numSamples);

    void (*handle_sample_buffer)(uint8_t *, uint8_t *, int) = fd->data;
    (*handle_sample_buffer)(outSampleBuf, inSampleBuf, numSamples);

    // echo test:
    //memcpy(outSampleBuf, inSampleBuf, numSamples);

    yate_codec_alaw_to_slin(outSampleBuf, (uint16_t *) yate_slin_samples, numSamples);

    if (write(FD_YATE_SAMPLE_OUTPUT, yate_slin_samples, len) != len) {
        fprintf(stderr, "can't write the entire outgoing buffer!\n");
        return -1;
    }

    return 0;
}
