#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <osmocom/core/select.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/isdn/v110.h>
#include <osmocom/trau/trau_sync.h>
#include <osmocom/gsm/i460_mux.h>
#include <osmocom/core/isdnhdlc.h>

#include <errno.h>
#include "gsmtap.h"

static struct osmo_i460_timeslot g_i460_ts;
static struct osmo_fsm_inst *g_sync_fi;
static struct osmo_fsm_inst *g_sync_fi2;

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

uint8_t syncword[] = {0x3E, 0x37, 0x50, 0x96, 0xC1, 0xC8, 0xAF, 0x69};
ubit_t syncword_bits[64] = {0};
uint8_t syncpos = 0;

uint32_t bit = 0;
ubit_t frame_bits[512 * 1024 * 8];
uint8_t bytes[512 * 1024];

bool in_sync = false;

int span_fax_init();
int span_fax_bits(uint8_t bit);
int span_in_sync();

bool data_mode = false;

/* call-back for each synced V.110 frame */
static void v110_bits_cb(void *user_data, const ubit_t *bits, unsigned int num_bits)
{
    struct osmo_v110_decoded_frame dfr;
    int rc;
    rc = osmo_v110_decode_frame(&dfr, bits, num_bits);
    if (rc < 0) {
        printf("\tERROR decoding V.110 frame\n");
        return;
    }
    //osmo_trau_sync_rx_ubits(g_sync_fi2, dfr.d_bits, sizeof(dfr.d_bits));

    for (int i = 0; i < sizeof(dfr.d_bits); ++i)
    {
        //fprintf(stderr, "syncpos: %d, bit: %d, syncword_bits[syncpos]: %d, dfr.d_bits[i]: %d\n", syncpos, bit, syncword_bits[syncpos], dfr.d_bits[i]);

        if (syncword_bits[syncpos] != dfr.d_bits[i])
        {
            syncpos = 0;
        }

        if (syncword_bits[syncpos] == dfr.d_bits[i])
        {
            syncpos++;
        }

        frame_bits[bit++] = dfr.d_bits[i];

        if (syncpos == 64)
        {
            syncpos = 0;
            bit = 0;

            // Sync found.
            if (!in_sync)
            {
                in_sync = true;
                span_in_sync();
            }
        }

        if (bit == 64)
        {
            bit = 0;

            osmo_ubit2pbit(bytes, frame_bits, 64);
            fprintf(stderr, "syncpos: %d, fax bits: %s\n", syncpos, osmo_hexdump(bytes, 8));

            if (memcmp("\x33\x0f\x33\x0f\x33\x0f\x33\x0f", bytes, 8) == 0)
            {
                data_mode = true;
            }
            else
            {
                if (data_mode)
                {
                    for (int j = 0; j < 64; ++j)
                    {
                        span_fax_bits(frame_bits[j]);
                    }
                }
            }

        }

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

int main(int argc, char const *argv[])
{
    tall_ras_ctx = talloc_named_const(NULL, 1, "RAS context");
    if (!tall_ras_ctx)
        return -ENOMEM;
    msgb_talloc_ctx_init(tall_ras_ctx, 0);

    gsmtap_init("::1");

    osmo_i460_ts_init(&g_i460_ts);
    osmo_i460_subchan_add(NULL, &g_i460_ts, &g_i460_chd);
    g_sync_fi = osmo_trau_sync_alloc(NULL, "V110", v110_bits_cb, OSMO_TRAU_SYNCP_V110, NULL);

    osmo_pbit2ubit(syncword_bits, syncword, 64);

    span_fax_init();

    FILE* file_rx = fopen(argv[1], "rb");

    uint8_t data[160];
    while (!feof(file_rx))
    {
        int ret = fread(data, 1, 160, file_rx);
        if (ret > 0)
        {
            /* feed into I.460 de-multiplexer (for 64/32/16/8k rate) */
            osmo_i460_demux_in(&g_i460_ts, data, ret);
        }
    }
    fclose(file_rx);

    talloc_report_full(tall_ras_ctx, stderr);
    talloc_free(tall_ras_ctx);

    return 0;
}