// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	X75 release 002
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	History
 *	X75 001	Jonathan Naulor	Started Coding
 *	X75 002	Jonathan Naylor	New timer architecture.
 *	2000-10-29	Henner Eisen	x75_data_indication() return status.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "x75_int.h"

/*
 *	State machine for state 0, Disconnected State.
 *	The handling of the timer(s) is in file x75_timer.c.
 */
static void x75_state0_machine(struct x75_cb *x75, struct msgb *skb,
                                struct x75_frame *frame)
{
    switch (frame->type) {
        case X75_SABM:
            x75_dbg(1, "(%p) S0 RX SABM(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S0 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            } else {
                x75_dbg(1, "(%p) S0 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_dbg(0, "(%p) S0 -> S3\n", x75->dev);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
                x75_stop_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state     = X75_STATE_3;
                x75->condition = 0x00;
                x75->n2count   = 0;
                x75->vs        = 0;
                x75->vr        = 0;
                x75->va        = 0;
                x75_connect_indication(x75, X75_OK);
            }
            break;

        case X75_SABME:
            x75_dbg(1, "(%p) S0 RX SABME(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S0 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_dbg(0, "(%p) S0 -> S3\n", x75->dev);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
                x75_stop_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state     = X75_STATE_3;
                x75->condition = 0x00;
                x75->n2count   = 0;
                x75->vs        = 0;
                x75->vr        = 0;
                x75->va        = 0;
                x75_connect_indication(x75, X75_OK);
            } else {
                x75_dbg(1, "(%p) S0 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            }
            break;

        case X75_DISC:
            x75_dbg(1, "(%p) S0 RX DISC(%d)\n", x75->dev, frame->pf);
            x75_dbg(1, "(%p) S0 TX UA(%d)\n", x75->dev, frame->pf);
            x75_send_control(x75, X75_UA, frame->pf, X75_RESPONSE);
            break;

        default:
            break;
    }

    msgb_free(skb);
}

/*
 *	State machine for state 1, Awaiting Connection State.
 *	The handling of the timer(s) is in file x75_timer.c.
 */
static void x75_state1_machine(struct x75_cb *x75, struct msgb *skb,
                                struct x75_frame *frame)
{
    switch (frame->type) {
        case X75_SABM:
            x75_dbg(1, "(%p) S1 RX SABM(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S1 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            } else {
                x75_dbg(1, "(%p) S1 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
            }
            break;

        case X75_SABME:
            x75_dbg(1, "(%p) S1 RX SABME(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S1 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
            } else {
                x75_dbg(1, "(%p) S1 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            }
            break;

        case X75_DISC:
            x75_dbg(1, "(%p) S1 RX DISC(%d)\n", x75->dev, frame->pf);
            x75_dbg(1, "(%p) S1 TX DM(%d)\n", x75->dev, frame->pf);
            x75_send_control(x75, X75_DM, frame->pf, X75_RESPONSE);
            break;

        case X75_UA:
            x75_dbg(1, "(%p) S1 RX UA(%d)\n", x75->dev, frame->pf);
            if (frame->pf) {
                x75_dbg(0, "(%p) S1 -> S3\n", x75->dev);
                x75_stop_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state     = X75_STATE_3;
                x75->condition = 0x00;
                x75->n2count   = 0;
                x75->vs        = 0;
                x75->vr        = 0;
                x75->va        = 0;
                x75_connect_confirmation(x75, X75_OK);
            }
            break;

        case X75_DM:
            x75_dbg(1, "(%p) S1 RX DM(%d)\n", x75->dev, frame->pf);
            if (frame->pf) {
                x75_dbg(0, "(%p) S1 -> S0\n", x75->dev);
                x75_clear_queues(x75);
                x75->state = X75_STATE_0;
                x75_start_t1timer(x75);
                x75_stop_t2timer(x75);
                x75_disconnect_indication(x75, X75_REFUSED);
            }
            break;
    }

    msgb_free(skb);
}

/*
 *	State machine for state 2, Awaiting Release State.
 *	The handling of the timer(s) is in file x75_timer.c
 */
static void x75_state2_machine(struct x75_cb *x75, struct msgb *skb,
                                struct x75_frame *frame)
{
    switch (frame->type) {
        case X75_SABM:
        case X75_SABME:
            x75_dbg(1, "(%p) S2 RX {SABM,SABME}(%d)\n",
                     x75->dev, frame->pf);
            x75_dbg(1, "(%p) S2 TX DM(%d)\n", x75->dev, frame->pf);
            x75_send_control(x75, X75_DM, frame->pf, X75_RESPONSE);
            break;

        case X75_DISC:
            x75_dbg(1, "(%p) S2 RX DISC(%d)\n", x75->dev, frame->pf);
            x75_dbg(1, "(%p) S2 TX UA(%d)\n", x75->dev, frame->pf);
            x75_send_control(x75, X75_UA, frame->pf, X75_RESPONSE);
            break;

        case X75_UA:
            x75_dbg(1, "(%p) S2 RX UA(%d)\n", x75->dev, frame->pf);
            if (frame->pf) {
                x75_dbg(0, "(%p) S2 -> S0\n", x75->dev);
                x75->state = X75_STATE_0;
                x75_start_t1timer(x75);
                x75_stop_t2timer(x75);
                x75_disconnect_confirmation(x75, X75_OK);
            }
            break;

        case X75_DM:
            x75_dbg(1, "(%p) S2 RX DM(%d)\n", x75->dev, frame->pf);
            if (frame->pf) {
                x75_dbg(0, "(%p) S2 -> S0\n", x75->dev);
                x75->state = X75_STATE_0;
                x75_start_t1timer(x75);
                x75_stop_t2timer(x75);
                x75_disconnect_confirmation(x75, X75_NOTCONNECTED);
            }
            break;

        case X75_I:
        case X75_REJ:
        case X75_RNR:
        case X75_RR:
            x75_dbg(1, "(%p) S2 RX {I,REJ,RNR,RR}(%d)\n",
                     x75->dev, frame->pf);
            x75_dbg(1, "(%p) S2 RX DM(%d)\n", x75->dev, frame->pf);
            if (frame->pf)
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            break;
    }

    msgb_free(skb);
}

/*
 *	State machine for state 3, Connected State.
 *	The handling of the timer(s) is in file x75_timer.c
 */
static void x75_state3_machine(struct x75_cb *x75, struct msgb *skb,
                                struct x75_frame *frame)
{
    int queued = 0;
    int modulus = (x75->mode & X75_EXTENDED) ? X75_EMODULUS :
                  X75_SMODULUS;

    switch (frame->type) {
        case X75_SABM:
            x75_dbg(1, "(%p) S3 RX SABM(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S3 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            } else {
                x75_dbg(1, "(%p) S3 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
                x75_stop_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->condition = 0x00;
                x75->n2count   = 0;
                x75->vs        = 0;
                x75->vr        = 0;
                x75->va        = 0;
                x75_requeue_frames(x75);
            }
            break;

        case X75_SABME:
            x75_dbg(1, "(%p) S3 RX SABME(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S3 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
                x75_stop_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->condition = 0x00;
                x75->n2count   = 0;
                x75->vs        = 0;
                x75->vr        = 0;
                x75->va        = 0;
                x75_requeue_frames(x75);
            } else {
                x75_dbg(1, "(%p) S3 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            }
            break;

        case X75_DISC:
            x75_dbg(1, "(%p) S3 RX DISC(%d)\n", x75->dev, frame->pf);
            x75_dbg(0, "(%p) S3 -> S0\n", x75->dev);
            x75_clear_queues(x75);
            x75_send_control(x75, X75_UA, frame->pf, X75_RESPONSE);
            x75_start_t1timer(x75);
            x75_stop_t2timer(x75);
            x75->state = X75_STATE_0;
            x75_disconnect_indication(x75, X75_OK);
            break;

        case X75_DM:
            x75_dbg(1, "(%p) S3 RX DM(%d)\n", x75->dev, frame->pf);
            x75_dbg(0, "(%p) S3 -> S0\n", x75->dev);
            x75_clear_queues(x75);
            x75->state = X75_STATE_0;
            x75_start_t1timer(x75);
            x75_stop_t2timer(x75);
            x75_disconnect_indication(x75, X75_NOTCONNECTED);
            break;

        case X75_RNR:
            x75_dbg(1, "(%p) S3 RX RNR(%d) R%d\n",
                     x75->dev, frame->pf, frame->nr);
            x75->condition |= X75_PEER_RX_BUSY_CONDITION;
            x75_check_need_response(x75, frame->cr, frame->pf);
            if (x75_validate_nr(x75, frame->nr)) {
                x75_check_iframes_acked(x75, frame->nr);
            } else {
                x75->frmr_data = *frame;
                x75->frmr_type = X75_FRMR_Z;
                x75_transmit_frmr(x75);
                x75_dbg(0, "(%p) S3 -> S4\n", x75->dev);
                x75_start_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state   = X75_STATE_4;
                x75->n2count = 0;
            }
            break;

        case X75_RR:
            x75_dbg(1, "(%p) S3 RX RR(%d) R%d\n",
                     x75->dev, frame->pf, frame->nr);
            x75->condition &= ~X75_PEER_RX_BUSY_CONDITION;
            x75_check_need_response(x75, frame->cr, frame->pf);
            if (x75_validate_nr(x75, frame->nr)) {
                x75_check_iframes_acked(x75, frame->nr);
            } else {
                x75->frmr_data = *frame;
                x75->frmr_type = X75_FRMR_Z;
                x75_transmit_frmr(x75);
                x75_dbg(0, "(%p) S3 -> S4\n", x75->dev);
                x75_start_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state   = X75_STATE_4;
                x75->n2count = 0;
            }
            break;

        case X75_REJ:
            x75_dbg(1, "(%p) S3 RX REJ(%d) R%d\n",
                     x75->dev, frame->pf, frame->nr);
            x75->condition &= ~X75_PEER_RX_BUSY_CONDITION;
            x75_check_need_response(x75, frame->cr, frame->pf);
            if (x75_validate_nr(x75, frame->nr)) {
                x75_frames_acked(x75, frame->nr);
                x75_stop_t1timer(x75);
                x75->n2count = 0;
                x75_requeue_frames(x75);
            } else {
                x75->frmr_data = *frame;
                x75->frmr_type = X75_FRMR_Z;
                x75_transmit_frmr(x75);
                x75_dbg(0, "(%p) S3 -> S4\n", x75->dev);
                x75_start_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state   = X75_STATE_4;
                x75->n2count = 0;
            }
            break;

        case X75_I:
            x75_dbg(1, "(%p) S3 RX I(%d) S%d R%d\n",
                     x75->dev, frame->pf, frame->ns, frame->nr);
            if (!x75_validate_nr(x75, frame->nr)) {
                x75->frmr_data = *frame;
                x75->frmr_type = X75_FRMR_Z;
                x75_transmit_frmr(x75);
                x75_dbg(0, "(%p) S3 -> S4\n", x75->dev);
                x75_start_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state   = X75_STATE_4;
                x75->n2count = 0;
                break;
            }
            if (x75->condition & X75_PEER_RX_BUSY_CONDITION)
                x75_frames_acked(x75, frame->nr);
            else
                x75_check_iframes_acked(x75, frame->nr);

            if (frame->ns == x75->vr) {
                int cn;
                cn = x75_data_indication(x75, skb);
                queued = 1;
                /*
                 * If upper layer has dropped the frame, we
                 * basically ignore any further protocol
                 * processing. This will cause the peer
                 * to re-transmit the frame later like
                 * a frame lost on the wire.
                 */
                //if (cn == NET_RX_DROP) {
                //    pr_debug("rx congestion\n");
                //    break;
                //}
                x75->vr = (x75->vr + 1) % modulus;
                x75->condition &= ~X75_REJECT_CONDITION;
                if (frame->pf)
                    x75_enquiry_response(x75);
                else {
                    if (!(x75->condition &
                          X75_ACK_PENDING_CONDITION)) {
                        x75->condition |= X75_ACK_PENDING_CONDITION;
                        x75_start_t2timer(x75);
                    }
                }
            } else {
                if (x75->condition & X75_REJECT_CONDITION) {
                    if (frame->pf)
                        x75_enquiry_response(x75);
                } else {
                    x75_dbg(1, "(%p) S3 TX REJ(%d) R%d\n",
                             x75->dev, frame->pf, x75->vr);
                    x75->condition |= X75_REJECT_CONDITION;
                    x75_send_control(x75, X75_REJ, frame->pf,
                                      X75_RESPONSE);
                    x75->condition &= ~X75_ACK_PENDING_CONDITION;
                }
            }
            break;

        case X75_FRMR:
            x75_dbg(1, "(%p) S3 RX FRMR(%d) %5ph\n",
                     x75->dev, frame->pf,
                     skb->data);
            x75_establish_data_link(x75);
            x75_dbg(0, "(%p) S3 -> S1\n", x75->dev);
            x75_requeue_frames(x75);
            x75->state = X75_STATE_1;
            break;

        case X75_ILLEGAL:
            x75_dbg(1, "(%p) S3 RX ILLEGAL(%d)\n", x75->dev, frame->pf);
            x75->frmr_data = *frame;
            x75->frmr_type = X75_FRMR_W;
            x75_transmit_frmr(x75);
            x75_dbg(0, "(%p) S3 -> S4\n", x75->dev);
            x75_start_t1timer(x75);
            x75_stop_t2timer(x75);
            x75->state   = X75_STATE_4;
            x75->n2count = 0;
            break;
    }

    if (!queued)
        msgb_free(skb);
}

/*
 *	State machine for state 4, Frame Reject State.
 *	The handling of the timer(s) is in file x75_timer.c.
 */
static void x75_state4_machine(struct x75_cb *x75, struct msgb *skb,
                                struct x75_frame *frame)
{
    switch (frame->type) {
        case X75_SABM:
            x75_dbg(1, "(%p) S4 RX SABM(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S4 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            } else {
                x75_dbg(1, "(%p) S4 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_dbg(0, "(%p) S4 -> S3\n", x75->dev);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
                x75_stop_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state     = X75_STATE_3;
                x75->condition = 0x00;
                x75->n2count   = 0;
                x75->vs        = 0;
                x75->vr        = 0;
                x75->va        = 0;
                x75_connect_indication(x75, X75_OK);
            }
            break;

        case X75_SABME:
            x75_dbg(1, "(%p) S4 RX SABME(%d)\n", x75->dev, frame->pf);
            if (x75->mode & X75_EXTENDED) {
                x75_dbg(1, "(%p) S4 TX UA(%d)\n",
                         x75->dev, frame->pf);
                x75_dbg(0, "(%p) S4 -> S3\n", x75->dev);
                x75_send_control(x75, X75_UA, frame->pf,
                                  X75_RESPONSE);
                x75_stop_t1timer(x75);
                x75_stop_t2timer(x75);
                x75->state     = X75_STATE_3;
                x75->condition = 0x00;
                x75->n2count   = 0;
                x75->vs        = 0;
                x75->vr        = 0;
                x75->va        = 0;
                x75_connect_indication(x75, X75_OK);
            } else {
                x75_dbg(1, "(%p) S4 TX DM(%d)\n",
                         x75->dev, frame->pf);
                x75_send_control(x75, X75_DM, frame->pf,
                                  X75_RESPONSE);
            }
            break;
    }

    msgb_free(skb);
}

/*
 *	Process an incoming X75 frame
 */
void x75_data_input(struct x75_cb *x75, struct msgb *skb)
{
    struct x75_frame frame;

    if (x75_decode(x75, skb, &frame) < 0) {
        msgb_free(skb);
        return;
    }

    switch (x75->state) {
        case X75_STATE_0:
            x75_state0_machine(x75, skb, &frame); break;
        case X75_STATE_1:
            x75_state1_machine(x75, skb, &frame); break;
        case X75_STATE_2:
            x75_state2_machine(x75, skb, &frame); break;
        case X75_STATE_3:
            x75_state3_machine(x75, skb, &frame); break;
        case X75_STATE_4:
            x75_state4_machine(x75, skb, &frame); break;
    }

    x75_kick(x75);
}