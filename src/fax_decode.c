
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

#define SAMPLES_PER_CHUNK   160

#define DISBIT1     0x01
#define DISBIT2     0x02
#define DISBIT3     0x04
#define DISBIT4     0x08
#define DISBIT5     0x10
#define DISBIT6     0x20
#define DISBIT7     0x40
#define DISBIT8     0x80

enum
{
    FAX_NONE,
    FAX_V27TER_RX,
    FAX_V29_RX,
    FAX_V17_RX
};

static const struct
{
    int bit_rate;
    int modem_type;
    int which;
    uint8_t dcs_code;
} fallback_sequence[] =
        {
                {14400, T30_MODEM_V17,    T30_SUPPORT_V17,    DISBIT6},
                {12000, T30_MODEM_V17,    T30_SUPPORT_V17,    (DISBIT6 | DISBIT4)},
                { 9600, T30_MODEM_V17,    T30_SUPPORT_V17,    (DISBIT6 | DISBIT3)},
                { 9600, T30_MODEM_V29,    T30_SUPPORT_V29,    DISBIT3},
                { 7200, T30_MODEM_V17,    T30_SUPPORT_V17,    (DISBIT6 | DISBIT4 | DISBIT3)},
                { 7200, T30_MODEM_V29,    T30_SUPPORT_V29,    (DISBIT4 | DISBIT3)},
                { 4800, T30_MODEM_V27TER, T30_SUPPORT_V27TER, DISBIT4},
                { 2400, T30_MODEM_V27TER, T30_SUPPORT_V27TER, 0},
                {    0, 0, 0, 0}
        };

int decode_test = FALSE;
int rx_bits = 0;

t30_state_t t30_dummy;
t4_state_t t4_state;
int t4_up = FALSE;

hdlc_rx_state_t hdlcrx;

int fast_trained = FAX_NONE;

uint8_t ecm_data[256][260];
int16_t ecm_len[256];

int line_encoding = T4_COMPRESSION_ITU_T4_1D;
int x_resolution = T4_X_RESOLUTION_R8;
int y_resolution = T4_Y_RESOLUTION_STANDARD;
int image_width = 1728;
int octets_per_ecm_frame = 256;
int error_correcting_mode = FALSE;
int current_fallback = 0;

static void print_frame(const char *io, const uint8_t *fr, int frlen)
{
    int i;
    int type;
    const char *country;
    const char *vendor;
    const char *model;

    fprintf(stderr, "%s %s:", io, t30_frametype(fr[2]));
    for (i = 2;  i < frlen;  i++)
        fprintf(stderr, " %02x", fr[i]);
    fprintf(stderr, "\n");
    type = fr[2] & 0xFE;
    if (type == T30_DIS  ||  type == T30_DTC  ||  type == T30_DCS)
        t30_decode_dis_dtc_dcs(&t30_dummy, fr, frlen);
    if (type == T30_NSF  ||  type == T30_NSS  ||  type == T30_NSC)
    {
        if (t35_decode(&fr[3], frlen - 3, &country, &vendor, &model))
        {
            if (country)
                fprintf(stderr, "The remote was made in '%s'\n", country);
            if (vendor)
                fprintf(stderr, "The remote was made by '%s'\n", vendor);
            if (model)
                fprintf(stderr, "The remote is a '%s'\n", model);
        }
    }
}
/*- End of function --------------------------------------------------------*/

static int find_fallback_entry(int dcs_code)
{
    int i;

    /* The table is short, and not searched often, so a brain-dead linear scan seems OK */
    for (i = 0;  fallback_sequence[i].bit_rate;  i++)
    {
        if (fallback_sequence[i].dcs_code == dcs_code)
            break;
    }
    if (fallback_sequence[i].bit_rate == 0)
        return -1;
    return i;
}
/*- End of function --------------------------------------------------------*/

static int check_rx_dcs(const uint8_t *msg, int len)
{
    static const int widths[3][4] =
            {
                    { 864, 1024, 1216, -1}, /* R4 resolution - no longer used in recent versions of T.30 */
                    {1728, 2048, 2432, -1}, /* R8 resolution */
                    {3456, 4096, 4864, -1}  /* R16 resolution */
            };
    uint8_t dcs_frame[T30_MAX_DIS_DTC_DCS_LEN];

    /* Check DCS frame from remote */
    if (len < 6)
    {
        printf("Short DCS frame\n");
        return -1;
    }

    /* Make a local copy of the message, padded to the maximum possible length with zeros. This allows
       us to simply pick out the bits, without worrying about whether they were set from the remote side. */
    if (len > T30_MAX_DIS_DTC_DCS_LEN)
    {
        memcpy(dcs_frame, msg, T30_MAX_DIS_DTC_DCS_LEN);
    }
    else
    {
        memcpy(dcs_frame, msg, len);
        if (len < T30_MAX_DIS_DTC_DCS_LEN)
            memset(dcs_frame + len, 0, T30_MAX_DIS_DTC_DCS_LEN - len);
    }

    octets_per_ecm_frame = (dcs_frame[6] & DISBIT4)  ?  256  :  64;
    if ((dcs_frame[8] & DISBIT1))
        y_resolution = T4_Y_RESOLUTION_SUPERFINE;
    else if (dcs_frame[4] & DISBIT7)
        y_resolution = T4_Y_RESOLUTION_FINE;
    else
        y_resolution = T4_Y_RESOLUTION_STANDARD;
    image_width = widths[(dcs_frame[8] & DISBIT3)  ?  2  :  1][dcs_frame[5] & (DISBIT2 | DISBIT1)];

    /* Check which compression we will use. */
    if ((dcs_frame[6] & DISBIT7))
        line_encoding = T4_COMPRESSION_ITU_T6;
    else if ((dcs_frame[4] & DISBIT8))
        line_encoding = T4_COMPRESSION_ITU_T4_2D;
    else
        line_encoding = T4_COMPRESSION_ITU_T4_1D;
    fprintf(stderr, "Selected compression %d\n", line_encoding);

    if ((current_fallback = find_fallback_entry(dcs_frame[4] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))) < 0)
        printf("Remote asked for a modem standard we do not support\n");
    error_correcting_mode = ((dcs_frame[6] & DISBIT3) != 0);

    //v17_rx_restart(&v17, fallback_sequence[fallback_entry].bit_rate, FALSE);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
{
    int type;
    int frame_no;
    int i;

    if (len < 0)
    {
        /* Special conditions */
        fprintf(stderr, "HDLC status is %s (%d)\n", signal_status_to_str(len), len);
        return;
    }

    if (ok)
    {
        if (msg[0] != 0xFF  ||  !(msg[1] == 0x03  ||  msg[1] == 0x13))
        {
            fprintf(stderr, "Bad frame header - %02x %02x\n", msg[0], msg[1]);
            return;
        }
        print_frame("HDLC: ", msg, len);
        type = msg[2] & 0xFE;
        switch (type)
        {
            case T4_FCD:
                if (len <= 4 + 256)
                {
                    frame_no = msg[3];
                    /* Just store the actual image data, and record its length */
                    memcpy(&ecm_data[frame_no][0], &msg[4], len - 4);
                    ecm_len[frame_no] = (int16_t) (len - 4);
                }
                break;
            case T30_DCS:
                check_rx_dcs(msg, len);
                break;
        }
    }
    else
    {
        fprintf(stderr, "Bad HDLC frame ");
        for (i = 0;  i < len;  i++)
            fprintf(stderr, " %02x", msg[i]);
        fprintf(stderr, "\n");
    }
}
/*- End of function --------------------------------------------------------*/

static void t4_begin(void)
{
    int i;

    //printf("Begin T.4 - %d %d %d %d\n", line_encoding, x_resolution, y_resolution, image_width);
    t4_rx_set_rx_encoding(&t4_state, line_encoding);
    t4_rx_set_x_resolution(&t4_state, x_resolution);
    t4_rx_set_y_resolution(&t4_state, y_resolution);
    t4_rx_set_image_width(&t4_state, image_width);

    t4_rx_start_page(&t4_state);
    t4_up = TRUE;

    for (i = 0;  i < 256;  i++)
        ecm_len[i] = -1;
}
/*- End of function --------------------------------------------------------*/

static void t4_end(void)
{
    t4_stats_t stats;
    int i;
    int j;
    int k;

    if (!t4_up)
        return;
    if (error_correcting_mode)
    {
        for (i = 0;  i < 256;  i++)
        {
            for (j = 0;  j < ecm_len[i];  j++)
            {
                for (k = 0;  k < 8;  k++)
                    t4_rx_put_bit(&t4_state, (ecm_data[i][j] >> k) & 1);
            }
            fprintf(stderr, "%d", (ecm_len[i] < 0)  ?  0  :  1);
        }
        fprintf(stderr, "\n");
    }
    t4_rx_end_page(&t4_state);
    //t30_get_transfer_statistics(&t4_state, &stats);
    //fprintf(stderr, "Pages = %d\n", stats.pages_transferred);
    //fprintf(stderr, "Image size = %dx%d\n", stats.width, stats.length);
    //fprintf(stderr, "Image resolution = %dx%d\n", stats.x_resolution, stats.y_resolution);
    //fprintf(stderr, "Bad rows = %d\n", stats.bad_rows);
    //fprintf(stderr, "Longest bad row run = %d\n", stats.longest_bad_row_run);
    t4_up = FALSE;
}
/*- End of function --------------------------------------------------------*/

static void v21_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        fprintf(stderr, "V.21 rx status is %s (%d)\n", signal_status_to_str(bit), bit);
        switch (bit)
        {
            case SIG_STATUS_CARRIER_DOWN:
                //t4_end();
                break;
        }
        return;
    }
    if (fast_trained == FAX_NONE)
        hdlc_rx_put_bit(&hdlcrx, bit);
    //printf("V.21 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

static void v17_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        fprintf(stderr, "V.17 rx status is %s (%d)\n", signal_status_to_str(bit), bit);
        switch (bit)
        {
            case SIG_STATUS_TRAINING_SUCCEEDED:
                fast_trained = FAX_V17_RX;
                t4_begin();
                break;
            case SIG_STATUS_CARRIER_DOWN:
                t4_end();
                if (fast_trained == FAX_V17_RX)
                    fast_trained = FAX_NONE;
                break;
        }
        return;
    }
    if (error_correcting_mode)
    {
        hdlc_rx_put_bit(&hdlcrx, bit);
    }
    else
    {
        if (t4_rx_put_bit(&t4_state, bit))
        {
            t4_end();
            fprintf(stderr, "End of page detected\n");
        }
    }
    //printf("V.17 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

static void v29_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        fprintf(stderr, "V.29 rx status is %s (%d)\n", signal_status_to_str(bit), bit);
        switch (bit)
        {
            case SIG_STATUS_TRAINING_SUCCEEDED:
                fast_trained = FAX_V29_RX;
                t4_begin();
                break;
            case SIG_STATUS_CARRIER_DOWN:
                t4_end();
                if (fast_trained == FAX_V29_RX)
                    fast_trained = FAX_NONE;
                break;
        }
        return;
    }
    if (error_correcting_mode)
    {
        hdlc_rx_put_bit(&hdlcrx, bit);
    }
    else
    {
        if (t4_rx_put_bit(&t4_state, bit))
        {
            t4_end();
            fprintf(stderr, "End of page detected\n");
        }
    }
    //printf("V.29 Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

static void v27ter_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        fprintf(stderr, "V.27ter rx status is %s (%d)\n", signal_status_to_str(bit), bit);
        switch (bit)
        {
            case SIG_STATUS_TRAINING_SUCCEEDED:
                fast_trained = FAX_V27TER_RX;
                t4_begin();
                break;
            case SIG_STATUS_CARRIER_DOWN:
                t4_end();
                if (fast_trained == FAX_V27TER_RX)
                    fast_trained = FAX_NONE;
                break;
        }
        return;
    }
    if (error_correcting_mode)
    {
        hdlc_rx_put_bit(&hdlcrx, bit);
    }
    else
    {
        if (t4_rx_put_bit(&t4_state, bit))
        {
            t4_end();
            fprintf(stderr, "End of page detected\n");
        }
    }
    //printf("V.27ter Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

int span_fax_init()
{
    memset(&t30_dummy, 0, sizeof(t30_dummy));
    span_log_init(&t30_dummy.logging, SPAN_LOG_FLOW, NULL);
    span_log_set_protocol(&t30_dummy.logging, "T.30");

    hdlc_rx_init(&hdlcrx, FALSE, TRUE, 5, hdlc_accept, NULL);

    if (t4_rx_init(&t4_state, "fax_decode.tif", T4_COMPRESSION_ITU_T4_1D) == NULL)
    {
        fprintf(stderr, "Failed to init\n");
        exit(0);
    }

    //t4_rx_release(&t4_state);

    //if (sf_close(inhandle) != 0)
    //{
    //    fprintf(stderr, "    Cannot close audio file '%s'\n", filename);
    //    exit(2);
    //}
    return  0;
}

int span_fax_bits(uint8_t bit) {
    if (error_correcting_mode)
    {
        hdlc_rx_put_bit(&hdlcrx, bit);
    }
    else
    {
        if (t4_rx_put_bit(&t4_state, bit))
        {
            t4_end();
            fprintf(stderr, "End of page detected\n");
        }
    }
}


int span_in_sync()
{
    fast_trained = FAX_V27TER_RX;
    t4_begin();
}