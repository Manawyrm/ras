#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/isdnhdlc.h>
#include <sys/wait.h>
#include "debug.h"
#include "pppd.h"
#include "yate_codec.h"
#include "yate_message.h"

// HDLC stuff
/* HDLC decoder state */
struct osmo_isdnhdlc_vars hdlc_rx = {0};
/* HDLC encoder state */
struct osmo_isdnhdlc_vars hdlc_tx = {0};

// Yate handling stuff
#define FD_YATE_STDIN 0
#define FD_YATE_STDOUT 1
#define FD_YATE_STDERR 2
#define FD_YATE_SAMPLE_INPUT 3
#define FD_YATE_SAMPLE_OUTPUT 4

// maximum packet size allowed over the PPP
// (normal IP packets are 1500)
#define MAX_MTU 2048

// PPPD handling stuff
uint8_t pppd_rx_buf[MAX_MTU] = {0};
uint8_t pppd_tx_buf[MAX_MTU] = {0};
int pppd_fd = -1;
int pppd = 0;

char yate_slin_samples[4096] = {0}; // buffer for the raw signed linear audio samples
uint8_t inSampleBuf[sizeof(yate_slin_samples) / 2];  // buffer for data coming from the ISDN line
uint8_t outSampleBuf[sizeof(yate_slin_samples) / 2]; // buffer for data going out to the ISDN line
fd_set in_fds;
fd_set out_fds;
fd_set err_fds;

uint8_t hdlc_rx_buf[MAX_MTU * 2] = {0}; // buffer for data decoded from ISDN HDLC frames
uint8_t hdlc_tx_buf[MAX_MTU * 2] = {0}; // TX buffer for data to be sent via ISDN HDLC
int hdlc_tx_buf_len = 0;
int hdlc_tx_buf_pos = 0;

void handle_incoming_hdlc_packet(uint8_t *buf, int buf_len)
{
	//fprintf(stderr, "shark I 2023-09-13T19:33:00Z\nshark 000000 %s\n\n",  osmo_hexdump(buf, len));

	// Encode the packet back into HDLC (this time byte aligned)
    int32_t hdlc_bytes_written = pppd_rfc1662_encode(buf, buf_len, pppd_tx_buf);

	if (write(pppd_fd, pppd_tx_buf, hdlc_bytes_written) != hdlc_bytes_written) {
		fprintf(stderr, "can't write the entire PPPD outgoing buffer!\n");
		return;
	}
}

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
		} else  if (rv < 0) {
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
    while (samplesProcessed < num_samples) {
        // FIXME: This is inefficient, we're sending at most a single packet per buffer (20ms, 160 bytes)
        rv = osmo_isdnhdlc_encode(&hdlc_tx,
                                  (const uint8_t *) (&hdlc_tx_buf[hdlc_tx_buf_pos]), hdlc_tx_buf_len - hdlc_tx_buf_pos,
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
                if (hdlc_tx_buf_pos == hdlc_tx_buf_len) {
                    // packet sent successfully
                    //fprintf(stderr, "packet sent successfully: hdlc_tx_buf_len: %d, count: %d\n", hdlc_tx_buf_len, count);
                    hdlc_tx_buf_len = 0;
                    hdlc_tx_buf_pos = 0;
                }
            }
        }
    }
}

// PPP sends HDLC(-ish, RFC1662) encoded data.
uint8_t temp_pack_buf[2048] = {0};
struct rfc1662_vars ppp_rfc1662_state = {0};

int main(int argc, char const *argv[])
{
    ssize_t len;
    ssize_t numSamples;

    start_pppd(&pppd_fd, &pppd);
	fprintf(stderr, "pppd started, fd: %d pid: %d\n", pppd_fd, pppd);

	osmo_isdnhdlc_rcv_init(&hdlc_rx, OSMO_HDLC_F_BITREVERSE);
	osmo_isdnhdlc_out_init(&hdlc_tx, OSMO_HDLC_F_BITREVERSE);

    int loop_cycles = 0;

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

		FD_ZERO(&in_fds);
		FD_ZERO(&out_fds);
		FD_ZERO(&err_fds);

		FD_SET(FD_YATE_STDIN, &in_fds);
		FD_SET(FD_YATE_SAMPLE_INPUT, &in_fds);
		FD_SET(pppd_fd, &in_fds);

		FD_SET(FD_YATE_STDOUT, &out_fds);
		FD_SET(FD_YATE_SAMPLE_OUTPUT, &out_fds);

		for (int i = 0; i < 5; i++) {
			FD_SET(i, &err_fds);
		}
		FD_SET(pppd_fd, &err_fds);

		if (select(pppd_fd + 1, &in_fds, NULL, &err_fds, NULL) <= 0) {
			fprintf(stderr, "select failed\n");
			break;
		}

		for (int i = 0; i <= pppd_fd; i++) {
			if (FD_ISSET(i, &err_fds)) {
				fprintf(stderr, "fd %d is exceptional\n", i);
				break;
			}
		}

		// Yate will send us messages through STDIN
		if (FD_ISSET(FD_YATE_STDIN, &in_fds)) {
            yate_message_read_from_fd(FD_YATE_STDIN, stdout);
		}

        // Data being sent from PPPD
		if (FD_ISSET(pppd_fd, &in_fds)) 
		{
			// no packet in tx buffer? 
			if (!hdlc_tx_buf_len)
			{
				len = read(pppd_fd, pppd_rx_buf, 160);
				if (len <= 0) {
					break;
				}
				//fprintf(stderr, "pppd TX (HDLC): %s\n\n",  osmo_hexdump(pppd_rx_buf, len));

                int bytes_read = 0;
                int count;
                while (bytes_read != len - 1)
                {
                    int rv = pppd_rfc1662_decode(&ppp_rfc1662_state, pppd_rx_buf + bytes_read, len - bytes_read, &count, temp_pack_buf, sizeof(temp_pack_buf));
                    bytes_read += count;

                    if (rv > 0)
                    {
                        //fprintf(stderr, "rv: %d, count: %d, bytes_read: %d\n", rv, count, bytes_read);
                        //fprintf(stderr, "dec: %s\n", osmo_hexdump(temp_pack_buf, rv));

                        memcpy(hdlc_tx_buf, temp_pack_buf, rv);
                        hdlc_tx_buf_len = rv;
                    }
                }
            }
		}

		// Yate will send us samples via FD 3
		if (FD_ISSET(FD_YATE_SAMPLE_INPUT, &in_fds)) {
			len = read(FD_YATE_SAMPLE_INPUT, yate_slin_samples, sizeof(yate_slin_samples));
			if (len <= 0) {
				break;
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
				break;
			}
		}

	}

	return 0;
}