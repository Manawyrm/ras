#include <osmocom/core/select.h>
#include <osmocom/core/socket.h>

#include "telnet.h"

static struct osmo_fd telnet_ofd;

int telnet_init(void *telnet_rx_cb, char* hostname, uint16_t port, struct osmo_fd **ofd)
{
    int rc = -1;
    telnet_ofd.cb = telnet_rx_cb;
    telnet_ofd.when = OSMO_FD_READ | OSMO_FD_EXCEPT;
    rc = osmo_sock_init_ofd(&telnet_ofd, AF_INET,
                            SOCK_STREAM, IPPROTO_TCP,
                            hostname, port,
                            OSMO_SOCK_F_CONNECT);

    *ofd = &telnet_ofd;

    return rc;
}