#include <osmocom/core/select.h>
#include <osmocom/core/socket.h>

#include "telnet.h"

static struct osmo_fd telnet_fd;

int telnet_init(void *telnet_rx_cb, char* hostname, uint16_t port)
{
    int rc = -1;
    telnet_fd.cb = telnet_rx_cb;
    telnet_fd.when = OSMO_FD_READ | OSMO_FD_EXCEPT;
    rc = osmo_sock_init_ofd(&telnet_fd, AF_INET,
                            SOCK_STREAM, IPPROTO_TCP,
                            hostname, port,
                            OSMO_SOCK_F_CONNECT);

    if (rc < 0)
    {
        return rc;
    }

    return telnet_fd.fd;
}