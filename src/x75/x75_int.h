/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _X75_INT_H
#define _X75_INT_H
#include <stdio.h>
#include <stdbool.h>

#include "x75.h"

#define HZ 1000

#define	X75_HEADER_LEN	20		/* X75 over Ethernet + a bit more */

#define	X75_ACK_PENDING_CONDITION	0x01
#define	X75_REJECT_CONDITION		0x02
#define	X75_PEER_RX_BUSY_CONDITION	0x04

/* Control field templates */
#define	X75_I		0x00	/* Information frames */
#define	X75_S		0x01	/* Supervisory frames */
#define	X75_U		0x03	/* Unnumbered frames */

#define	X75_RR		0x01	/* Receiver ready */
#define	X75_RNR	0x05	/* Receiver not ready */
#define	X75_REJ	0x09	/* Reject */

#define	X75_SABM	0x2F	/* Set Asynchronous Balanced Mode */
#define	X75_SABME	0x6F	/* Set Asynchronous Balanced Mode Extended */
#define	X75_DISC	0x43	/* Disconnect */
#define	X75_DM		0x0F	/* Disconnected mode */
#define	X75_UA		0x63	/* Unnumbered acknowledge */
#define	X75_FRMR	0x87	/* Frame reject */

#define X75_ILLEGAL	0x100	/* Impossible to be a real frame type */

#define	X75_SPF	0x10	/* Poll/final bit for standard X75 */
#define	X75_EPF	0x01	/* Poll/final bit for extended X75 */

#define	X75_FRMR_W	0x01	/* Control field invalid	*/
#define	X75_FRMR_X	0x02	/* I field invalid		*/
#define	X75_FRMR_Y	0x04	/* I field too long		*/
#define	X75_FRMR_Z	0x08	/* Invalid N(R)			*/

#define	X75_POLLOFF	0
#define	X75_POLLON	1

/* X75 C-bit */
#define X75_COMMAND	1
#define X75_RESPONSE	2

#define	X75_ADDR_A	0x03
#define	X75_ADDR_B	0x01
#define	X75_ADDR_C	0x0F
#define	X75_ADDR_D	0x07

/* Define Link State constants. */
enum {
    X75_STATE_0,	/* Disconnected State		*/
    X75_STATE_1,	/* Awaiting Connection State	*/
    X75_STATE_2,	/* Awaiting Disconnection State	*/
    X75_STATE_3,	/* Data Transfer State		*/
    X75_STATE_4	    /* Frame Reject State		*/
};

#define	X75_DEFAULT_MODE		(X75_STANDARD | X75_SLP | X75_DTE)
#define	X75_DEFAULT_WINDOW		7		/* Window=7 */
#define	X75_DEFAULT_T1			(5 * HZ)	/* T1=5s    */
#define	X75_DEFAULT_T2			(1 * HZ)	/* T2=1s    */
#define	X75_DEFAULT_N2			20		/* N2=20    */

#define	X75_SMODULUS	8
#define	X75_EMODULUS	128



/* x75_iface.c */
void x75_connect_confirmation(struct x75_cb *x75, int);
void x75_connect_indication(struct x75_cb *x75, int);
void x75_disconnect_confirmation(struct x75_cb *x75, int);
void x75_disconnect_indication(struct x75_cb *x75, int);
int x75_data_indication(struct x75_cb *x75, struct msgb *);
int x75_data_transmit(struct x75_cb *x75, struct msgb *);

/* x75_in.c */
void x75_data_input(struct x75_cb *x75, struct msgb *);

/* x75_out.c */
void x75_kick(struct x75_cb *x75);
void x75_transmit_buffer(struct x75_cb *x75, struct msgb *, int);
void x75_establish_data_link(struct x75_cb *x75);
void x75_enquiry_response(struct x75_cb *x75);
void x75_timeout_response(struct x75_cb *x75);
void x75_check_iframes_acked(struct x75_cb *x75, unsigned short);
void x75_check_need_response(struct x75_cb *x75, int, int);

/* x75_subr.c */
void x75_clear_queues(struct x75_cb *x75);
void x75_frames_acked(struct x75_cb *x75, unsigned short);
void x75_requeue_frames(struct x75_cb *x75);
int x75_validate_nr(struct x75_cb *x75, unsigned short);
int x75_decode(struct x75_cb *x75, struct msgb *, struct x75_frame *);
void x75_send_control(struct x75_cb *x75, int, int, int);
void x75_transmit_frmr(struct x75_cb *x75);

/* x75_timer.c */
void x75_start_t1timer(struct x75_cb *x75);
void x75_start_t2timer(struct x75_cb *x75);
void x75_stop_t1timer(struct x75_cb *x75);
void x75_stop_t2timer(struct x75_cb *x75);
int x75_t1timer_running(struct x75_cb *x75);

/*
 * Debug levels.
 *	0 = Off
 *	1 = State Changes
 *	2 = Packets I/O and State Changes
 *	3 = Hex dumps, Packets I/O and State Changes.
 */
#define	X75_DEBUG	3

#define x75_dbg(level, fmt, ...)			\
do {							\
	if (level < X75_DEBUG)				\
		fprintf(stderr, fmt, ##__VA_ARGS__);		\
} while (0)

#endif