#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <osmocom/core/utils.h>

#include "isdnhdlc.h"
#include "a2s.h"

#define DLPRINTF(args...) fprintf(stderr, args)

// HDLC stuff 

/* HDLC decoder state */
struct osmo_isdnhdlc_vars hdlc_rx;

/* HDLC encoder state */
struct osmo_isdnhdlc_vars hdlc_tx;

/* PPP HDLC encoder state */
struct osmo_isdnhdlc_vars ppp_hdlc_tx;

// Sample conversion stuff  

// yate interfacing code inspired by Karl Koscher <supersat@cs.washington.edu>
typedef uint8_t samp_t;

static unsigned char s2a[65536];
void alaw_slin_init()
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

// Yate handling stuff
#define FD_YATE_STDIN 0
#define FD_YATE_STDOUT 1
#define FD_YATE_STDERR 2
#define FD_YATE_SAMPLE_INPUT 3
#define FD_YATE_SAMPLE_OUTPUT 4
#define FD_MAX 5

char yate_msg_buf[4096];
int yate_msg_buf_pos = 0;

// PPPD handling stuff
uint8_t pppd_rx_buf[2048];
uint8_t pppd_tx_buf[3000];
int pppd_fd = -1;
int pppd = 0;
#define fcstab  ppp_crc16_table

#define PPP_FLAG 0x7e
#define PPP_ESCAPE 0x7d
#define PPP_TRANS 0x20

#define PPP_INITFCS 0xffff
#define PPP_GOODFCS 0xf0b8
#define PPP_FCS(fcs,c) (((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

uint16_t ppp_crc16_table[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

void yate_parse_incoming_message(char *buf, int len)
{
	// we don't want to really talk to yate (just yet)
	// let's just answer it's execute call to make it happy.

	// %%>message:0x7f1882d68b10.476653321:1694630261:call.execute::id=sig/2:module=sig:status=answered:address=auerswald/1:billid=1694630023-3:lastpeerid=wave/2:answered=true:direction=incoming:callto=external/playrec//root/yate/share/scripts/x75/x75:handlers=javascript%z15,javascript%z15,gvoice%z20,queues%z45,cdrbuild%z50,yrtp%z50,lateroute%z75,dbwave%z90,filetransfer%z90,conf%z90,jingle%z90,tone%z90,wave%z90,iax%z90,sip%z90,sig%z90,dumb%z90,analyzer%z90,mgcpgw%z90,analog%z90,callgen%z100,pbx%z100,extmodule%z100
	char *message_id = memchr(buf, ':', len);
	if(message_id == NULL)
	{
		return;
	}
	// we don't want the first char (:)
	message_id += 1;

	// %%>message:0x7f1882d68b10.1202973640:1694630927:call.execute::id=sig/7:module=sig:status=answered:address=auerswald/1:billid=1694630023-13:lastpeerid=wave/7:answered=true:direction=incoming:callto=external/playrec//root/yate/share/scripts/x75/x75:handlers=javascript%z15,javascript%z15,gvoice%z20,queues%z45,cdrbuild%z50,yrtp%z50,lateroute%z75,dbwave%z90,filetransfer%z90,conf%z90,jingle%z90,tone%z90,wave%z90,iax%z90,sip%z90,sig%z90,dumb%z90,analyzer%z90,mgcpgw%z90,analog%z90,callgen%z100,pbx%z100,extmodule%z100
	char *message_id_end = memchr(message_id, ':', len - (message_id - buf));
	if(message_id_end == NULL)
	{
		return;
	}
	message_id_end[0] = 0x00;
	fprintf(stderr, "%%%%<message:%s:true:\n", message_id);
	fprintf(stdout, "%%%%<message:%s:true:\n", message_id);
	fflush(stdout);
}

void handle_incoming_hdlc_packet(uint8_t *buf, int len)
{
	fprintf(stderr, "I 2023-09-13T19:33:00Z\n000000 %s\n\n",  osmo_hexdump(buf, len));

	// Encode the packet back into HDLC (this time byte aligned)
	int pos = 0;
	unsigned char e;

	// calculate checksum
	uint16_t fcs = PPP_INITFCS;
    unsigned char *c = buf;
    for (int x = 0; x < len; x++)
    {
        fcs = PPP_FCS (fcs, *c);
        c++;
    }
    fcs = fcs ^ 0xFFFF;
    *c = fcs & 0xFF;
    c++;
    *c = (fcs >> 8) & 0xFF;
    len += 2;

	// add flags
	pppd_tx_buf[pos++] = PPP_FLAG;
	for (int x = 0; x < len; x++)
	{
		e = buf[x];
		if ((e < 0x20) || (e == PPP_ESCAPE) || (e == PPP_FLAG))
		{
			/* Escape this */
			e = e ^ 0x20;
			pppd_tx_buf[pos++] = PPP_ESCAPE;
		}
		pppd_tx_buf[pos++] = e;
	}
	pppd_tx_buf[pos++] = PPP_FLAG;

	//fprintf(stderr, "PPPD W: %s\n\n",  osmo_hexdump(pppd_tx_buf, pos));

	if (write(pppd_fd, pppd_tx_buf, pos) != pos) {
		fprintf(stderr, "can't write the entire PPPD outgoing buffer!\n");
		return;
	}
}

uint8_t hdlc_rx_buf[2048];
uint8_t hdlc_tx_buf[2048];
int hdlc_tx_buf_len = 0;
int hdlc_tx_buf_pos = 0;

void handle_sample_buffer(uint8_t *outSampleBuf, uint8_t *inSampleBuf, int numSamples)
{
	int rv, count = 0;

	int samplesProcessed = 0;	
	while (samplesProcessed < numSamples)
	{
		rv = osmo_isdnhdlc_decode(&hdlc_rx,
			inSampleBuf, numSamples, &count,
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
	// FIXME: This is inefficient, we're sending at most a single packet per buffer (20ms, 160 bytes)
	if (hdlc_tx_buf_len)
	{
		//fprintf(stderr, "osmo_isdnhdlc_encode: %s\n\n",  osmo_hexdump(hdlc_tx_buf + hdlc_tx_buf_pos, hdlc_tx_buf_len - hdlc_tx_buf_pos));
		fprintf(stderr, "O 2023-09-13T19:33:00Z\n000000 %s\n\n",  osmo_hexdump(hdlc_tx_buf + hdlc_tx_buf_pos, hdlc_tx_buf_len - hdlc_tx_buf_pos));
	}
	rv = osmo_isdnhdlc_encode(&hdlc_tx,
		&hdlc_tx_buf + hdlc_tx_buf_pos, hdlc_tx_buf_len - hdlc_tx_buf_pos, &count,
		outSampleBuf, numSamples
	);

	if (rv < 0)
	{
		fprintf(stderr, "ERR TX: %d\n", rv);
	}

	if (rv > 0)
	{
		//fprintf(stderr, "O %s\n\n",  osmo_hexdump(outSampleBuf, rv));
		hdlc_tx_buf_pos += count;
		if (hdlc_tx_buf_pos == hdlc_tx_buf_len)
		{
			// packet sent successfully
			hdlc_tx_buf_len = 0;
			hdlc_tx_buf_pos = 0;
		}
	}

}

int main(int argc, char const *argv[])
{
	int len;
	int numSamples;
	char buf[4096];
	uint8_t inSampleBuf[sizeof(buf) / 2];
	uint8_t outSampleBuf[sizeof(buf) / 2];
	fd_set in_fds;
	fd_set out_fds;
	fd_set err_fds;

	start_pppd(&pppd_fd, &pppd);
	fprintf(stderr, "fd: %d pid: %d\n", pppd_fd, pppd);

	osmo_isdnhdlc_rcv_init(&hdlc_rx, OSMO_HDLC_F_BITREVERSE);
	osmo_isdnhdlc_out_init(&hdlc_tx, OSMO_HDLC_F_BITREVERSE);

	// fill our codec conversion tables
	alaw_slin_init();

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
			len = read(FD_YATE_STDIN, buf, sizeof(buf));
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

				yate_msg_buf[yate_msg_buf_pos] = buf[i];
				yate_msg_buf_pos++;

				if (buf[i] == '\n') {
					// process incoming message
					yate_msg_buf[yate_msg_buf_pos - 1] = 0x00;
					fprintf(stderr, "Yate incoming message: %s\n", yate_msg_buf);

					yate_parse_incoming_message(yate_msg_buf, yate_msg_buf_pos);

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
				len = read(pppd_fd, pppd_rx_buf, sizeof(pppd_rx_buf));
				if (len <= 0) {
					break;
				}
				fprintf(stderr, "pppd wrote SYNC: %s\n\n",  osmo_hexdump(pppd_rx_buf, len));

				int deframed_bytes = 0;

				unsigned char escape = 0;
				for (int i = 0; i < len; ++i)
				{
					unsigned char ch = pppd_rx_buf[i];
					switch (ch)
					{
						case PPP_FLAG:
						    if (escape)
						    {
								fprintf(stderr, "%s: got an escaped PPP_FLAG\n",  __FUNCTION__);
								break;
						    }

						    if (len >= 2) {
						      /* must be the end, drop the FCS */
						      len -= 2;
						    }
						    else if (len == 1) {
						      /* Do nothing, just return the single character*/
						    }
						    else {
						      /* if the buffer is empty, then we have the beginning
						       * of a packet, not the end
						       */
						      break;
						    }

						    break;

						case PPP_ESCAPE:
						    escape = PPP_TRANS;
						    break;

						default:
						    ch ^= escape;
						    escape = 0;

						    hdlc_tx_buf[deframed_bytes] = ch;
						    deframed_bytes++;
					}	
				}

				hdlc_tx_buf_len = deframed_bytes;
				hdlc_tx_buf_pos = 0;

				//fprintf(stderr, "pppd wrote: %s\n\n",  osmo_hexdump(pppd_rx_buf, len));
			}
		}

		// Yate will send us samples via FD 3
		if (FD_ISSET(FD_YATE_SAMPLE_INPUT, &in_fds)) {
			len = read(FD_YATE_SAMPLE_INPUT, buf, sizeof(buf));
			if (len <= 0) {
				break;
			}
			
			if (len % 2) {
				DLPRINTF("read an odd number of bytes!! (%d)\n", len);
			}
			numSamples = len / 2;

			// convert the "signed linear PCM" back to "alaw" (so we get our real bits back)
			for (int i = 0; i < numSamples; i++)
			{
				inSampleBuf[i] = s2a[ ((uint16_t *)buf)[i] ];
			}

			handle_sample_buffer(outSampleBuf, inSampleBuf, numSamples);

			// echo test: 
			//memcpy(outSampleBuf, inSampleBuf, numSamples);

			for (int i = 0; i < numSamples; i++)
			{
				((int16_t *)buf)[i] = a2s[ outSampleBuf[i] ];
			}
			
			if (write(FD_YATE_SAMPLE_OUTPUT, buf, len) != len) {
				fprintf(stderr, "can't write the entire outgoing buffer!\n");
				break;
			}
		}

	}

	//kill(pppd, SIGKILL);

	return 0;
}