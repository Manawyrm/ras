#include <stdio.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/isdnhdlc.h>

#include "pppd.h"


struct rfc1662_vars hdlc_rx = {0};
/* HDLC encoder state */
struct osmo_isdnhdlc_vars hdlc_tx;


int main()
{
    // encoder test
//    uint8_t out[512];
//
//    uint8_t data[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0xFF };
//    printf(" in: %s\n", osmo_hexdump(data, sizeof (data)));
//
//    int32_t hdlc_bytes_written = pppd_rfc1662_encode(data, sizeof (data), out);
//    printf("out: %s\n", osmo_hexdump(out, hdlc_bytes_written));
//
//    // decoder test
//    uint8_t hdlc[] = {0x7e, 0x7d , 0x20, 0x7d, 0x21, 0x7d, 0x22, 0x7d, 0x23, 0x7d, 0x24, 0xff, 0xc1, 0x42, 0x7e,
//                      0x7e, 0x7d , 0x2e, 0x7e, 0x21, 0x7d, 0x22, 0x7d, 0x23, 0x7d, 0x24, 0xff, 0xc1, 0x42, 0x7e};
//    int count;
//    uint8_t raw[512];
//
//    int bytes_read = 0;
//
//    while (bytes_read != sizeof(hdlc) - 1)
//    {
//        int rv = pppd_rfc1662_decode(&hdlc_rx, hdlc + bytes_read, sizeof (hdlc) - bytes_read, &count, raw, sizeof(raw));
//
//        bytes_read += count;
//
//        printf("rv: %d, count: %d, bytes_read: %d\n", rv, count, bytes_read);
//        printf("dec: %s\n", osmo_hexdump(raw, rv));
//    }

//    int count;
//    uint8_t raw[512];
//
//    uint8_t hdlc[] = {0x7e, 0x7d , 0x20, 0x7d, 0x21, 0x7d};
//    int rv = pppd_rfc1662_decode(&hdlc_rx, hdlc, sizeof (hdlc), &count, raw, sizeof(raw));
//    printf("rv: %d, count: %d\n", rv, count);
//    printf("dec: %s\n", osmo_hexdump(raw, rv));
//
//    uint8_t hdlc2[] = {0x22, 0x7d, 0x23, 0x7d, 0x24, 0xff, 0xc1, 0x42, 0x7e};
//    rv = pppd_rfc1662_decode(&hdlc_rx, hdlc2, sizeof (hdlc2), &count, raw, sizeof(raw));
//    printf("rv: %d, count: %d\n", rv, count);
//    printf("dec: %s\n", osmo_hexdump(raw, rv));
//
//    uint8_t hdlc3[] = {0x7e, 0x55, 0xaa, 0xaa, 0x7e, 0x7e, 0xc1, 0x42, 0xaa, 0x7e};
//    rv = pppd_rfc1662_decode(&hdlc_rx, hdlc3, sizeof (hdlc3), &count, raw, sizeof(raw));
//    printf("rv: %d, count: %d\n", rv, count);
//    printf("dec: %s\n", osmo_hexdump(raw, rv));
    osmo_isdnhdlc_out_init(&hdlc_tx, 0);

    int rv;
    int count;
    uint8_t out[512] = {0};
    uint8_t data[40] = { 0 };
    for (size_t i = 0; i < sizeof(data); ++i) {
        data[i] = (uint8_t) i;
    }


    int bytes_sent = 0;

    while (bytes_sent != sizeof(data))
    {
        rv = osmo_isdnhdlc_encode(&hdlc_tx,
                                  data + bytes_sent, sizeof(data) - bytes_sent, &count,
                                  out, 160
        );

        if (rv < 0)
        {
            fprintf(stderr, "ERR TX: %d\n", rv);
        }

        if (rv > 0)
        {
            bytes_sent += count;
            fprintf(stderr, "O count: %d, rv: %d,  %s\n\n", count, rv, osmo_hexdump(out, 160));
        }
    }


    return 0;
}