#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <osmocom/core/select.h>
#include <osmocom/core/socket.h>

#include "telnet.h"

static struct osmo_fd telnet_ofd;

int telnet_init(void *telnet_rx_cb, char* hostname, uint16_t port, struct osmo_fd **ofd)
{
    int ret, on;
    int rc = -1;
    telnet_ofd.cb = telnet_rx_cb;
    telnet_ofd.when = OSMO_FD_READ | OSMO_FD_EXCEPT;
    rc = osmo_sock_init_ofd(&telnet_ofd, AF_INET,
                            SOCK_STREAM, IPPROTO_TCP,
                            hostname, port,
                            OSMO_SOCK_F_CONNECT);

    ret = setsockopt(telnet_ofd.fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    if (ret != 0) {
        fprintf(stderr, "Failed to set TCP_NODELAY: %s\n", strerror(errno));
        return rc;
    }

    *ofd = &telnet_ofd;

    return rc;
}