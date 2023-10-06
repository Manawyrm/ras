#pragma once
int minimodem_run_tty_tx(int *minimodem_pid, int *data_in_fd, int *sample_out_fd);
int minimodem_run_tty_rx(int *minimodem_pid, int *sample_in_fd, int *data_out_fd);
