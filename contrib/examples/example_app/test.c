/*
 * Copyright (c) 2001,2002 Florian Schulze.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * test.c - This file is part of lwIP test
 *
 */

 /* C runtime includes */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "lwipcfg.h"
#include "lwipopts.h"

/* lwIP core includes */
#include "lwip/opt.h"

#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/ip4_addr.h"
#include "../../../apps/tcpecho_raw/tcpecho_raw.h"

typedef int (*WINDIVERT_INPUT)(unsigned char* payload, int len);
int windivert_poll(WINDIVERT_INPUT input_callback);
int windivert_send(void* payload, int len);

static struct netif g_windivert_netif;

err_t windivert_netif_init(struct netif* netif)
{
    (void*)netif;
    LWIP_DEBUGF(NETIF_DEBUG, ("windivert_netif_init called\n"));

    return ERR_OK;
}

err_t windivert_netif_input(struct pbuf* p, struct netif* inp)
{
    LWIP_DEBUGF(NETIF_DEBUG, ("windivert_netif_input called\n"));

    //ip_input(p, inp);

    if (p != NULL) {
        unsigned char version = IP_HDR_GET_VERSION(p->payload);
        if (version == 6) {
            return ip6_input(p, inp);
        }
        return ip4_input(p, inp);
    }
    return ERR_VAL;

    //return 0;
}

int windivert_input(unsigned char* payload, int len) {

    LWIP_DEBUGF(NETIF_DEBUG, ("windivert_input called\n"));

    struct pbuf* p = pbuf_alloc_reference((void*)payload, (unsigned short)len, PBUF_REF);

    g_windivert_netif.input(p, &g_windivert_netif);

    return 0;
}

err_t windivert_output(struct netif* netif, struct pbuf* p, const ip4_addr_t* ipaddr) {

    (void)netif;
    (void)ipaddr;

    printf("windivert_output called len=%d.\n", p->tot_len);

    windivert_send(p->payload, p->tot_len);

    return ERR_OK;
}


static void test_netif_init()
{
    
    g_windivert_netif.name[0] = 'w';
    g_windivert_netif.name[1] = '0';

    // 配置网络接口的IP地址和子网掩码
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gateway;
    ip4addr_aton("192.168.200.126", &ipaddr); // 设置IP地址为192.168.0.1
    ip4addr_aton("255.255.255.0", &netmask); // 设置子网掩码为255.255.255.0
    ip4addr_aton("192.168.200.254", &gateway); //
    // netif_set_ipaddr(&netif, &ipaddr);
     //netif_set_netmask(&netif, &netmask);
     //netif_set_gw(&netif, &gateway);

#if LWIP_IPV6
    netif_create_ip6_linklocal_address(&g_windivert_netif, 1);
    printf("ip6 linklocal address: %s\n", ip6addr_ntoa(netif_ip6_addr(&g_windivert_netif, 0)));
#endif

    netif_add(&g_windivert_netif, &ipaddr, &netmask, &gateway, NULL, windivert_netif_init, windivert_netif_input);

    // 设置output回调函数
    g_windivert_netif.output = windivert_output;

    netif_set_default(&g_windivert_netif);
    netif_set_up(&g_windivert_netif);
    netif_set_link_up(&g_windivert_netif);

    g_windivert_netif.flags &= ~(NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP); /* no ARP */
    g_windivert_netif.flags |= NETIF_FLAG_ETHERNET;                    /* but pure ethernet */


}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    lwip_init();

    test_netif_init();

    tcpecho_raw_init();

    windivert_poll(windivert_input);

    /*
    while (1)
    {
      netif.input(p, &netif);

      sys_check_timeouts();

    }
    */

    return 0;
}

static const char* err_strerr[] = {
  "Ok.",                    /* ERR_OK          0  */
  "Out of memory error.",   /* ERR_MEM        -1  */
  "Buffer error.",          /* ERR_BUF        -2  */
  "Timeout.",               /* ERR_TIMEOUT    -3  */
  "Routing problem.",       /* ERR_RTE        -4  */
  "Operation in progress.", /* ERR_INPROGRESS -5  */
  "Illegal value.",         /* ERR_VAL        -6  */
  "Operation would block.", /* ERR_WOULDBLOCK -7  */
  "Address in use.",        /* ERR_USE        -8  */
  "Already connecting.",    /* ERR_ALREADY    -9  */
  "Already connected.",     /* ERR_ISCONN     -10 */
  "Not connected.",         /* ERR_CONN       -11 */
  "Low-level netif error.", /* ERR_IF         -12 */
  "Connection aborted.",    /* ERR_ABRT       -13 */
  "Connection reset.",      /* ERR_RST        -14 */
  "Connection closed.",     /* ERR_CLSD       -15 */
  "Illegal argument."       /* ERR_ARG        -16 */
};

#ifdef  LWIP_DEBUG

const char*
lwip_strerr(err_t err)
{
    if ((err > 0) || (-err >= (err_t)LWIP_ARRAYSIZE(err_strerr))) {
        return "Unknown error.";
    }
    return err_strerr[-err];
}
#endif //  LWIP_DEBUG
