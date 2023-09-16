#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <osmocom/core/utils.h>
#include <sys/wait.h>

#include "debug.h"
#include "pppd.h"
#include "isdnhdlc.h"
#include "yate_codec.h"
#include "yate_message.h"

// yate interfacing code inspired by Karl Koscher <supersat@cs.washington.edu>


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
#define FD_MAX 5

char yate_msg_buf[4096] = {0};
int yate_msg_buf_pos = 0;

// PPPD handling stuff
uint8_t pppd_rx_buf[2048] = {0};
uint8_t pppd_tx_buf[3000] = {0};
int pppd_fd = -1;
int pppd = 0;



void handle_incoming_hdlc_packet(uint8_t *buf, int len)
{
	fprintf(stderr, "shark I 2023-09-13T19:33:00Z\nshark 000000 %s\n\n",  osmo_hexdump(buf, len));

	// Encode the packet back into HDLC (this time byte aligned)
    int32_t hdlc_bytes_written = pppd_rfc1662_encode(buf, len, pppd_tx_buf);

	//fprintf(stderr, "PPPD W: %s\n\n",  osmo_hexdump(pppd_tx_buf, pos));

	if (write(pppd_fd, pppd_tx_buf, hdlc_bytes_written) != hdlc_bytes_written) {
		fprintf(stderr, "can't write the entire PPPD outgoing buffer!\n");
		return;
	}
}

ssize_t len;
ssize_t numSamples;
char yate_slin_samples[4096] = {0};
char yate_message_buf[4096] = {0};
uint8_t inSampleBuf[sizeof(yate_slin_samples) / 2];
uint8_t outSampleBuf[sizeof(yate_slin_samples) / 2];
fd_set in_fds;
fd_set out_fds;
fd_set err_fds;

uint8_t hdlc_rx_buf[sizeof(yate_slin_samples)] = {0}; // FIXME: size should be 2x maximum transmit packet + 2x FLAG + FCS
uint8_t hdlc_tx_buf[sizeof(yate_slin_samples)] = {0};
int hdlc_tx_buf_len = 0;
int hdlc_tx_buf_pos = 0;

void handle_sample_buffer(uint8_t *outSampleBuf, uint8_t *inSampleBuf, int numSamples)
{
	int rv, count = 0;

	int samplesProcessed = 0;	
	while (samplesProcessed < numSamples)
	{
		rv = osmo_isdnhdlc_decode(&hdlc_rx,
			inSampleBuf + samplesProcessed, numSamples - samplesProcessed, &count,
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
    while (samplesProcessed < numSamples) {
        // FIXME: This is inefficient, we're sending at most a single packet per buffer (20ms, 160 bytes)
        //if (hdlc_tx_buf_len) {
        //    //fprintf(stderr, "osmo_isdnhdlc_encode: %s\n\n",  osmo_hexdump(hdlc_tx_buf + hdlc_tx_buf_pos, hdlc_tx_buf_len - hdlc_tx_buf_pos));
        //    fprintf(stderr, "shark O 2023-09-13T19:33:00Z\nshark 000000 %s\n\n",
        //            osmo_hexdump(hdlc_tx_buf + hdlc_tx_buf_pos, hdlc_tx_buf_len - hdlc_tx_buf_pos));
        //}
        rv = osmo_isdnhdlc_encode(&hdlc_tx,
                                  (const uint8_t *) (&hdlc_tx_buf[hdlc_tx_buf_pos]), hdlc_tx_buf_len - hdlc_tx_buf_pos,
                                  &count,
                                  outSampleBuf + samplesProcessed, numSamples - samplesProcessed
        );

        if (rv < 0) {
            fprintf(stderr, "ERR TX: %d\n", rv);
        }

        if (rv > 0) {
            samplesProcessed += rv;

            //fprintf(stderr, "O %s\n\n",  osmo_hexdump(outSampleBuf, rv));
            if (hdlc_tx_buf_len) {
                hdlc_tx_buf_pos += count;
                if (hdlc_tx_buf_pos == hdlc_tx_buf_len) {
                    // packet sent successfully
                    fprintf(stderr, "packet sent successfully: hdlc_tx_buf_len: %d, count: %d\n", hdlc_tx_buf_len,
                            count);
                    hdlc_tx_buf_len = 0;
                    hdlc_tx_buf_pos = 0;
                }
            }
        }
    }
}

uint8_t temp_pack_buf[2048] = {0};
struct rfc1662_vars ppp_rfc1662_state = {0};


int main(int argc, char const *argv[])
{
	start_pppd(&pppd_fd, &pppd);
	fprintf(stderr, "fd: %d pid: %d\n", pppd_fd, pppd);

	osmo_isdnhdlc_rcv_init(&hdlc_rx, OSMO_HDLC_F_BITREVERSE);
	osmo_isdnhdlc_out_init(&hdlc_tx, OSMO_HDLC_F_BITREVERSE);

	while (!pppd || !waitpid(pppd, NULL, WNOHANG))
	{
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
			len = read(FD_YATE_STDIN, yate_message_buf, sizeof(yate_message_buf));
			if (len <= 0) {
				fprintf(stderr, "FD_YATE_STDIN read failed\n");
				break;
			}
			for (int i = 0; i < len; i++) {
				if (yate_msg_buf_pos == sizeof(yate_msg_buf) - 1)
				{
					fprintf(stderr, "Yate incoming message buffer overflowed. Aborting.\n");
					memset(yate_msg_buf, 0x00, sizeof(yate_msg_buf));
					yate_msg_buf_pos = 0;
					break;
				}

				yate_msg_buf[yate_msg_buf_pos] = yate_message_buf[i];
				yate_msg_buf_pos++;

				if (yate_message_buf[i] == '\n') {
					// process incoming message
					yate_msg_buf[yate_msg_buf_pos - 1] = 0x00;
					fprintf(stderr, "Yate incoming message: %s\n", yate_msg_buf);

                    yate_message_parse_incoming(yate_msg_buf, yate_msg_buf_pos);

					memset(yate_msg_buf, 0x00, sizeof(yate_msg_buf));
					yate_msg_buf_pos = 0;
				}
			}
		}

		if (FD_ISSET(pppd_fd, &in_fds)) 
		{
			// no packet in tx buffer? 
			if (!hdlc_tx_buf_len)
			{
				len = read(pppd_fd, pppd_rx_buf, 160);
				if (len <= 0) {
					break;
				}
				fprintf(stderr, "pppd TX (HDLC): %s\n\n",  osmo_hexdump(pppd_rx_buf, len));

                int bytes_read = 0;
                int count;
                while (bytes_read != len - 1)
                {
                    int rv = pppd_rfc1662_decode(&ppp_rfc1662_state, pppd_rx_buf + bytes_read, len - bytes_read, &count, temp_pack_buf, sizeof(temp_pack_buf));
                    bytes_read += count;

                    if (rv > 0)
                    {
                        fprintf(stderr, "rv: %d, count: %d, bytes_read: %d\n", rv, count, bytes_read);
                        fprintf(stderr, "dec: %s\n", osmo_hexdump(temp_pack_buf, rv));

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
				DLPRINTF("read an odd number of bytes!! (%d)\n", len);
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

	//kill(pppd, SIGKILL);

	return 0;
}