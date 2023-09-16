#include <stdio.h>
#include <unistd.h>
#include <osmocom/core/utils.h>

#include "yate_message.h"

void yate_message_parse_incoming(char *buf, int len)
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

    // %%>message:0x7f1882d68b10.1202973640:1694630927:call.execute::id=sig/7:module=sig:status=answered:address=auerswald/1:billid=1694630023-13:lastpeerid=wave/7:answered=true:direction=incoming:callto=external/playrec//root/yate/share/scripts/x75/x75:handlers=javascript%z15,javascript%z15,gvoice%z20,queues%z45,cdrbuild%z50,yrtp%z50,lateroute%z75,dbwave%z90,filetransfer%z90,conf%z90,jingle%z90,tone%z90,wave%z90,iax%z90,sip%z90,sig%z90,dumb%z90,analyzer%z90,mgcpgw%z90,analog%z90,callgen%z100,pbx%z100,extmodule%z100
    char *message_id_end = memchr(message_id, ':', len - (message_id - buf));
    if(message_id_end == NULL)
    {
        return;
    }
    message_id_end[0] = 0x00;
    fprintf(stderr, "%%%%<message:%s:true:\n", message_id);
    fprintf(stdout, "%%%%<message:%s:true:\n", message_id);
    fflush(stdout);
}
