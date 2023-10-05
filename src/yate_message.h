#pragma once
#include <stdio.h>
#include <stdbool.h>

bool yate_message_parse_incoming(FILE *out, char *buf, int len);
int yate_message_read_cb(struct osmo_fd *fd, unsigned int what);
char *yate_get_called();
char *yate_get_caller();
char *yate_get_format();