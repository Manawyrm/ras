// ITU V.18 - Operational and interworking requirements for DCEs operating in the text telephone mode
// TTY/TDD, 45.45 baud, 1400/1800 Hz, 400 Hz shift, 5-bit baudot, half-duplex

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <osmocom/core/select.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/talloc.h>

#include <errno.h>
#include "minimodem.h"
#include "yate.h"

// Talloc context
void *tall_ras_ctx = NULL;

static struct osmo_fd telnet_fd;

// Minimodem TX -> alaw OUT
int minimodem_tx_pid = -1;
int minimodem_data_in_fd = -1;
int minimodem_sample_out_fd = -1;

// alaw IN -> Minimodem RX
int minimodem_rx_pid = -1;
int minimodem_sample_in_fd = -1;
static struct osmo_fd minimodem_data_out_ofd;
int minimodem_data_out_fd = -1;

char yate_slin_samplebuf[4096] = {0}; // buffer for the raw signed linear audio samples
float minimodem_f32_tx_buf[sizeof(yate_slin_samplebuf) * 4]; // buffer for audio sent by minimodem (TX)
float minimodem_f32_rx_buf[sizeof(yate_slin_samplebuf) * 4];

void handle_sample_buffer(uint8_t *out_buf, uint8_t *in_buf, int num_samples)
{
    // yate.c provides us with alaw data here. This used to be slin16 once, but it was already converted.
    // alaw IN -> Minimodem RX
    yate_codec_alaw_to_slin(in_buf, (uint16_t*) yate_slin_samplebuf, num_samples);
    yate_codec_slin_to_f32((uint16_t*) yate_slin_samplebuf, minimodem_f32_rx_buf, num_samples);
    write(minimodem_sample_in_fd, (uint8_t*) minimodem_f32_rx_buf, num_samples * 4);

    // Minimodem TX -> alaw OUT
    ssize_t len = read(minimodem_sample_out_fd, (uint8_t*) minimodem_f32_tx_buf, num_samples * 4);
    if (len <= 0)
    {
        fprintf(stderr, "didn't read enough data from minimodem\n");
        return;
    }
    yate_codec_f32_to_slin(minimodem_f32_tx_buf, (uint16_t*) yate_slin_samplebuf, num_samples);
    yate_codec_slin_to_alaw((uint16_t*) yate_slin_samplebuf, out_buf, num_samples);
}

char text_buf[8192];

int telnet_cb(struct osmo_fd *fd, unsigned int what) {
    ssize_t len;

    len = read(fd->fd, text_buf, sizeof(text_buf));
    if (len <= 0) {
        return -1;
    }

    write(minimodem_data_in_fd, text_buf, len);

    return 0;
}

int minimodem_rx_cb(struct osmo_fd *fd, unsigned int what) {
    ssize_t len;

    len = read(fd->fd, text_buf, sizeof(text_buf));
    if (len <= 0) {
        return -1;
    }

    write(telnet_fd.fd, text_buf, len);
    return 0;
}


int main(int argc, char const *argv[])
{
    tall_ras_ctx = talloc_named_const(NULL, 1, "RAS context");
    if (!tall_ras_ctx)
        return -ENOMEM;
    msgb_talloc_ctx_init(tall_ras_ctx, 0);

    int loop_cycles = 0;

    int rc = -1;
    telnet_fd.cb = telnet_cb;
    telnet_fd.when = OSMO_FD_READ | OSMO_FD_EXCEPT;
    rc = osmo_sock_init_ofd(&telnet_fd, AF_INET,
                            SOCK_STREAM, IPPROTO_TCP,
                            "communityisdn.tbspace.de", 13097,
                            OSMO_SOCK_F_CONNECT);

    if (rc < 0)
    {
        fprintf(stderr, "Connection could not be established\n");
        exit(1);
    }
    else
    {
        fprintf(stderr, "Connection established\n");
    }

    minimodem_run_tty_tx(&minimodem_tx_pid, &minimodem_data_in_fd, &minimodem_sample_out_fd);
    minimodem_run_tty_rx(&minimodem_rx_pid, &minimodem_sample_in_fd, &minimodem_data_out_fd);

    osmo_fd_setup(&minimodem_data_out_ofd, minimodem_data_out_fd, OSMO_FD_READ | OSMO_FD_EXCEPT, minimodem_rx_cb, NULL, 0);
    osmo_fd_register(&minimodem_data_out_ofd);

    // register yate onto STDIN and FD3 (sample input)
    yate_osmo_fd_register(&handle_sample_buffer);

    while (true)
    {
        osmo_select_main(0);
    }

    talloc_report_full(tall_ras_ctx, stderr);
    talloc_free(tall_ras_ctx);

    return 0;
}