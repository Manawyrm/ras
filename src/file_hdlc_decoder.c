#include <stdio.h>
#include <osmocom/core/select.h>
#include <osmocom/core/isdnhdlc.h>

#include "config.h"
#include "gsmtap.h"

/* HDLC RX decoder state */
struct osmo_isdnhdlc_vars hdlc_rx = {0};
/* HDLC TX decoder state */
struct osmo_isdnhdlc_vars hdlc_tx = {0};

uint8_t hdlc_rx_buf[MAX_MTU * 2] = {0}; // buffer for data decoded from ISDN HDLC frames

void handle_sample_buffer(struct osmo_isdnhdlc_vars *hdlc, uint8_t sub_type, bool network_to_user, uint8_t *in_buf, int num_samples)
{
    int rv, count = 0;

    int samplesProcessed = 0;
    while (samplesProcessed < num_samples)
    {
        rv = osmo_isdnhdlc_decode(hdlc,
                                  in_buf + samplesProcessed, num_samples - samplesProcessed, &count,
                                  hdlc_rx_buf, sizeof(hdlc_rx_buf) - 3
        );

        if (rv > 0) {
            gsmtap_send_packet(sub_type, network_to_user, hdlc_rx_buf, rv);
        } else if (rv < 0) {
            switch (rv) {
                case -OSMO_HDLC_FRAMING_ERROR:
                    fprintf(stderr, "OSMO_HDLC_FRAMING_ERROR\n");
                    break;
                case -OSMO_HDLC_LENGTH_ERROR:
                    fprintf(stderr, "OSMO_HDLC_LENGTH_ERROR\n");
                    break;
                case -OSMO_HDLC_CRC_ERROR:
                    fprintf(stderr, "OSMO_HDLC_CRC_ERROR\n");
                    break;
            }
        }
        samplesProcessed += count;
    }
}

int main(int argc, char const *argv[])
{
    gsmtap_init("127.0.0.1");

    osmo_isdnhdlc_rcv_init(&hdlc_rx, OSMO_HDLC_F_BITREVERSE);
    osmo_isdnhdlc_rcv_init(&hdlc_tx, OSMO_HDLC_F_BITREVERSE);

    FILE* file_rx = fopen(argv[1], "rb");
    FILE* file_tx = fopen(argv[2], "rb");

    uint8_t data[16];
    while (!feof(file_rx) || !feof(file_tx))
    {
        int ret = fread(data, 1, 16, file_rx);
        if (ret > 0)
            handle_sample_buffer(&hdlc_rx, GSMTAP_E1T1_X75, true, data, ret);

        ret = fread(data, 1, 16, file_tx);
        if (ret > 0)
            handle_sample_buffer(&hdlc_tx,GSMTAP_E1T1_X75, false, data, ret);
    }
    fclose(file_rx);
    fclose(file_tx);

    return 0;
}