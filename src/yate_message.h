#pragma once

void yate_message_parse_incoming(FILE *out, char *buf, int len);
void yate_message_read_from_fd(int fd_in, FILE *out);