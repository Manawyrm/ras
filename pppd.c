#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <osmocom/core/utils.h>

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const char *PPPD = "/usr/sbin/pppd";
int getPtyMaster(char *, int);

int start_pppd(int *fd, int *pppd)
{
	//int fd = -1;                /* File descriptor for pty */
	//int pppd;                   /* PID of pppd */

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
