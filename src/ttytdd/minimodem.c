#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/fcntl.h>
// we cannot #include <fcntl.h> as this conflicts with linux/fcntl.h, but we need the above for F_SETPIPE_SZ,
extern int fcntl (int __fd, int __cmd, ...);


#include "minimodem.h"
#include "../config.h"

#define MINIMODEM_CMD "/root/yate/share/scripts/tty/minimodem/src/minimodem"

static int minimodem_run(int *minimodem_pid, int parent_write_fd, int child_write_fd, char **stropt)
{
    fprintf(stderr, "%s: exec: ", __FUNCTION__);
    for (int x = 0; stropt[x]; x++)
    {
        fprintf(stderr, "%s ", stropt[x]);
    };
    fprintf(stderr, "\n");
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
        dup2(parent_write_fd, 0);
        dup2(child_write_fd, 1);

        /* close all the calls pty fds */
        close (2);
        close (3);
        close (4);

        execv (MINIMODEM_CMD, stropt);
        fprintf(stderr, "%s: Exec of %s failed!\n", __FUNCTION__, MINIMODEM_CMD);
        _exit (1);
    }
    close(parent_write_fd);
    close(child_write_fd);

    int argc = 0;
    while (stropt[argc])
    {
        free (stropt[argc]);
        argc++;
    };

    return 0;
}

int minimodem_run_tty_tx(int *minimodem_pid, int *data_in_fd, int *sample_out_fd)
{
    int parent_write[2], child_write[2];
    char *stropt[80];
    int pos = 1;

    pipe(parent_write);
    pipe(child_write);

    // reduce buffer sizes for output buffer (we want minimodem to stall its sample output when we're not reading any more data)
    // we're reading samples from the minimodem TX with the incoming line rate, to generate the same amount of data we're receiving.
    int ret = fcntl(child_write[0], F_SETPIPE_SZ, 160 * 4 * TTY_MINIMODEM_BUFFER_SIZE);
    if (ret < 0) {
        perror("set pipe size failed.");
    }
    ret = fcntl(child_write[1], F_SETPIPE_SZ, 160 * 4 * TTY_MINIMODEM_BUFFER_SIZE);
    if (ret < 0) {
        perror("set pipe size failed.");
    }
    if(fcntl(child_write[0], F_SETFL, O_NONBLOCK)!=0) {
        fprintf(stderr, "failed to set nonblock 0: %s\n", strerror(errno));
        return -EINVAL;
    }

    stropt[0] = strdup (MINIMODEM_CMD);
    stropt[pos++] = strdup ("--tx");
    stropt[pos++] = strdup ("--baudot");
    stropt[pos++] = strdup ("--quiet");
    stropt[pos++] = strdup ("--samplerate");
    stropt[pos++] = strdup ("8000");
    stropt[pos++] = strdup ("--stdio");

    // optional: send carrier permanently ("full-duplex")
    //stropt[pos++] = strdup ("--tx-carrier");

    stropt[pos++] = strdup ("tdd");
    stropt[pos] = NULL;

    *data_in_fd = parent_write[1];
    *sample_out_fd = child_write[0];

    return minimodem_run(minimodem_pid, parent_write[0], child_write[1], stropt);
}

int minimodem_run_tty_rx(int *minimodem_pid, int *sample_in_fd, int *data_out_fd)
{
    int parent_write[2], child_write[2];
    char *stropt[80];
    int pos = 1;

    pipe(parent_write);
    pipe(child_write);

    stropt[0] = strdup (MINIMODEM_CMD);
    stropt[pos++] = strdup ("--rx");
    stropt[pos++] = strdup ("--baudot");
    stropt[pos++] = strdup ("--samplerate");
    stropt[pos++] = strdup ("8000");
    stropt[pos++] = strdup ("--stdio");
    stropt[pos++] = strdup ("tdd");
    stropt[pos] = NULL;

    *sample_in_fd = parent_write[1];
    *data_out_fd = child_write[0];

    return minimodem_run(minimodem_pid, parent_write[0], child_write[1], stropt);
}