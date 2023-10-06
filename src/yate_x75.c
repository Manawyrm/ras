// ITU-T Rec. X.75 (10/96)
// Packet-switched signalling system between public networks providing data transmission services

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <osmocom/core/select.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/isdnhdlc.h>

#include <errno.h>
#include "yate.h"
#include "yate_message.h"
#include "telnet.h"
#include "config.h"
#include "gsmtap.h"
#include "x75/x75.h"

// Talloc context
void *tall_ras_ctx = NULL;
int telnet_fd = -1;

char *hostname = NULL;
uint16_t port = 0;

/* HDLC decoder state */
struct osmo_isdnhdlc_vars hdlc_rx = {0};
/* HDLC encoder state */
struct osmo_isdnhdlc_vars hdlc_tx = {0};
uint8_t hdlc_rx_buf[MAX_MTU * 2] = {0}; // buffer for data decoded from ISDN HDLC frames
struct llist_head isdn_hdlc_tx_queue;

struct msgb *hdlc_tx_msgb = NULL;
uint8_t *hdlc_tx_buf;
int hdlc_tx_buf_pos;
int hdlc_tx_buf_len;

struct x75_cb x75_instance = { 0 };

void isdn_packet_tx(uint8_t *buf, int len)
{
    struct msgb *msg;
    uint8_t *ptr;

    msg = msgb_alloc_c(tall_ras_ctx, len, "isdn hdlc transmit");
    gsmtap_send_packet(GSMTAP_E1T1_X75, false, buf, len);
    ptr = msgb_put(msg, len);
    memcpy(ptr, buf, len);
    msgb_enqueue(&isdn_hdlc_tx_queue, msg);
}

void handle_sample_buffer(uint8_t *out_buf, uint8_t *in_buf, int num_samples)
{
    int rv, count = 0;

    int samplesProcessed = 0;
    while (samplesProcessed < num_samples)
    {
        rv = osmo_isdnhdlc_decode(&hdlc_rx,
                                  in_buf + samplesProcessed, num_samples - samplesProcessed, &count,
                                  hdlc_rx_buf, sizeof(hdlc_rx_buf) - 5
        );

        if (rv > 0) {
            gsmtap_send_packet(GSMTAP_E1T1_X75, true, hdlc_rx_buf, rv);
            fprintf(stderr, "msgb_alloc(%d)\n", rv);
            struct msgb *skb = msgb_alloc_headroom(rv + 2048, 2048, "incoming hdlc packet");
            uint8_t *ptr = msgb_push(skb, rv);
            memcpy(ptr, hdlc_rx_buf, rv);
            x75_data_received(&x75_instance, skb);
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

    // send packets
    samplesProcessed = 0;
    while (samplesProcessed < num_samples)
    {
        // is there still a packet being transmitted?
        if (!hdlc_tx_buf_len)
        {
            // get a new one from the queue
            hdlc_tx_msgb = msgb_dequeue(&isdn_hdlc_tx_queue);
            if (hdlc_tx_msgb != NULL)
            {
                // got one
                hdlc_tx_buf = msgb_data(hdlc_tx_msgb);
                hdlc_tx_buf_len = msgb_length(hdlc_tx_msgb);
                hdlc_tx_buf_pos = 0;
            }
        }
        rv = osmo_isdnhdlc_encode(&hdlc_tx,
                                  (const uint8_t *) (hdlc_tx_buf + hdlc_tx_buf_pos), hdlc_tx_buf_len - hdlc_tx_buf_pos,
                                  &count,
                                  out_buf + samplesProcessed, num_samples - samplesProcessed
        );

        if (rv < 0) {
            fprintf(stderr, "ERR TX: %d\n", rv);
        }

        if (rv > 0) {
            samplesProcessed += rv;

            if (hdlc_tx_buf_len) {
                hdlc_tx_buf_pos += count;
                if (hdlc_tx_buf_pos == hdlc_tx_buf_len)
                {
                    // finished sending packet
                    hdlc_tx_buf_len = 0;
                    hdlc_tx_buf_pos = 0;
                    msgb_free(hdlc_tx_msgb);
                }
            }
        }
    }

}

bool connected = false;

char text_buf[512];
int telnet_rx_cb(struct osmo_fd *fd, unsigned int what) {
    ssize_t len;

    len = read(fd->fd, text_buf, sizeof(text_buf));
    if (len <= 0) {
        return -1;
    }

    struct msgb *msg;
    uint8_t *ptr;
    msg = msgb_alloc_headroom(len + 5, 5, "raw x75 payload data");
    ptr = msgb_put(msg, len);
    memcpy(ptr, text_buf, len);
    x75_data_request(&x75_instance, msg);

    fprintf(stderr, "x75_data_request(%d)\n", len);

    return 0;
}

void call_initialize(char *called, char *caller, char *format) {
    telnet_fd = telnet_init(&telnet_rx_cb, hostname, port);
    if (telnet_fd < 0)
    {
        // connection setup failed.
        fprintf(stderr, "Telnet connection could not be established: %d\n", telnet_fd);
        exit(1);
    }
}

void yate_x75_connected(void *dev, int reason)
{
    fprintf(stderr, "x75_connected\n");
    connected = true;
}

void yate_x75_disconnected(void *dev, int reason)
{
    fprintf(stderr, "x75_disconnected\n");
}

int  yate_x75_data_indication(void *dev, struct msgb *skb)
{
    fprintf(stderr, "x75_data_indication\n");
    if (telnet_fd)
    {
        write(telnet_fd, msgb_data(skb), msgb_length(skb));
    }
    return 0;
}

void yate_x75_data_transmit(void *dev, struct msgb *skb) {
    fprintf(stderr, "x75_data_transmit\n");

    struct msgb *msg;
    uint8_t *ptr;

    msg = msgb_alloc_c(tall_ras_ctx, msgb_length(skb), "isdn hdlc transmit");
    gsmtap_send_packet(GSMTAP_E1T1_X75, true, msgb_data(skb), msgb_length(skb));

    ptr = msgb_put(msg, msgb_length(skb));
    memcpy(ptr, msgb_data(skb), msgb_length(skb));
    msgb_enqueue(&isdn_hdlc_tx_queue, msg);
}

static const struct x75_register_struct cb = {
        .connect_confirmation = yate_x75_connected,
        .connect_indication = yate_x75_connected,
        .disconnect_confirmation = yate_x75_disconnected,
        .disconnect_indication = yate_x75_disconnected,
        .data_indication = yate_x75_data_indication,
        .data_transmit = yate_x75_data_transmit,
};

int main(int argc, char const *argv[])
{
    if (argc != 2 && argc != 3) {
        fprintf( stderr, " Not enough arguments given on command line.\n\n" );
        fprintf( stderr, " usage: %s <hostname> <port>\n", argv[0] );
        exit(1);
    }
    if (argc == 3) {
        hostname = (char *) argv[1];
        port = atoi(argv[2]);
    }
    if (argc == 2) {
        // yate passes all arguments as a single one...
        char *end = memchr(argv[1], ' ', strlen(argv[1]));
        if (end == NULL)
        {
            exit(1);
        }
        end[0] = 0x00;

        hostname = (char*) argv[1];
        port = atoi(&end[1]);
    }

    tall_ras_ctx = talloc_named_const(NULL, 1, "RAS context");
    if (!tall_ras_ctx)
        return -ENOMEM;
    msgb_talloc_ctx_init(tall_ras_ctx, 0);
    INIT_LLIST_HEAD(&isdn_hdlc_tx_queue);

    gsmtap_init("::1");

    osmo_isdnhdlc_rcv_init(&hdlc_rx, OSMO_HDLC_F_BITREVERSE);
    osmo_isdnhdlc_out_init(&hdlc_tx, OSMO_HDLC_F_BITREVERSE);

    x75_register(&x75_instance, &cb);

    // register yate onto STDIN and FD3 (sample input)
    yate_osmo_fd_register(&handle_sample_buffer, &call_initialize);

    while (true)
    {
        osmo_select_main(0);
    }

    talloc_report_full(tall_ras_ctx, stderr);
    talloc_free(tall_ras_ctx);

    return 0;
}