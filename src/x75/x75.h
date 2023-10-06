#ifndef	X75_H
#define	X75_H
#include <osmocom/core/msgb.h>
#include <osmocom/core/timer.h>

/* Define Link State constants. */
enum {
    X75_STATE_0,	/* Disconnected State		*/
    X75_STATE_1,	/* Awaiting Connection State	*/
    X75_STATE_2,	/* Awaiting Disconnection State	*/
    X75_STATE_3,	/* Data Transfer State		*/
    X75_STATE_4	    /* Frame Reject State		*/
};

#define	X75_OK			    0
#define	X75_BADTOKEN		1
#define	X75_INVALUE		    2
#define	X75_CONNECTED		3
#define	X75_NOTCONNECTED	4
#define	X75_REFUSED		    5
#define	X75_TIMEDOUT		6
#define	X75_NOMEM		    7

#define	X75_STANDARD		0x00
#define	X75_EXTENDED		0x01

#define	X75_SLP		        0x00
#define	X75_MLP		        0x02

#define	X75_DTE		        0x00
#define	X75_DCE		        0x04

struct x75_register_struct {
    void (*connect_confirmation)(void *dev, int reason);
    void (*connect_indication)(void *dev, int reason);
    void (*disconnect_confirmation)(void *dev, int reason);
    void (*disconnect_indication)(void *dev, int reason);
    int  (*data_indication)(void *dev, struct msgb *skb);
    void (*data_transmit)(void *dev, struct msgb *skb);
};

struct x75_parms_struct {
    unsigned int t1;
    unsigned int t1timer;
    unsigned int t2;
    unsigned int t2timer;
    unsigned int n2;
    unsigned int n2count;
    unsigned int window;
    unsigned int state;
    unsigned int mode;
};

/*
 *	Information about the current frame.
 */
struct x75_frame {
    unsigned short		type;		/* Parsed type		*/
    unsigned short		nr, ns;		/* N(R), N(S)		*/
    unsigned char		cr;		/* Command/Response	*/
    unsigned char		pf;		/* Poll/Final		*/
    unsigned char		control[2];	/* Original control data*/
};

/*
 *	The per X75 connection control structure.
 */
struct x75_cb {
    //struct list_head	node;
    void	*dev;

    /* Link status fields */
    unsigned int		mode;
    unsigned char		state;
    unsigned short		vs, vr, va;
    unsigned char		condition;
    unsigned short		n2, n2count;
    unsigned short		t1, t2;
    struct osmo_timer_list	t1timer, t2timer;
    bool			t1timer_running, t2timer_running;

    /* Internal control information */
    struct llist_head	write_queue;
    unsigned int write_queue_count;
    struct llist_head	ack_queue;
    unsigned int ack_queue_count;
    unsigned char		window;
    const struct x75_register_struct *callbacks;

    /* FRMR control information */
    struct x75_frame	frmr_data;
    unsigned char		frmr_type;

    //spinlock_t		lock;
    //refcount_t		refcnt;
};
extern int x75_register(struct x75_cb *x75,
                         const struct x75_register_struct *callbacks);
extern int x75_unregister(struct x75_cb *x75);
extern int x75_getparms(struct x75_cb *x75, struct x75_parms_struct *parms);
extern int x75_setparms(struct x75_cb *x75, struct x75_parms_struct *parms);
extern int x75_connect_request(struct x75_cb *x75);
extern int x75_disconnect_request(struct x75_cb *x75);
extern int x75_data_request(struct x75_cb *x75, struct msgb *skb);
extern int x75_data_received(struct x75_cb *x75, struct msgb *skb);

#endif