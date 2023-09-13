#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <osmocom/core/utils.h>

#include "isdnhdlc.h"
#include "a2s.h"

#define DLPRINTF(args...) fprintf(stderr, args)

// yate interfacing code by Karl Koscher <supersat@cs.washington.edu>
typedef uint8_t samp_t;

#define FD_YATE_STDIN 0
#define FD_YATE_STDOUT 1
#define FD_YATE_STDERR 2
#define FD_YATE_SAMPLE_INPUT 3
#define FD_YATE_SAMPLE_OUTPUT 4

#define FD_MAX 5

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

char yate_msg_buf[4096];
int yate_msg_buf_pos = 0;

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

int main(int argc, char const *argv[])
{
	int len;
	int numSamples;
	int skipSamples;
	char buf[4096];
	uint8_t inSampleBuf[sizeof(buf) / 2];
	uint8_t outSampleBuf[sizeof(buf) / 2];
	fd_set in_fds;
	fd_set out_fds;
	fd_set err_fds;
	struct termios termios;
	int child = 0;

	// fill our codec conversion tables
	alaw_slin_init();

	//dp_dummy_init();
	//dp_sinus_init();
	//prop_dp_init();
	//modem_timer_init();

	//init_modem(&modem);

//	if (argc > 1) {
//		// If a program is specified as a command line argument, then we run
//		// the command with the pty name as the argument.
//
//		int attach_pty = strstr(argv[0], "attach") >= 0;
//
//		const char * const prgName = argv[1];
//		const char * const prgArgv[3] = { prgName, modem.modem->pty_name, NULL };
//		child = fork();
//
//		if (!child) {
//			if (attach_pty) {
//				int pty = open(modem.modem->pty_name, O_RDWR);
//				dup2(pty, 0);
//				dup2(pty, 1);
//				dup2(pty, 2);
//			}
//			// The const-ness is probably safe to case away here since we've fork()ed
//			execv(argv[1], (char * const *)prgArgv);
//			return 0;
//		}
//	}

//	DLPRINTF("Modem pty is fd %d\n", modem.modem->pty);

	//while (!child || !waitpid(child, NULL, WNOHANG))
	while (1)
	{
		FD_ZERO(&in_fds);
		FD_ZERO(&out_fds);
		FD_ZERO(&err_fds);

		FD_SET(FD_YATE_STDIN, &in_fds);
		FD_SET(FD_YATE_SAMPLE_INPUT, &in_fds);

		FD_SET(1, &out_fds);
		FD_SET(FD_YATE_SAMPLE_OUTPUT, &out_fds);

		for (int i = 0; i < 5; i++) {
			FD_SET(i, &err_fds);
		}

		if (select(FD_MAX, &in_fds, NULL, &err_fds, NULL) <= 0) {
			fprintf(stderr, "select failed\n");
			break;
		}

		for (int i = 0; i <= FD_MAX; i++) {
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

			//DLPRINTF("Received %d bytes (%d samples)\n", len, numSamples);
			
			// convert the "signed linear PCM" back to "alaw" (so we get our real bits back)
			for (int i = 0; i < numSamples; i++)
			{
				inSampleBuf[i] = s2a[ ((uint16_t *)buf)[i] ];
			}

			
			memcpy(outSampleBuf, inSampleBuf, numSamples);

			for (int i = 0; i < numSamples; i++)
			{
				((int16_t *)buf)[i] = a2s[ outSampleBuf[i] ];
			}

			//DLPRINTF("Writing %d samples (%d bytes) back to YATE\n", numSamples, len);
			
			if (write(FD_YATE_SAMPLE_OUTPUT, buf, len) != len) {
				fprintf(stderr, "can't write the entire outgoing buffer!\n");
				break;
			}
		}

		//if (FD_ISSET(modem.modem->pty, &in_fds)) {
		//	tcgetattr(modem.modem->pty, &termios);
		//	if (memcmp(&termios, &modem.modem->termios, sizeof(termios))) {
		//		modem_update_termios(modem.modem, &termios);
		//	}
		//	len = modem.modem->xmit.size - modem.modem->xmit.count;
		//	if (len == 0) {
		//		continue;
		//	}
		//	if (len > sizeof(buf)) {
		//		len = sizeof(buf);
		//	}
		//	len = read(modem.modem->pty, buf, len);
		//	if (len > 0) {
		//		modem_write(modem.modem, buf, len);
		//	}
		//}
	}

	//kill(child, SIGHUP);

//	// Read data
//	uint8_t rx_buf[2048];
//
//	long bytesRead = 0;
//	int rv, cl, oi = 0;
//
//	char ch;
//	while (1) {
//		if (read(0, &ch, 1) > 0)
//		{
//			rv = osmo_isdnhdlc_decode(&hdlc_rx,
//				&ch, 1, &cl,
//				rx_buf, sizeof(rx_buf)
//			);
//
//			if (rv > 0) {
//				int bytes_to_write = rv;
//				printf("I 2023-09-13T19:33:00Z\n000000 %s\n\n",  osmo_hexdump(rx_buf, rv));
//				//rv = write(ts->fd, ts->hdlc.rx_buf, bytes_to_write);
//				//if (rv <= 0)
//				//	return rv;
//			} else  if (rv < 0) {
//				switch (rv) {
//				case -OSMO_HDLC_FRAMING_ERROR:
//					printf("OSMO_HDLC_FRAMING_ERROR\n");
//					break;
//				case -OSMO_HDLC_LENGTH_ERROR:
//					printf("OSMO_HDLC_LENGTH_ERROR\n");
//					break;
//				case -OSMO_HDLC_CRC_ERROR:
//					printf("OSMO_HDLC_CRC_ERROR\n");
//					break;
//				}
//				//printf("ERR RX: %d %d %d [ %s]\n",
//				//	rv, oi, cl, osmo_hexdump(&ch, 1));
//
//			}
//
//			bytesRead += cl;
//		}
//	}

	return 0;
}