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
int minimodem_pid = -1;
int minimodem_data_in_fd = -1;
int minimodem_sample_out_fd = -1;

char yate_slin_samplebuf[4096] = {0}; // buffer for the raw signed linear audio samples
float minimodem_f32_tx_buf[sizeof(yate_slin_samplebuf) * 4]; // buffer for audio sent by minimodem (TX)
float minimodem_f32_rx_buf[sizeof(yate_slin_samplebuf) * 4];

void handle_sample_buffer(uint8_t *out_buf, uint8_t *in_buf, int num_samples)
{
	int rv, count = 0;
    // yate.c provides us with alaw data here. This used to be slin16 once, but it was already converted.

    ssize_t len = read(minimodem_sample_out_fd, (uint8_t*) minimodem_f32_tx_buf, num_samples * 4);
    if (len <= 0)
    {
        fprintf(stderr, "didn't read enough data from minimodem\n");
        return;
    }

    //fprintf(stderr, "read %d from minimodem: %s\n", num_samples, osmo_hexdump((uint8_t*)minimodem_f32_tx_buf, len));

    yate_codec_f32_to_slin(minimodem_f32_tx_buf, (uint16_t*) yate_slin_samplebuf, num_samples);
    yate_codec_slin_to_alaw((uint16_t*) yate_slin_samplebuf, out_buf, num_samples);
}

char test[8192];

int telnet_cb(struct osmo_fd *fd, unsigned int what) {
    ssize_t len;

    len = read(fd->fd, test, sizeof(test));
    if (len <= 0) {
        return -1;
    }

    write(minimodem_data_in_fd, test, len);

    return 0;
}

int minimodem_tx_cb(struct osmo_fd *fd, unsigned int what) {
    ssize_t len;

    len = read(fd->fd, test, sizeof(test));
    if (len <= 0) {
        return -1;
    }

    fprintf(stderr, "[minimodem_tx_cb] Read %zd bytes\n", len);

    //write(1, test, len);

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

    minimodem_run_tty_tx(&minimodem_pid, &minimodem_data_in_fd, &minimodem_sample_out_fd);

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