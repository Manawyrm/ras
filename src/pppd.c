#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "pppd.h"

const char *PPPD = "/usr/sbin/pppd";
int getPtyMaster(char *, int);

#define PPP_FLAG 0x7e
#define PPP_ESCAPE 0x7d
#define PPP_TRANS 0x20

#define PPP_INITFCS 0xffff
#define PPP_GOODFCS 0xf0b8
#define PPP_FCS(fcs,c) (((fcs) >> 8) ^ ppp_crc16_table[((fcs) ^ (c)) & 0xff])

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

uint16_t pppd_rfc1662_checksum(const uint8_t *buf, uint32_t len)
{
    uint16_t fcs = PPP_INITFCS;
    const unsigned char *c = buf;
    for (uint32_t x = 0; x < len; x++)
    {
        fcs = PPP_FCS (fcs, *c);
        c++;
    }
    fcs = fcs ^ 0xFFFF;

    return fcs;
}

int32_t pppd_rfc1662_encode(const uint8_t *raw_in, uint32_t raw_len, uint8_t *hdlc_out)
{
    int32_t pos = 0;
    unsigned char e = 0;

    uint16_t fcs = pppd_rfc1662_checksum(raw_in, raw_len);

    hdlc_out[pos++] = PPP_FLAG;
    for (uint32_t x = 0; x < raw_len + 2; x++)
    {
        if (x < raw_len)
        {
            // data
            e = raw_in[x];
        }
        else
        {
            // FCS
            if (x == raw_len)      e = fcs & 0xFF;
            if (x == raw_len + 1)  e = (fcs >> 8) & 0xFF;
        }
        if ((e < 0x20) || (e == PPP_ESCAPE) || (e == PPP_FLAG))
        {
            /* Escape this */
            e = e ^ 0x20;
            hdlc_out[pos++] = PPP_ESCAPE;
        }
        hdlc_out[pos++] = e;
    }
    hdlc_out[pos++] = PPP_FLAG;

    return pos;
}

int pppd_rfc1662_decode(struct rfc1662_vars *state, const uint8_t *src,
                         int slen, int *count, uint8_t *dst, int dsize)
{
    for (int i = 0; i < slen; ++i)
    {
        unsigned char ch = src[i];
        *count = i;

        switch (ch)
        {
            case PPP_FLAG:
                if (state->escape)
                {
                    fprintf(stderr, "%s: got an escaped PPP_FLAG\n",  __FUNCTION__);
                    break;
                }

                if (state->deframed_bytes >= 2) {
                    /* must be the end, drop the FCS */
                    state->deframed_bytes -= 2;
                }
                else if (state->deframed_bytes == 1) {
                    /* framing error */
                    state->deframed_bytes -= 1;
                    break;
                }
                else {
                    /* if the buffer is empty, then we have the beginning
                     * of a packet, not the end
                     */
                    break;
                }

                int packet_len = state->deframed_bytes;
                state->deframed_bytes = 0;
                state->escape = 0;
                return packet_len;
            case PPP_ESCAPE:
                state->escape = PPP_TRANS;
                break;

            default:
                ch ^= state->escape;
                state->escape = 0;
                if (state->deframed_bytes < dsize)
                {
                    dst[state->deframed_bytes] = ch;
                    state->deframed_bytes++;
                    break;
                }
                // too many bytes for dest buf
                return -EINVAL;
        }
    }

    // no valid frame decoded
    return 0;
}


int start_pppd(int *fd, int *pppd)
{
	/* char a, b; */
	char tty[512];
	char *stropt[80];
	int pos = 1;
	int fd2 = -1;
	int x;
	struct termios ptyconf;

	stropt[0] = strdup (PPPD);
	if (*fd > -1)
	{
		fprintf(stderr, "%s: file descriptor already assigned!\n",
			 __FUNCTION__);
		return -EINVAL;
	}

	if ((*fd = getPtyMaster (tty, sizeof(tty))) < 0)
	{
		fprintf(stderr, "%s: unable to allocate pty, abandoning!\n",
				  __FUNCTION__);
		return -EINVAL;
	}

	/* set fd opened above to not echo so we don't see read our own packets
	   back of the file descriptor that we just wrote them to */
	tcgetattr (*fd, &ptyconf);
	
	ptyconf.c_cflag &= ~(ICANON | ECHO);
	ptyconf.c_lflag &= ~ECHO;
	tcsetattr (*fd, TCSANOW, &ptyconf);
	if(fcntl(*fd, F_SETFL, O_NONBLOCK)!=0) {
	   fprintf(stderr, "failed to set nonblock: %s\n", strerror(errno));
		return -EINVAL;
	}

	fd2 = open (tty, O_RDWR);
	if (fd2 < 0) {
		fprintf(stderr, "unable to open tty %s, cannot start pppd", tty);
		return -EINVAL;
	}
	stropt[pos++] = strdup(tty);

	//{
	//	struct ppp_opts *p = opts;
	//	int maxn_opts = sizeof(stropt) / sizeof(stropt[0]) - 1;
	//	while (p && pos < maxn_opts)
	//	{
	//		stropt[pos] = strdup (p->option);
	//		pos++;
	//		p = p->next;
	//	}
	//	stropt[pos] = NULL;
	//}

	stropt[pos] = strdup ("auth"); // PPPD options
	pos++;

	stropt[pos] = strdup ("debug"); // PPPD options
	pos++;

	stropt[pos] = strdup ("file"); // PPPD options
	pos++;

	stropt[pos] = strdup ("/etc/ppp/options.osmoras"); // PPPD options
	pos++;

	stropt[pos] = strdup ("172.21.118.1:172.21.118.5"); // PPPD options
	pos++;

	stropt[pos] = strdup ("passive"); // PPPD options
	pos++;

	stropt[pos] = strdup ("nodetach"); // PPPD options
	pos++;

	fprintf(stderr, "%s: I'm running: \n", __FUNCTION__);
	for (x = 0; stropt[x]; x++)
	{
		fprintf(stderr, "\"%s\" \n", stropt[x]);
	};
	*pppd = fork ();

	if (*pppd < 0)
	{
		/* parent */
		fprintf(stderr, "%s: unable to fork(), abandoning!\n", __FUNCTION__);
		close(fd2);
		return -EINVAL;
	}
	else if (!*pppd)
	{
		/* child */
		close (0); /* redundant; the dup2() below would do that, too */
		close (1); /* ditto */
		/* close (2); No, we want to keep the connection to /dev/null. */

		/* connect the pty to stdin and stdout */
		dup2 (fd2, 0);
		dup2 (fd2, 1);
		close(fd2);
	   
		/* close all the calls pty fds */
		close (2);
		close (3);
		close (4);

		execv (PPPD, stropt);
		fprintf(stderr, "%s: Exec of %s failed!\n", __FUNCTION__, PPPD);
		_exit (1);
	}
	close (fd2);
	pos = 0;
	while (stropt[pos])
	{
		free (stropt[pos]);
		pos++;
	};
	return 0;
}
