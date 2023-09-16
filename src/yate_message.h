#pragma once

void yate_message_parse_incoming(FILE *out, char *buf, int len);
int yate_message_read_cb(struct osmo_fd *fd, unsigned int what);
