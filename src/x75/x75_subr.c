// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	X.75 Implementation
 *	Based on Linux LAPB implementation by Jonathan Naylor
 *
 *	History
 *	LAPB 001	Jonathan Naylor	Started Coding
 */

#include <linux/string.h>
#include "x75_int.h"

/*
 *	This routine purges all the queues of frames.
 */
void x75_clear_queues(struct x75_cb *x75)
{
    msgb_queue_free(&x75->write_queue);
    msgb_queue_free(&x75->ack_queue);
}

/*
 * This routine purges the input queue of those frames that have been
 * acknowledged. This replaces the boxes labelled "V(a) <- N(r)" on the
 * SDL diagram.
 */
void x75_frames_acked(struct x75_cb *x75, unsigned short nr)
{
    struct msgb *skb;
    int modulus;

    modulus = (x75->mode & X75_EXTENDED) ? X75_EMODULUS : X75_SMODULUS;

    /*
     * Remove all the ack-ed frames from the ack queue.
     */
    if (x75->va != nr)
        while (x75->ack_queue_count && x75->va != nr) {
            skb = msgb_dequeue_count(&x75->ack_queue, &x75->ack_queue_count);
            msgb_free(skb);
            x75->va = (x75->va + 1) % modulus;
        }
}

void x75_requeue_frames(struct x75_cb *x75)
{
    struct msgb *skb, *skb_prev = NULL;

    /*
     * Requeue all the un-ack-ed frames on the output queue to be picked
     * up by x75_kick called from the timer. This arrangement handles the
     * possibility of an empty output queue.
     */
    while ((skb = msgb_dequeue_count(&x75->ack_queue, &x75->ack_queue_count)) != NULL) {
        // FIXME: unclear logic here...
        msgb_enqueue_count(&x75->write_queue, skb, &x75->write_queue_count);

        //if (!skb_prev)
        //    skb_queue_head(&x75->write_queue, skb);
        //else
        //    skb_append(skb_prev, skb, &x75->write_queue);
        skb_prev = skb;
    }
}

/*
 *	Validate that the value of nr is between va and vs. Return true or
 *	false for testing.
 */
int x75_validate_nr(struct x75_cb *x75, unsigned short nr)
{
    unsigned short vc = x75->va;
    int modulus;

    modulus = (x75->mode & X75_EXTENDED) ? X75_EMODULUS : X75_SMODULUS;

    while (vc != x75->vs) {
        if (nr == vc)
            return 1;
        vc = (vc + 1) % modulus;
    }

    return nr == x75->vs;
}

/*
 *	This routine is the centralised routine for parsing the control
 *	information for the different frame formats.
 */
int x75_decode(struct x75_cb *x75, struct msgb *skb,
                struct x75_frame *frame)
{
    frame->type = X75_ILLEGAL;

    x75_dbg(2, "(%p) S%d RX %3ph\n", x75->dev, x75->state, skb->data);

    /* We always need to look at 2 bytes, sometimes we need
     * to look at 3 and those cases are handled below.
     */
    if (msgb_headlen(skb) < 2)
    {
        x75_dbg(2, "msgb_headlen(skb) < 2\n");
        return -1;
    }

    if (x75->mode & X75_MLP) {
        if (x75->mode & X75_DCE) {
            if (skb->data[0] == X75_ADDR_D)
                frame->cr = X75_COMMAND;
            if (skb->data[0] == X75_ADDR_C)
                frame->cr = X75_RESPONSE;
        } else {
            if (skb->data[0] == X75_ADDR_C)
                frame->cr = X75_COMMAND;
            if (skb->data[0] == X75_ADDR_D)
                frame->cr = X75_RESPONSE;
        }
    } else {
        if (x75->mode & X75_DCE) {
            if (skb->data[0] == X75_ADDR_B)
                frame->cr = X75_COMMAND;
            if (skb->data[0] == X75_ADDR_A)
                frame->cr = X75_RESPONSE;
        } else {
            if (skb->data[0] == X75_ADDR_A)
                frame->cr = X75_COMMAND;
            if (skb->data[0] == X75_ADDR_B)
                frame->cr = X75_RESPONSE;
        }
    }

    msgb_pull(skb, 1);

    if (x75->mode & X75_EXTENDED) {
        if (!(skb->data[0] & X75_S)) {
            if (msgb_headlen(skb) < 2)
            {
                x75_dbg(2, "msgb_headlen(skb) < 2\n");
                return -1;
            }
            /*
             * I frame - carries NR/NS/PF
             */
            frame->type       = X75_I;
            frame->ns         = (skb->data[0] >> 1) & 0x7F;
            frame->nr         = (skb->data[1] >> 1) & 0x7F;
            frame->pf         = skb->data[1] & X75_EPF;
            frame->control[0] = skb->data[0];
            frame->control[1] = skb->data[1];
            msgb_pull(skb, 2);
        } else if ((skb->data[0] & X75_U) == 1) {
            if (msgb_headlen(skb) < 2)
            {
                x75_dbg(2, "msgb_headlen(skb) < 2\n");
                return -1;
            }
            /*
             * S frame - take out PF/NR
             */
            frame->type       = skb->data[0] & 0x0F;
            frame->nr         = (skb->data[1] >> 1) & 0x7F;
            frame->pf         = skb->data[1] & X75_EPF;
            frame->control[0] = skb->data[0];
            frame->control[1] = skb->data[1];
            msgb_pull(skb, 2);
        } else if ((skb->data[0] & X75_U) == 3) {
            /*
             * U frame - take out PF
             */
            frame->type       = skb->data[0] & ~X75_SPF;
            frame->pf         = skb->data[0] & X75_SPF;
            frame->control[0] = skb->data[0];
            frame->control[1] = 0x00;
            msgb_pull(skb, 1);
        }
    } else {
        if (!(skb->data[0] & X75_S)) {
            /*
             * I frame - carries NR/NS/PF
             */
            frame->type = X75_I;
            frame->ns   = (skb->data[0] >> 1) & 0x07;
            frame->nr   = (skb->data[0] >> 5) & 0x07;
            frame->pf   = skb->data[0] & X75_SPF;
        } else if ((skb->data[0] & X75_U) == 1) {
            /*
             * S frame - take out PF/NR
             */
            frame->type = skb->data[0] & 0x0F;
            frame->nr   = (skb->data[0] >> 5) & 0x07;
            frame->pf   = skb->data[0] & X75_SPF;
        } else if ((skb->data[0] & X75_U) == 3) {
            /*
             * U frame - take out PF
             */
            frame->type = skb->data[0] & ~X75_SPF;
            frame->pf   = skb->data[0] & X75_SPF;
        }

        frame->control[0] = skb->data[0];

        msgb_pull(skb, 1);
    }

    return 0;
}

/*
 *	This routine is called when the HDLC layer internally  generates a
 *	command or  response  for  the remote machine ( eg. RR, UA etc. ).
 *	Only supervisory or unnumbered frames are processed, FRMRs are handled
 *	by x75_transmit_frmr below.
 */
void x75_send_control(struct x75_cb *x75, int frametype,
                       int poll_bit, int type)
{
    struct msgb *skb;
    unsigned char  *dptr;

    if ((skb = msgb_alloc(X75_HEADER_LEN + 3, "x75_send_control")) == NULL)
        return;

    msgb_reserve(skb, X75_HEADER_LEN + 1);

    if (x75->mode & X75_EXTENDED) {
        if ((frametype & X75_U) == X75_U) {
            dptr   = msgb_put(skb, 1);
            *dptr  = frametype;
            *dptr |= poll_bit ? X75_SPF : 0;
        } else {
            dptr     = msgb_put(skb, 2);
            dptr[0]  = frametype;
            dptr[1]  = (x75->vr << 1);
            dptr[1] |= poll_bit ? X75_EPF : 0;
        }
    } else {
        dptr   = msgb_put(skb, 1);
        *dptr  = frametype;
        *dptr |= poll_bit ? X75_SPF : 0;
        if ((frametype & X75_U) == X75_S)	/* S frames carry NR */
            *dptr |= (x75->vr << 5);
    }

    x75_transmit_buffer(x75, skb, type);
}

/*
 *	This routine generates FRMRs based on information previously stored in
 *	the X75 control block.
 */
void x75_transmit_frmr(struct x75_cb *x75)
{
    struct msgb *skb;
    unsigned char  *dptr;

    if ((skb = msgb_alloc(X75_HEADER_LEN + 7, "x75_transmit_frmr")) == NULL)
        return;

    msgb_reserve(skb, X75_HEADER_LEN + 1);

    if (x75->mode & X75_EXTENDED) {
        dptr    = msgb_put(skb, 6);
        *dptr++ = X75_FRMR;
        *dptr++ = x75->frmr_data.control[0];
        *dptr++ = x75->frmr_data.control[1];
        *dptr++ = (x75->vs << 1) & 0xFE;
        *dptr   = (x75->vr << 1) & 0xFE;
        if (x75->frmr_data.cr == X75_RESPONSE)
            *dptr |= 0x01;
        dptr++;
        *dptr++ = x75->frmr_type;

        x75_dbg(1, "(%p) S%d TX FRMR %5ph\n",
                 x75->dev, x75->state,
                 &skb->data[1]);
    } else {
        dptr    = msgb_put(skb, 4);
        *dptr++ = X75_FRMR;
        *dptr++ = x75->frmr_data.control[0];
        *dptr   = (x75->vs << 1) & 0x0E;
        *dptr  |= (x75->vr << 5) & 0xE0;
        if (x75->frmr_data.cr == X75_RESPONSE)
            *dptr |= 0x10;
        dptr++;
        *dptr++ = x75->frmr_type;

        x75_dbg(1, "(%p) S%d TX FRMR %3ph\n",
                 x75->dev, x75->state, &skb->data[1]);
    }

    x75_transmit_buffer(x75, skb, X75_RESPONSE);
}