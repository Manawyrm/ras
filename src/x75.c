#include <stdio.h>
#include <osmocom/core/select.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/isdnhdlc.h>
#include <osmocom/core/msgb.h>

#include "x75.h"

#define X75_ADDRESS_SLP_STE_A   0x03
#define X75_ADDRESS_SLP_STE_B   0x01

#define X75_N_R_MASK		    0xE0
#define X75_N_R_SHIFT		    5
#define X75_N_S_MASK		    0x0E
#define X75_N_S_SHIFT		    1
#define X75_P_F                 0x10

#define X75_S_FTYPE_MASK        0x0C
#define X75_RR			        0x00	// Receiver ready
#define X75_RNR		            0x04	// Receiver not ready
#define X75_REJ		            0x08	// Reject
#define X75_SREJ		        0x0C	// Selective reject

#define X75_I_MASK		0x01	// Mask to test for I or not I
#define X75_I			0x00	// Information frames
#define X75_S_U_MASK	0x03	// Mask to test for S or U
#define X75_S			0x01	// Supervisory frames
#define X75_U			0x03	// Unnumbered frames

/*
 * U-format modifiers.
 */
#define X75_U_MODIFIER_MASK	0xEC
#define X75_UI		        0x00	// Unnumbered Information
#define X75_UP		        0x20	// Unnumbered Poll
#define X75_DISC	        0x40	// Disconnect (command)
#define X75_RD		        0x40	// Request Disconnect (response)
#define X75_UA		        0x60	// Unnumbered Acknowledge
#define X75_SNRM	        0x80	// Set Normal Response Mode
#define X75_TEST	        0xE0	// Test
#define X75_SIM	            0x04	// Set Initialization Mode (command)
#define X75_RIM	            0x04	// Request Initialization Mode (response)
#define X75_FRMR	        0x84	// Frame reject
#define X75_CFGR	        0xC4	// Configure
#define X75_SARM	        0x0C	// Set Asynchronous Response Mode (command)
#define X75_DM		        0x0C	// Disconnected mode (response)
#define X75_SABM	        0x2C	// Set Asynchronous Balanced Mode
#define X75_SARME	        0x4C	// Set Asynchronous Response Mode Extended
#define X75_SABME	        0x6C	// Set Asynchronous Balanced Mode Extended
#define X75_RESET	        0x8C	// Reset
#define X75_XID	            0xAC	// Exchange identification
#define X75_SNRME	        0xCC	// Set Normal Response Mode Extended
#define X75_BCN	            0xEC	// Beacon

void (*packet_tx)(uint8_t *, int);

void x75_init(void *isdn_packet_tx_cb)
{
    packet_tx = isdn_packet_tx_cb;
}

uint8_t txbuf[2048] = {0};
void x75_recv(uint8_t *buf, int len)
{
    uint8_t address = buf[0];
    uint8_t control = buf[1];
    uint8_t n_s;

    if (address == X75_ADDRESS_SLP_STE_B)
    {
        switch (control & X75_S_U_MASK) {
            case X75_S: // Supervisory frame

                break;
            case X75_U: // Unnumbered frame
                txbuf[0] = X75_ADDRESS_SLP_STE_B;
                txbuf[1] = 0x73;
                (*packet_tx)(txbuf, 2);
                break;
            default: // Information frame
                n_s = (control & X75_N_S_MASK) >> X75_N_S_SHIFT;
                fprintf(stderr, "I N(S): %d frame: %s\n", n_s, osmo_hexdump(buf + 2, len - 2));

                uint8_t n_r = n_s + 1;
                if (n_r >= 8)
                    n_r = 0;

                txbuf[0] = X75_ADDRESS_SLP_STE_B;
                txbuf[1] = X75_S | X75_P_F | (X75_RR << 2) | ((n_r & 0x07) << X75_N_R_SHIFT);
                (*packet_tx)(txbuf, 2);
        }
    }
}
