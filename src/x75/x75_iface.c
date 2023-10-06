// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	X.75 Implementation
 *	Based on Linux LAPB implementation by Jonathan Naylor
 *
 *	History
 *	LAPB 001	Jonathan Naylor	Started Coding
 *	LAPB 002	Jonathan Naylor	New timer architecture.
 *	2000-10-29	Henner Eisen	x75_data_indication() return status.
 */

#include "x75_int.h"

int x75_register(struct x75_cb *x75,
                  const struct x75_register_struct *callbacks)
{
    INIT_LLIST_HEAD(&x75->write_queue);
    INIT_LLIST_HEAD(&x75->ack_queue);

    osmo_timer_setup(&x75->t1timer, NULL, 0);
    osmo_timer_setup(&x75->t2timer, NULL, 0);
    x75->t1timer_running = false;
    x75->t2timer_running = false;

    x75->t1      = X75_DEFAULT_T1;
    x75->t2      = X75_DEFAULT_T2;
    x75->n2      = X75_DEFAULT_N2;
    // DCE (inbound connection)
    x75->mode    = X75_STANDARD | X75_SLP | X75_DCE;
    x75->window  = X75_DEFAULT_WINDOW;
    x75->state   = X75_STATE_0;

    x75->callbacks = callbacks;
    x75_start_t1timer(x75);

    return X75_OK;
}

int x75_unregister(struct x75_cb *x75)
{
    x75_stop_t1timer(x75);
    x75_stop_t2timer(x75);

    x75_clear_queues(x75);

    /* Wait for running timers to stop */
    osmo_timer_del(&x75->t1timer);
    osmo_timer_del(&x75->t2timer);

    return X75_OK;
}

int x75_getparms(struct x75_cb *x75, struct x75_parms_struct *parms)
{
    parms->t1      = x75->t1 / HZ;
    parms->t2      = x75->t2 / HZ;
    parms->n2      = x75->n2;
    parms->n2count = x75->n2count;
    parms->state   = x75->state;
    parms->window  = x75->window;
    parms->mode    = x75->mode;

    if (!osmo_timer_pending(&x75->t1timer))
        parms->t1timer = 0;
    else
        parms->t1timer = (x75->t1timer.timeout.tv_sec) / HZ;

    if (!osmo_timer_pending(&x75->t2timer))
        parms->t2timer = 0;
    else
        parms->t2timer = (x75->t2timer.timeout.tv_sec) / HZ;

    return X75_OK;
}

int x75_setparms(struct x75_cb *x75, struct x75_parms_struct *parms)
{
    int rc = X75_INVALUE;

    if (parms->t1 < 1 || parms->t2 < 1 || parms->n2 < 1)
        goto out_put;

    if (x75->state == X75_STATE_0) {
        if (parms->mode & X75_EXTENDED) {
            if (parms->window < 1 || parms->window > 127)
                goto out_put;
        } else {
            if (parms->window < 1 || parms->window > 7)
                goto out_put;
        }
        x75->mode    = parms->mode;
        x75->window  = parms->window;
    }

    x75->t1    = parms->t1 * HZ;
    x75->t2    = parms->t2 * HZ;
    x75->n2    = parms->n2;

    rc = X75_OK;
    out_put:
    return rc;
}

int x75_connect_request(struct x75_cb *x75)
{
    int rc = X75_OK;

    rc = X75_OK;
    if (x75->state == X75_STATE_1)
        goto out_put;

    rc = X75_CONNECTED;
    if (x75->state == X75_STATE_3 || x75->state == X75_STATE_4)
        goto out_put;

    x75_establish_data_link(x75);

    x75_dbg(0, "(%p) S0 -> S1\n", x75->dev);
    x75->state = X75_STATE_1;

    rc = X75_OK;
    out_put:
    return rc;
}

static int __x75_disconnect_request(struct x75_cb *x75)
{
    switch (x75->state) {
        case X75_STATE_0:
            return X75_NOTCONNECTED;

        case X75_STATE_1:
            x75_dbg(1, "(%p) S1 TX DISC(1)\n", x75->dev);
            x75_dbg(0, "(%p) S1 -> S0\n", x75->dev);
            x75_send_control(x75, X75_DISC, X75_POLLON, X75_COMMAND);
            x75->state = X75_STATE_0;
            x75_start_t1timer(x75);
            return X75_NOTCONNECTED;

        case X75_STATE_2:
            return X75_OK;
    }

    x75_clear_queues(x75);
    x75->n2count = 0;
    x75_send_control(x75, X75_DISC, X75_POLLON, X75_COMMAND);
    x75_start_t1timer(x75);
    x75_stop_t2timer(x75);
    x75->state = X75_STATE_2;

    x75_dbg(1, "(%p) S3 DISC(1)\n", x75->dev);
    x75_dbg(0, "(%p) S3 -> S2\n", x75->dev);

    return X75_OK;
}

int x75_disconnect_request(struct x75_cb *x75)
{
    return __x75_disconnect_request(x75);
}

int x75_data_request(struct x75_cb *x75, struct msgb *skb)
{
    int rc = X75_NOTCONNECTED;

    if (x75->state != X75_STATE_3 && x75->state != X75_STATE_4)
        goto out_put;

    msgb_enqueue_count(&x75->write_queue, skb, &x75->write_queue_count);
    x75_kick(x75);
    rc = X75_OK;
    out_put:
    return rc;
}

int x75_data_received(struct x75_cb *x75, struct msgb *skb)
{
    x75_data_input(x75, skb);
    return X75_OK;
}

void x75_connect_confirmation(struct x75_cb *x75, int reason)
{
    if (x75->callbacks->connect_confirmation)
        x75->callbacks->connect_confirmation(x75->dev, reason);
}

void x75_connect_indication(struct x75_cb *x75, int reason)
{
    if (x75->callbacks->connect_indication)
        x75->callbacks->connect_indication(x75->dev, reason);
}

void x75_disconnect_confirmation(struct x75_cb *x75, int reason)
{
    if (x75->callbacks->disconnect_confirmation)
        x75->callbacks->disconnect_confirmation(x75->dev, reason);
}

void x75_disconnect_indication(struct x75_cb *x75, int reason)
{
    if (x75->callbacks->disconnect_indication)
        x75->callbacks->disconnect_indication(x75->dev, reason);
}

int x75_data_indication(struct x75_cb *x75, struct msgb *skb)
{
    if (x75->callbacks->data_indication)
        return x75->callbacks->data_indication(x75->dev, skb);

    msgb_free(skb);
    return -1; /* For now; must be != NET_RX_DROP */
}

int x75_data_transmit(struct x75_cb *x75, struct msgb *skb)
{
    int used = 0;

    if (x75->callbacks->data_transmit) {
        x75->callbacks->data_transmit(x75->dev, skb);
        used = 1;
    }

    return used;
}

/* Handle device status changes. */
//static int x75_device_event(struct notifier_block *this, unsigned long event,
//                             void *ptr)
//{
//    struct net_device *dev = netdev_notifier_info_to_dev(ptr);
//    struct x75_cb *x75;
//
//    if (!net_eq(dev_net(dev), &init_net))
//        return NOTIFY_DONE;
//
//    if (dev->type != ARPHRD_X25)
//        return NOTIFY_DONE;
//
//    x75 = x75_devtostruct(dev);
//    if (!x75)
//        return NOTIFY_DONE;
//
//    //spin_lock_bh(&x75->lock);
//
//    switch (event) {
//        case NETDEV_UP:
//            x75_dbg(0, "(%p) Interface up: %s\n", dev, dev->name);
//
//            if (netif_carrier_ok(dev)) {
//                x75_dbg(0, "(%p): Carrier is already up: %s\n", dev,
//                         dev->name);
//                if (x75->mode & X75_DCE) {
//                    x75_start_t1timer(x75);
//                } else {
//                    if (x75->state == X75_STATE_0) {
//                        x75->state = X75_STATE_1;
//                        x75_establish_data_link(x75);
//                    }
//                }
//            }
//            break;
//        case NETDEV_GOING_DOWN:
//            if (netif_carrier_ok(dev))
//                __x75_disconnect_request(x75);
//            break;
//        case NETDEV_DOWN:
//            x75_dbg(0, "(%p) Interface down: %s\n", dev, dev->name);
//            x75_dbg(0, "(%p) S%d -> S0\n", dev, x75->state);
//            x75_clear_queues(x75);
//            x75->state = X75_STATE_0;
//            x75->n2count   = 0;
//            x75_stop_t1timer(x75);
//            x75_stop_t2timer(x75);
//            break;
//        case NETDEV_CHANGE:
//            if (netif_carrier_ok(dev)) {
//                x75_dbg(0, "(%p): Carrier detected: %s\n", dev,
//                         dev->name);
//                if (x75->mode & X75_DCE) {
//                    x75_start_t1timer(x75);
//                } else {
//                    if (x75->state == X75_STATE_0) {
//                        x75->state = X75_STATE_1;
//                        x75_establish_data_link(x75);
//                    }
//                }
//            } else {
//                x75_dbg(0, "(%p) Carrier lost: %s\n", dev, dev->name);
//                x75_dbg(0, "(%p) S%d -> S0\n", dev, x75->state);
//                x75_clear_queues(x75);
//                x75->state = X75_STATE_0;
//                x75->n2count   = 0;
//                x75_stop_t1timer(x75);
//                x75_stop_t2timer(x75);
//            }
//            break;
//    }
//
//    //spin_unlock_bh(&x75->lock);
//    x75_put(x75);
//    return NOTIFY_DONE;
//}
