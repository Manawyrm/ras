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
#include "yate.h"
#include "config.h"
#include "gsmtap.h"

// submitted to upstream, https://gerrit.osmocom.org/c/libosmocore/+/34457
// delete this line after merge/libosmocore update
#ifndef GSMTAP_E1T1_PPP
    #define GSMTAP_E1T1_PPP		0x0b	/* PPP */
#endif

/* HDLC decoder state */
struct osmo_isdnhdlc_vars hdlc_rx = {0};
/* HDLC encoder state */
struct osmo_isdnhdlc_vars hdlc_tx = {0};

// Talloc context (for tx queue buffer allocations)
void *tall_ras_ctx = NULL;

// PPPD handling
int pppd_fd = -1;
int pppd = 0;
static struct osmo_fd pppd_ofd;

uint8_t hdlc_rx_buf[MAX_MTU * 2] = {0}; // buffer for data decoded from ISDN HDLC frames
struct llist_head isdn_hdlc_tx_queue;

void handle_incoming_ppp_packet(uint8_t *buf, int len)
{
    struct msgb *msg;
    uint8_t *ptr;

    msg = msgb_alloc_c(tall_ras_ctx, len, "pppd receive/isdn hdlc transmit");

    // PPP in HDLC-ish sometimes has a FF03 (Address & Control) header. Wireshark can't handle that, let's skip it.
    if (buf[0] == 0xFF && buf[1] == 0x03)
    {
        gsmtap_send_packet(GSMTAP_E1T1_PPP, true, buf + 2, len - 2);
    }
    else
    {
        gsmtap_send_packet(GSMTAP_E1T1_PPP, true, buf, len);
    }

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
			hdlc_rx_buf, sizeof(hdlc_rx_buf) - 5
		);

		if (rv > 0) {
            // PPP sometimes uses a FF03 (Address & Control) header. Wireshark can't handle that, let's skip it.
            if (rv >= 2 && hdlc_rx_buf[0] == 0xFF && hdlc_rx_buf[1] == 0x03)
            {
                gsmtap_send_packet(GSMTAP_E1T1_PPP, false, hdlc_rx_buf + 2, rv - 2);
            }
            else
            {
                gsmtap_send_packet(GSMTAP_E1T1_PPP, false, hdlc_rx_buf, rv);
            }

            pppd_input_raw_packet(pppd_fd, hdlc_rx_buf, rv);
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

int main(int argc, char const *argv[])
{
    tall_ras_ctx = talloc_named_const(NULL, 1, "RAS context");
    if (!tall_ras_ctx)
        return -ENOMEM;
    msgb_talloc_ctx_init(tall_ras_ctx, 0);
    INIT_LLIST_HEAD(&isdn_hdlc_tx_queue);

    gsmtap_init("::1");

    start_pppd(&pppd_fd, &pppd);
	fprintf(stderr, "pppd started, fd: %d pid: %d\n", pppd_fd, pppd);

	osmo_isdnhdlc_rcv_init(&hdlc_rx, OSMO_HDLC_F_BITREVERSE);
	osmo_isdnhdlc_out_init(&hdlc_tx, OSMO_HDLC_F_BITREVERSE);

    int loop_cycles = 0;

    // register yate onto STDIN and FD3 (sample input)
    yate_osmo_fd_register(&handle_sample_buffer, NULL);

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