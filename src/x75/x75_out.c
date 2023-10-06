// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	X.75 Implementation
 *	Based on Linux LAPB implementation by Jonathan Naulor
 *
 *	History
 *	LAPB 001	Jonathan Naulor	Started Coding
 *	LAPB 002	Jonathan Naylor	New timer architecture.
 */

#include <linux/string.h>
#include "x75_int.h"

/*
 *  This procedure is passed a buffer descriptor for an iframe. It builds
 *  the rest of the control part of the frame and then writes it out.
 */
static void x75_send_iframe(struct x75_cb *x75, struct msgb *skb, int poll_bit)
{
    unsigned char *frame;

    if (!skb) {
        return;
    }

    if (x75->mode & X75_EXTENDED) {
        frame = msgb_push(skb, 2);

        frame[0] = X75_I;
        frame[0] |= x75->vs << 1;
        frame[1] = poll_bit ? X75_EPF : 0;
        frame[1] |= x75->vr << 1;
    } else {
        frame = msgb_push(skb, 1);

        *frame = X75_I;
        *frame |= poll_bit ? X75_SPF : 0;
        *frame |= x75->vr << 5;
        *frame |= x75->vs << 1;
    }

    x75_dbg(1, "(%p) S%d TX I(%d) S%d R%d\n",
             x75->dev, x75->state, poll_bit, x75->vs, x75->vr);

    x75_transmit_buffer(x75, skb, X75_COMMAND);
}

void x75_kick(struct x75_cb *x75)
{
    struct msgb *skb, *skbn;
    unsigned short modulus, start, end;

    modulus = (x75->mode & X75_EXTENDED) ? X75_EMODULUS : X75_SMODULUS;
    start = !x75->ack_queue_count ? x75->va : x75->vs;
    end   = (x75->va + x75->window) % modulus;

    if (!(x75->condition & X75_PEER_RX_BUSY_CONDITION) &&
        start != end && x75->write_queue_count) {
        x75->vs = start;

        /*
         * Dequeue the frame and copy it.
         */
        skb = msgb_dequeue_count(&x75->write_queue, &x75->write_queue_count);

        do {
            skbn = msgb_copy(skb, "x75_msgb");
            if (!skbn) {
                fprintf(stderr, "msgb_copy failed! unimplemented currently. aborting.");
                exit(1);
                msgb_enqueue_count(&x75->write_queue, skb, &x75->write_queue_count);
                break;
            }

            // FIXME: refcounting mechanism isn't implemented
            //if (skb->sk)
            //    skb_set_owner_w(skbn, skb->sk);

            /*
             * Transmit the frame copy.
             */
            x75_send_iframe(x75, skbn, X75_POLLOFF);

            x75->vs = (x75->vs + 1) % modulus;

            /*
             * Requeue the original data frame.
             */
            msgb_enqueue_count(&x75->ack_queue, skb, &x75->ack_queue_count);

        } while (x75->vs != end && (skb = msgb_dequeue_count(&x75->write_queue, &x75->write_queue_count)) != NULL);

        x75->condition &= ~X75_ACK_PENDING_CONDITION;

        if (!x75_t1timer_running(x75))
            x75_start_t1timer(x75);
    }
}

void x75_transmit_buffer(struct x75_cb *x75, struct msgb *skb, int type)
{
    unsigned char *ptr;

    ptr = msgb_push(skb, 1);

    if (x75->mode & X75_MLP) {
        if (x75->mode & X75_DCE) {
            if (type == X75_COMMAND)
                *ptr = X75_ADDR_C;
            if (type == X75_RESPONSE)
                *ptr = X75_ADDR_D;
        } else {
            if (type == X75_COMMAND)
                *ptr = X75_ADDR_D;
            if (type == X75_RESPONSE)
                *ptr = X75_ADDR_C;
        }
    } else {
        if (x75->mode & X75_DCE) {
            if (type == X75_COMMAND)
                *ptr = X75_ADDR_A;
            if (type == X75_RESPONSE)
                *ptr = X75_ADDR_B;
        } else {
            if (type == X75_COMMAND)
                *ptr = X75_ADDR_B;
            if (type == X75_RESPONSE)
                *ptr = X75_ADDR_A;
        }
    }

    x75_dbg(2, "(%p) S%d TX %3ph\n", x75->dev, x75->state, skb->data);

    if (!x75_data_transmit(x75, skb))
        msgb_free(skb);
}

void x75_establish_data_link(struct x75_cb *x75)
{
    x75->condition = 0x00;
    x75->n2count   = 0;

    if (x75->mode & X75_EXTENDED) {
        x75_dbg(1, "(%p) S%d TX SABME(1)\n", x75->dev, x75->state);
        x75_send_control(x75, X75_SABME, X75_POLLON, X75_COMMAND);
    } else {
        x75_dbg(1, "(%p) S%d TX SABM(1)\n", x75->dev, x75->state);
        x75_send_control(x75, X75_SABM, X75_POLLON, X75_COMMAND);
    }

    x75_start_t1timer(x75);
    x75_stop_t2timer(x75);
}

void x75_enquiry_response(struct x75_cb *x75)
{
    x75_dbg(1, "(%p) S%d TX RR(1) R%d\n",
             x75->dev, x75->state, x75->vr);

    x75_send_control(x75, X75_RR, X75_POLLON, X75_RESPONSE);

    x75->condition &= ~X75_ACK_PENDING_CONDITION;
}

void x75_timeout_response(struct x75_cb *x75)
{
    x75_dbg(1, "(%p) S%d TX RR(0) R%d\n",
             x75->dev, x75->state, x75->vr);
    x75_send_control(x75, X75_RR, X75_POLLOFF, X75_RESPONSE);

    x75->condition &= ~X75_ACK_PENDING_CONDITION;
}

void x75_check_iframes_acked(struct x75_cb *x75, unsigned short nr)
{
    if (x75->vs == nr) {
        x75_frames_acked(x75, nr);
        x75_stop_t1timer(x75);
        x75->n2count = 0;
    } else if (x75->va != nr) {
        x75_frames_acked(x75, nr);
        x75_start_t1timer(x75);
    }
}

void x75_check_need_response(struct x75_cb *x75, int type, int pf)
{
    if (type == X75_COMMAND && pf)
        x75_enquiry_response(x75);
}