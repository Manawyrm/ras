// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	X.75 Implementation
 *	Based on Linux LAPB implementation by Jonathan Naulor
 *
 *	History
 *	LAPB 001	Jonathan Naulor	Started Coding
 *	LAPB 002	Jonathan Naylor	New timer architecture.
 */

#include <stdbool.h>
#include "x75_int.h"

static void x75_t1timer_expiry(void *);
static void x75_t2timer_expiry(void *);

void x75_start_t1timer(struct x75_cb *x75)
{
    osmo_timer_del(&x75->t1timer);

    osmo_timer_setup(&x75->t1timer, &x75_t1timer_expiry, x75);

    x75->t1timer_running = true;
    osmo_timer_schedule(&x75->t1timer, 0, x75->t1 * 1000);
}

void x75_start_t2timer(struct x75_cb *x75)
{
    osmo_timer_del(&x75->t2timer);

    osmo_timer_setup(&x75->t1timer, &x75_t2timer_expiry, x75);

    x75->t2timer_running = true;
    osmo_timer_schedule(&x75->t2timer, 0, x75->t2 * 1000);
}

void x75_stop_t1timer(struct x75_cb *x75)
{
    x75->t1timer_running = false;
    osmo_timer_del(&x75->t1timer);
}

void x75_stop_t2timer(struct x75_cb *x75)
{
    x75->t2timer_running = false;
    osmo_timer_del(&x75->t2timer);
}

int x75_t1timer_running(struct x75_cb *x75)
{
    return x75->t1timer_running;
}

static void x75_t2timer_expiry(void *t)
{
    struct x75_cb *x75 = t;

    //spin_lock_bh(&x75->lock);
    if (osmo_timer_pending(&x75->t2timer)) /* A new timer has been set up */
        goto out;
    if (!x75->t2timer_running) /* The timer has been stopped */
        goto out;

    if (x75->condition & X75_ACK_PENDING_CONDITION) {
        x75->condition &= ~X75_ACK_PENDING_CONDITION;
        x75_timeout_response(x75);
    }
    x75->t2timer_running = false;

    out:
    return;
    //spin_unlock_bh(&x75->lock);
}

static void x75_t1timer_expiry(void *t)
{
    struct x75_cb *x75 = t;

    //spin_lock_bh(&x75->lock);
    if (osmo_timer_pending(&x75->t1timer)) /* A new timer has been set up */
        goto out;
    if (!x75->t1timer_running) /* The timer has been stopped */
        goto out;

    switch (x75->state) {

        /*
         *	If we are a DCE, send DM up to N2 times, then switch to
         *	STATE_1 and send SABM(E).
         */
        case X75_STATE_0:
            if (x75->mode & X75_DCE &&
                x75->n2count != x75->n2) {
                x75->n2count++;
                x75_send_control(x75, X75_DM, X75_POLLOFF, X75_RESPONSE);
            } else {
                x75->state = X75_STATE_1;
                x75_establish_data_link(x75);
            }
            break;

            /*
             *	Awaiting connection state, send SABM(E), up to N2 times.
             */
        case X75_STATE_1:
            if (x75->n2count == x75->n2) {
                x75_clear_queues(x75);
                x75->state = X75_STATE_0;
                x75_disconnect_indication(x75, X75_TIMEDOUT);
                x75_dbg(0, "(%p) S1 -> S0\n", x75->dev);
                x75->t1timer_running = false;
                goto out;
            } else {
                x75->n2count++;
                if (x75->mode & X75_EXTENDED) {
                    x75_dbg(1, "(%p) S1 TX SABME(1)\n",
                             x75->dev);
                    x75_send_control(x75, X75_SABME, X75_POLLON, X75_COMMAND);
                } else {
                    x75_dbg(1, "(%p) S1 TX SABM(1)\n",
                             x75->dev);
                    x75_send_control(x75, X75_SABM, X75_POLLON, X75_COMMAND);
                }
            }
            break;

            /*
             *	Awaiting disconnection state, send DISC, up to N2 times.
             */
        case X75_STATE_2:
            if (x75->n2count == x75->n2) {
                x75_clear_queues(x75);
                x75->state = X75_STATE_0;
                x75_disconnect_confirmation(x75, X75_TIMEDOUT);
                x75_dbg(0, "(%p) S2 -> S0\n", x75->dev);
                x75->t1timer_running = false;
                goto out;
            } else {
                x75->n2count++;
                x75_dbg(1, "(%p) S2 TX DISC(1)\n", x75->dev);
                x75_send_control(x75, X75_DISC, X75_POLLON, X75_COMMAND);
            }
            break;

            /*
             *	Data transfer state, restransmit I frames, up to N2 times.
             */
        case X75_STATE_3:
            if (x75->n2count == x75->n2) {
                x75_clear_queues(x75);
                x75->state = X75_STATE_0;
                x75_stop_t2timer(x75);
                x75_disconnect_indication(x75, X75_TIMEDOUT);
                x75_dbg(0, "(%p) S3 -> S0\n", x75->dev);
                x75->t1timer_running = false;
                goto out;
            } else {
                x75->n2count++;
                x75_requeue_frames(x75);
                x75_kick(x75);
            }
            break;

            /*
             *	Frame reject state, restransmit FRMR frames, up to N2 times.
             */
        case X75_STATE_4:
            if (x75->n2count == x75->n2) {
                x75_clear_queues(x75);
                x75->state = X75_STATE_0;
                x75_disconnect_indication(x75, X75_TIMEDOUT);
                x75_dbg(0, "(%p) S4 -> S0\n", x75->dev);
                x75->t1timer_running = false;
                goto out;
            } else {
                x75->n2count++;
                x75_transmit_frmr(x75);
            }
            break;
    }

    x75_start_t1timer(x75);

    out:
        return;
    //spin_unlock_bh(&x75->lock);
}