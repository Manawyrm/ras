/* gsmtap - Encapsulate ISDN packet traces in GSMTAP]
 * mostly copied from osmocom/simtrace2/host/lib/gsmtap.c
 *
 * (C) 2016-2019 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <osmocom/core/gsmtap.h>
#include <osmocom/core/gsmtap_util.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "gsmtap.h"

#define GSMTAP_ARFCN_F_UPLINK			0x4000

/*! global GSMTAP instance */
static struct gsmtap_inst *g_gti;

/*! initialize the global GSMTAP instance */
int gsmtap_init(const char *gsmtap_host)
{
    if (g_gti)
        return -EEXIST;

    g_gti = gsmtap_source_init(gsmtap_host, GSMTAP_UDP_PORT, 0);
    if (!g_gti) {
        perror("unable to open GSMTAP");
        return -EIO;
    }
    gsmtap_source_add_sink(g_gti);

    return 0;
}

/*! log one packet via the global GSMTAP instance.
 *  \param[in] sub_type GSMTAP sub-type (GSMTAP_E1T1_* constant)
 *  \param[in] network_to_user Packet direction
 *  \param[in] data User-provided buffer with packet to log
 *  \param[in] len Length of apdu in bytes
 */
int gsmtap_send_packet(uint8_t sub_type, bool network_to_user, const uint8_t *data, unsigned int len)
{
    struct gsmtap_hdr *gh;
    unsigned int gross_len = len + sizeof(*gh);
    uint8_t *buf = malloc(gross_len);
    int rc;

    if (!buf)
        return -ENOMEM;

    memset(buf, 0, sizeof(*gh));
    gh = (struct gsmtap_hdr *) buf;
    gh->version = GSMTAP_VERSION;
    gh->hdr_len = sizeof(*gh)/4;
    gh->arfcn = htons(network_to_user ? GSMTAP_ARFCN_F_UPLINK : 0x00);
    gh->type = GSMTAP_TYPE_E1T1;
    gh->sub_type = sub_type;

    memcpy(buf + sizeof(*gh), data, len);

    rc = write(gsmtap_inst_fd(g_gti), buf, gross_len);
    if (rc < 0) {
        fprintf(stderr, "write packet via gsmtap failed\n");
        free(buf);
        return rc;
    }

    free(buf);
    return 0;
}

int gsmtap_send_rlp(bool network_to_user, const uint8_t *data, unsigned int len)
{
    struct gsmtap_hdr *gh;
    unsigned int gross_len = len + sizeof(*gh);
    uint8_t *buf = malloc(gross_len);
    int rc;

    if (!buf)
        return -ENOMEM;

    memset(buf, 0, sizeof(*gh));
    gh = (struct gsmtap_hdr *) buf;
    gh->version = GSMTAP_VERSION;
    gh->hdr_len = sizeof(*gh)/4;
    gh->arfcn = htons(network_to_user ? GSMTAP_ARFCN_F_UPLINK : 0x00);
    gh->type = GSMTAP_TYPE_GSM_RLP;

    memcpy(buf + sizeof(*gh), data, len);

    rc = write(gsmtap_inst_fd(g_gti), buf, gross_len);
    if (rc < 0) {
        fprintf(stderr, "write packet via gsmtap failed\n");
        free(buf);
        return rc;
    }

    free(buf);
    return 0;
}