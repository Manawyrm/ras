
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <osmocom/core/select.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/isdn/v110.h>
#include <osmocom/trau/trau_sync.h>
#include <osmocom/gsm/i460_mux.h>

#include "v110/soft_uart.h"
#include <errno.h>
#include "yate.h"
#include "yate_message.h"
#include "telnet.h"
#include "config.h"
#include "gsmtap.h"

static struct osmo_i460_timeslot g_i460_ts;
static struct osmo_fsm_inst *g_sync_fi;

/*! one instance of a soft-uart */
struct osmo_soft_uart {
    struct osmo_soft_uart_cfg cfg;
    const char *name;
    struct {
        bool running;
        uint8_t bit_count;
        uint8_t shift_reg;
        struct msgb *msg;
        ubit_t parity_bit;
        unsigned int flags;
        unsigned int status;
        struct osmo_timer_list timer;
    } rx;
    struct {
        bool running;
        uint8_t bit_count;
        uint8_t shift_reg;
        struct msgb *msg;
        struct llist_head queue;
    } tx;
};

void uart_rx(void *user, struct msgb *msg, unsigned int flags)
{
    fprintf(stderr, "uart_rx(): %s\n", osmo_hexdump(msgb_data(msg), msgb_length(msg)));

    msgb_free(msg);
}

/* See GSM 08.20 - Version 3.1.2, Figure 2 */
#define MAX_RLP_D_BITS	60
#define MAX_RLP_E_BITS	3

/*! a 'decoded' representation of a non-transparent/modified (RLP) V.110 frame. contains unpacked D and E bits */
struct osmo_v110_decoded_rlp_frame {
    ubit_t d_bits[MAX_RLP_D_BITS];
    ubit_t e_bits[MAX_RLP_E_BITS];
};


int osmo_v110_decode_rlp_frame(struct osmo_v110_decoded_rlp_frame *fr, const ubit_t *ra_bits, size_t n_bits)
{
    // GSM 08.20 - Version 3.1.2
    // V.110 non-transparent (RLP) frames have a different encoding
    if (n_bits < 80)
        return -EINVAL;

    /* E1 .. E7 */
    memcpy(fr->e_bits, ra_bits + 5 * 8 + 1, 3);

    /* D-bits */
    size_t cur_bit = 0;
    memcpy(fr->d_bits + cur_bit, ra_bits + 1 * 8 + 1, 7); cur_bit += 7;
    memcpy(fr->d_bits + cur_bit, ra_bits + 2 * 8 + 1, 7); cur_bit += 7;
    memcpy(fr->d_bits + cur_bit, ra_bits + 3 * 8 + 1, 7); cur_bit += 7;
    memcpy(fr->d_bits + cur_bit, ra_bits + 4 * 8 + 1, 7); cur_bit += 7;

    fr->d_bits[cur_bit++] = ra_bits[5 * 8 + 4]; // D'5
    fr->d_bits[cur_bit++] = ra_bits[5 * 8 + 5]; // D'6
    fr->d_bits[cur_bit++] = ra_bits[5 * 8 + 6]; // D'7
    fr->d_bits[cur_bit++] = ra_bits[5 * 8 + 7]; // D'8

    memcpy(fr->d_bits + cur_bit, ra_bits + 6 * 8 + 1, 7); cur_bit += 7;
    memcpy(fr->d_bits + cur_bit, ra_bits + 7 * 8 + 1, 7); cur_bit += 7;
    memcpy(fr->d_bits + cur_bit, ra_bits + 8 * 8 + 1, 7); cur_bit += 7;
    memcpy(fr->d_bits + cur_bit, ra_bits + 9 * 8 + 1, 7); cur_bit += 7;

    return 0;
}


static struct osmo_soft_uart g_suart = {
        .cfg = {
                .num_data_bits = 8,
                .num_stop_bits = 1,
                .parity_mode = OSMO_SUART_PARITY_NONE,
                .rx_cb = uart_rx,
        },
};


typedef void (*process_cb)(const uint8_t *buf, size_t buf_len);

// Talloc context
void *tall_ras_ctx = NULL;
static struct osmo_fd *telnet_ofd;

static void i460_out_bits_cb(struct osmo_i460_subchan *schan, void *user_data,
                             const ubit_t *bits, unsigned int num_bits)
{
    /* feed 8/16/32/64k stream into frame sync */
    osmo_trau_sync_rx_ubits(g_sync_fi, bits, num_bits);
}

ubit_t rlp_frame_bits[240];
uint8_t rlp_frame[30];

/* call-back for each synced V.110 frame */
static void frame_bits_cb(void *user_data, const ubit_t *bits, unsigned int num_bits)
{
    struct osmo_v110_decoded_rlp_frame dfr;
    int rc;

    /* dump the bits of each V.110 frame */
    //fprintf(stderr, "%s\n", osmo_ubit_dump(bits, num_bits));
    rc = osmo_v110_decode_rlp_frame(&dfr, bits, num_bits);
    if (rc < 0) {
        fprintf(stderr, "\tERROR decoding V.110 frame\n");
        return;
    }
    //fprintf(stderr, "\tS: %s\n", osmo_ubit_dump(dfr.s_bits, sizeof(dfr.s_bits)));
    //fprintf(stderr, "\tX: %s\n", osmo_ubit_dump(dfr.x_bits, sizeof(dfr.x_bits)));
    //fprintf(stderr, "\tE: %s\n", osmo_ubit_dump(dfr.e_bits, sizeof(dfr.e_bits)));
    //fprintf(stderr, "\tD: %s\n", osmo_ubit_dump(dfr.d_bits, sizeof(dfr.d_bits)));

    // https://www.etsi.org/deliver/etsi_gts/08/0820/03.01.02_60/gsmts_0820sv030102p.pdf
    uint8_t rlp_fsi = (dfr.e_bits[1] << 1) | dfr.e_bits[2];
    //fprintf(stderr, "\tFSI: %02x\n", rlp_fsi);

    memcpy(rlp_frame_bits + (rlp_fsi * 60), dfr.d_bits, 60);

    if (rlp_fsi == 0x03)
    {
        //fprintf(stderr, "RLP frame bits: %s\n", osmo_ubit_dump(rlp_frame_bits, sizeof(rlp_frame_bits)));
        osmo_ubit2pbit(rlp_frame, rlp_frame_bits, 240);
        for (int i = 0; i < sizeof(rlp_frame); ++i) {
            rlp_frame[i] = osmo_revbytebits_8(rlp_frame[i]);
        }

        fprintf(stderr, "RLP frame: %s\n", osmo_hexdump(rlp_frame, 30));
        gsmtap_send_rlp(false, rlp_frame, 30);
    }
}

static struct osmo_i460_schan_desc g_i460_chd = {
        //TODO: this must be adjustable to 8/16/32/64k depending on the user rate
        .rate = OSMO_I460_RATE_16k,
        .bit_offset = 0,
        .demux = {
                .num_bits = 80,
                .out_cb_bits = i460_out_bits_cb,
                .user_data = NULL,
        },
};

void handle_sample_buffer(uint8_t *out_buf, uint8_t *in_buf, int num_samples)
{
    int rc, count = 0;

    /* feed into I.460 de-multiplexer (for 64/32/16/8k rate) */
    osmo_i460_demux_in(&g_i460_ts, in_buf, num_samples);

    memcpy(out_buf, in_buf, num_samples);
}

void call_initialize(char *called, char *caller, char *format) {
}


int main(int argc, char const *argv[])
{
    tall_ras_ctx = talloc_named_const(NULL, 1, "RAS context");
    if (!tall_ras_ctx)
        return -ENOMEM;
    msgb_talloc_ctx_init(tall_ras_ctx, 0);

    gsmtap_init("::1");

    // register yate onto STDIN and FD3 (sample input)
    yate_osmo_fd_register(&handle_sample_buffer, &call_initialize);

    osmo_i460_ts_init(&g_i460_ts);
    osmo_i460_subchan_add(NULL, &g_i460_ts, &g_i460_chd);
    g_sync_fi = osmo_trau_sync_alloc(NULL, "V110", frame_bits_cb, OSMO_TRAU_SYNCP_V110, NULL);

    while (true)
    {
        osmo_select_main(0);
    }

    talloc_report_full(tall_ras_ctx, stderr);
    talloc_free(tall_ras_ctx);

    return 0;
}