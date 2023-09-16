#include <stdio.h>
#include <unistd.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/select.h>

#include "yate_message.h"

char yate_msg_buf[4096] = {0};
int yate_msg_buf_pos = 0;

// yate interfacing code inspired by Karl Koscher <supersat@cs.washington.edu>
void yate_message_parse_incoming(FILE *out, char *buf, int len)
{
    // we don't want to really talk to yate (just yet)
    // let's just answer it's execute call to make it happy.

    // %%>message:0x7f1882d68b10.476653321:1694630261:call.execute::id=sig/2:module=sig:status=answered:address=auerswald/1:billid=1694630023-3:lastpeerid=wave/2:answered=true:direction=incoming:callto=external/playrec//root/yate/share/scripts/x75/x75:handlers=javascript%z15,javascript%z15,gvoice%z20,queues%z45,cdrbuild%z50,yrtp%z50,lateroute%z75,dbwave%z90,filetransfer%z90,conf%z90,jingle%z90,tone%z90,wave%z90,iax%z90,sip%z90,sig%z90,dumb%z90,analyzer%z90,mgcpgw%z90,analog%z90,callgen%z100,pbx%z100,extmodule%z100
    char *message_id = memchr(buf, ':', len);
    if(message_id == NULL)
    {
        return;
    }
    // we don't want the first char (:)
    message_id += 1;

    char *message_id_end = memchr(message_id, ':', len - (message_id - buf));
    if(message_id_end == NULL)
    {
        return;
    }
    message_id_end[0] = 0x00;
    fprintf(stderr, "%%%%<message:%s:true:\n", message_id);
    fprintf(out, "%%%%<message:%s:true:\n", message_id);
    fflush(out);
}

int yate_message_read_cb(struct osmo_fd *fd, unsigned int what)
{
    ssize_t len;
    char temp_buf[4096] = {0};

    len = read(fd->fd, temp_buf, sizeof(temp_buf));
    if (len <= 0) {
        fprintf(stderr, "FD_YATE_STDIN read failed\n");
        return -1;
    }
    for (int i = 0; i < len; i++) {
        if (yate_msg_buf_pos == sizeof(yate_msg_buf) - 1)
        {
            fprintf(stderr, "Yate incoming message buffer overflowed. Aborting.\n");
            memset(yate_msg_buf, 0x00, sizeof(yate_msg_buf));
            yate_msg_buf_pos = 0;
            return -1;
        }

        yate_msg_buf[yate_msg_buf_pos] = temp_buf[i];
        yate_msg_buf_pos++;

        if (temp_buf[i] == '\n') {
            // process incoming message
            yate_msg_buf[yate_msg_buf_pos - 1] = 0x00;
            fprintf(stderr, "Yate incoming message: %s\n", yate_msg_buf);

            yate_message_parse_incoming(stdout, yate_msg_buf, yate_msg_buf_pos);

            memset(yate_msg_buf, 0x00, sizeof(yate_msg_buf));
            yate_msg_buf_pos = 0;
        }
    }

    return 0;
}