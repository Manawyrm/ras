#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/fcntl.h>

#include "minimodem.h"

#define MINIMODEM_CMD "/root/yate/share/scripts/tty/minimodem/src/minimodem"

int minimodem_run_tty_tx(int *minimodem_pid, int *data_in_fd, int *sample_out_fd)
{
    int parent_write[2], child_write[2];
    char *stropt[80];
    int pos = 1;
    int x;

    pipe(parent_write);
    pipe(child_write);

    int ret = fcntl(child_write[0], F_SETPIPE_SZ, 160 * 4 * 16);
    if (ret < 0) {
        perror("set pipe size failed.");
    }
    ret = fcntl(child_write[1], F_SETPIPE_SZ, 160 * 4 * 16);
    if (ret < 0) {
        perror("set pipe size failed.");
    }

    stropt[0] = strdup (MINIMODEM_CMD);
    stropt[pos++] = strdup ("-t");
    stropt[pos++] = strdup ("-5");
    stropt[pos++] = strdup ("-R");
    stropt[pos++] = strdup ("8000");
    stropt[pos++] = strdup ("-O");
    stropt[pos++] = strdup ("tdd");
    stropt[pos++] = strdup ("--tx-carrier");
    stropt[pos] = NULL;

    fprintf(stderr, "%s: I'm running: \n", __FUNCTION__);
    for (x = 0; stropt[x]; x++)
    {
        fprintf(stderr, "\"%s\" \n", stropt[x]);
    };
    *minimodem_pid = fork();

    if (*minimodem_pid < 0)
    {
        /* parent */
        fprintf(stderr, "%s: unable to fork(), abandoning!\n", __FUNCTION__);
        return -EINVAL;
    }
    else if (!*minimodem_pid)
    {
        /* child */
        close (0); /* redundant; the dup2() below would do that, too */
        close (1); /* ditto */
        /* close (2); No, we want to keep the connection to /dev/null. */

        /* connect the pty to stdin and stdout */
        dup2(parent_write[0], 0);
        dup2(child_write[1], 1);

        /* close all the calls pty fds */
        close (2);
        close (3);
        close (4);

        execv (MINIMODEM_CMD, stropt);
        fprintf(stderr, "%s: Exec of %s failed!\n", __FUNCTION__, MINIMODEM_CMD);
        _exit (1);
    }
    close(parent_write[0]);
    close(child_write[1]);

    *data_in_fd = parent_write[1];
    *sample_out_fd = child_write[0];

    pos = 0;
    while (stropt[pos])
    {
        free (stropt[pos]);
        pos++;
    };
    return 0;
}