#pragma once

/* PPP-specific settings */

// maximum packet size allowed over the PPP
// (normal IP packets are 1500)
#define MAX_MTU 2048

/* TTY/TDD-specific settings */

// number of buffers (yate default, 20ms, 160 samples) to hold
#define TTY_MINIMODEM_BUFFER_SIZE 8

/* X.75-specific settings */

// maximum number of packets waiting to be transmitted from TCP->X.75
// (before stalling the TCP socket)
#define X75_FLOW_CONTROL_MAX_WRITE_QUEUE 8

// size of a single read() syscall on the TCP stream
// will result in X.75 data blocks of the same size
#define X75_TCP_READ_BLOCK_SIZE 128

// static buffer allocation for any received HDLC frames
#define X75_MAXIMUM_HDLC_RX_SIZE 8192