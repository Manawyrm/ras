#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <osmocom/core/select.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/isdnhdlc.h>
#include <osmocom/core/msgb.h>
#include <sys/wait.h>
#include <errno.h>

#include "pppd.h"
#include "yate_codec.h"
#include "yate_message.h"
#include "config.h"

// HDLC stuff
/* HDLC decoder state */
struct osmo_isdnhdlc_vars hdlc_rx = {0};
/* HDLC encoder state */
struct osmo_isdnhdlc_vars hdlc_tx = {0};

void *tall_ras_ctx = NULL;

// Yate handling stuff
#define FD_YATE_STDIN 0
#define FD_YATE_SAMPLE_INPUT 3
#define FD_YATE_SAMPLE_OUTPUT 4
static struct osmo_fd stdin_ofd;
static struct osmo_fd yate_sample_input_ofd;
static struct osmo_fd pppd_ofd;

// PPPD handling stuff
uint8_t pppd_tx_buf[MAX_MTU] = {0};
int pppd_fd = -1;
int pppd = 0;

char yate_slin_samples[4096] = {0}; // buffer for the raw signed linear audio samples
uint8_t inSampleBuf[sizeof(yate_slin_samples) / 2];  // buffer for data coming from the ISDN line
uint8_t outSampleBuf[sizeof(yate_slin_samples) / 2]; // buffer for data going out to the ISDN line

uint8_t hdlc_rx_buf[MAX_MTU * 2] = {0}; // buffer for data decoded from ISDN HDLC frames
struct llist_head isdn_hdlc_tx_queue;

void handle_incoming_hdlc_packet(uint8_t *buf, int buf_len)
{
	// Encode the packet back into HDLC (this time byte aligned)
    int32_t hdlc_bytes_written = pppd_rfc1662_encode(buf, buf_len, pppd_tx_buf);

	if (write(pppd_fd, pppd_tx_buf, hdlc_bytes_written) != hdlc_bytes_written) {
		fprintf(stderr, "can't write the entire PPPD outgoing buffer!\n");
		return;
	}
}

void handle_incoming_ppp_packet(uint8_t *buf, int len)
{
    struct msgb *msg;
    uint8_t *ptr;

    msg = msgb_alloc_c(tall_ras_ctx, len, "pppd receive/isdn hdlc transmit");
    ptr = msgb_put(msg, len);
    memcpy(ptr, buf, len);
    msgb_enqueue(&isdn_hdlc_tx_queue, msg);
}

struct msgb *hdlc_tx_msgb = NULL;
uint8_t *hdlc_tx_buf;
int hdlc_tx_buf_pos;
int hdlc_tx_buf_len;

void handle_sample_buffer(uint8_t *out_buf, uint8_t *in_buf, int num_samples)
{
	int rv, count = 0;

	int samplesProcessed = 0;	
	while (samplesProcessed < num_samples)
	{
		rv = osmo_isdnhdlc_decode(&hdlc_rx,
                                  in_buf + samplesProcessed, num_samples - samplesProcessed, &count,
			hdlc_rx_buf, sizeof(hdlc_rx_buf) - 3
		);

		if (rv > 0) {
			handle_incoming_hdlc_packet(hdlc_rx_buf, rv);
		} else if (rv < 0) {
			switch (rv) {
				case -OSMO_HDLC_FRAMING_ERROR:
					fprintf(stderr, "OSMO_HDLC_FRAMING_ERROR\n");
					break;
				case -OSMO_HDLC_LENGTH_ERROR:
					fprintf(stderr, "OSMO_HDLC_LENGTH_ERROR\n");
					break;
				case -OSMO_HDLC_CRC_ERROR:
					fprintf(stderr, "OSMO_HDLC_CRC_ERROR\n");
					break;
			}
		}
		samplesProcessed += count;
	}

	// send packets
    samplesProcessed = 0;
    while (samplesProcessed < num_samples)
    {
        // is there still a packet being transmitted?
        if (!hdlc_tx_buf_len)
        {
            // get a new one from the queue
            hdlc_tx_msgb = msgb_dequeue(&isdn_hdlc_tx_queue);
            if (hdlc_tx_msgb != NULL)
            {
                // got one
                hdlc_tx_buf = msgb_data(hdlc_tx_msgb);
                hdlc_tx_buf_len = msgb_length(hdlc_tx_msgb);
                hdlc_tx_buf_pos = 0;
            }
        }
        rv = osmo_isdnhdlc_encode(&hdlc_tx,
                                  (const uint8_t *) (hdlc_tx_buf + hdlc_tx_buf_pos), hdlc_tx_buf_len - hdlc_tx_buf_pos,
                                  &count,
                                  out_buf + samplesProcessed, num_samples - samplesProcessed
        );

        if (rv < 0) {
            fprintf(stderr, "ERR TX: %d\n", rv);
        }

        if (rv > 0) {
            samplesProcessed += rv;

            if (hdlc_tx_buf_len) {
                hdlc_tx_buf_pos += count;
                if (hdlc_tx_buf_pos == hdlc_tx_buf_len)
                {
                    // finished sending packet
                    hdlc_tx_buf_len = 0;
                    hdlc_tx_buf_pos = 0;
                    msgb_free(hdlc_tx_msgb);
                }
            }
        }
    }
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

    handle_sample_buffer(outSampleBuf, inSampleBuf, numSamples);

    // echo test:
    //memcpy(outSampleBuf, inSampleBuf, numSamples);

    yate_codec_alaw_to_slin(outSampleBuf, (uint16_t *) yate_slin_samples, numSamples);

    if (write(FD_YATE_SAMPLE_OUTPUT, yate_slin_samples, len) != len) {
        fprintf(stderr, "can't write the entire outgoing buffer!\n");
        return -1;
    }

    return 0;
}

int main(int argc, char const *argv[])
{
    tall_ras_ctx = talloc_named_const(NULL, 1, "RAS context");
    if (!tall_ras_ctx)
        return -ENOMEM;
    msgb_talloc_ctx_init(tall_ras_ctx, 0);
    INIT_LLIST_HEAD(&isdn_hdlc_tx_queue);

    start_pppd(&pppd_fd, &pppd);
	fprintf(stderr, "pppd started, fd: %d pid: %d\n", pppd_fd, pppd);

	osmo_isdnhdlc_rcv_init(&hdlc_rx, OSMO_HDLC_F_BITREVERSE);
	osmo_isdnhdlc_out_init(&hdlc_tx, OSMO_HDLC_F_BITREVERSE);

    int loop_cycles = 0;

    // stdin is used for Yates message/event bus
    osmo_fd_setup(&stdin_ofd, FD_YATE_STDIN, OSMO_FD_READ | OSMO_FD_EXCEPT, yate_message_read_cb, NULL, 0);
    osmo_fd_register(&stdin_ofd);

    // sample input from Yate
    osmo_fd_setup(&yate_sample_input_ofd, FD_YATE_SAMPLE_INPUT, OSMO_FD_READ | OSMO_FD_EXCEPT, yate_sample_input_cb, NULL, 0);
    osmo_fd_register(&yate_sample_input_ofd);

    // transmitted frames from pppd
    osmo_fd_setup(&pppd_ofd, pppd_fd, OSMO_FD_READ | OSMO_FD_EXCEPT, pppd_input_cb, &handle_incoming_ppp_packet, 0);
    osmo_fd_register(&pppd_ofd);

    while (pppd)
    {
        // we don't want to send the waitpid syscall on each loop
        loop_cycles++;
        if (loop_cycles % 100 == 0)
        {
            // if the pppd process has exited, end our call too
            if (waitpid(pppd, NULL, WNOHANG) > 0)
            {
                fprintf(stderr, "pppd (pid: %d) has died. Exiting yate_hdlc_ppp.\n", pppd);
                pppd = 0;
            }
        }

        osmo_select_main(0);
    }

    talloc_report_full(tall_ras_ctx, stderr);
    talloc_free(tall_ras_ctx);

    return 0;
}