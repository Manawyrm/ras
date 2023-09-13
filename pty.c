/*
 * pty.c, originally from Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Pseudo-pty allocation routines...  Concepts and code borrowed
 * from pty-redir by Magosanyi Arpad.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#define PTY00 "/dev/ptyXX"
#define PTY10 "pqrstuvwxyzabcde"
#define PTY01 "0123456789abcdef"

int getPtyMaster_pty (char *tty10, char *tty01)
{
	char *p10;
	char *p01;
	static char dev[] = PTY00;
	int fd;

	for (p10 = PTY10; *p10; p10++)
	{
		dev[8] = *p10;
		for (p01 = PTY01; *p01; p01++)
		{
			dev[9] = *p01;
			fd = open (dev, O_RDWR | O_NONBLOCK);
			if (fd >= 0)
			{
				*tty10 = *p10;
				*tty01 = *p01;
				return fd;
			}
		}
	}
	fprintf(stderr, "%s: No more free pseudo-tty's\n", __FUNCTION__);
	return -1;
}

int getPtyMaster_ptmx(char *ttybuf, int ttybuflen)
{
	int fd;
	char *tty;

	fd = open("/dev/ptmx", O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "%s: unable to open /dev/ptmx to allocate pty\n",
				  __FUNCTION__);
		return -EINVAL;
	}

	/* No need to call grantpt */

	if (unlockpt(fd))
	{
		fprintf(stderr, "%s: unable to unlockpt() on pty\n",
				  __FUNCTION__);
		close(fd);
		return -EINVAL;
	}

	tty = ptsname(fd);
	if (tty == NULL)
	{
		fprintf(stderr, "%s: unable to obtain name of slave tty\n",
				  __FUNCTION__);
		close(fd);
		return -EINVAL;
	}
	ttybuf[0]='\0';
	strncat(ttybuf, tty, ttybuflen);

	return fd;
}

int getPtyMaster(char *ttybuf, int ttybuflen)
{
	int fd;
	fd = getPtyMaster_ptmx(ttybuf, ttybuflen);
	char a, b;

	if(fd >= 0) {
		return fd;
	}

	fprintf(stderr, "%s: failed to use pts -- using legacy ptys\n", __FUNCTION__);
	fd = getPtyMaster_pty(&a,&b);
	
	if(fd >= 0) {
		snprintf(ttybuf, ttybuflen, "/dev/tty%c%c", a, b);
		return fd;
	}

	return -EINVAL;
}
