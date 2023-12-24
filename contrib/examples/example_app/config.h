#pragma once

typedef struct _app_config
{
    //是否启用802.1x
    char enable_8021x;

    //是否启动防火墙功能
    char no_firewall;

    //是否启用sdp
    char enable_sdp;
    char sdp_host[32];
    unsigned short sdp_port;
    char sdp_username[256];
    char sdp_password[256];

    //终端管控服务器
    char ops_host[32];
    unsigned short ops_port;

    //状态
    char sdp_ok;

    //获取用户桌面地址退出
    char get_user_desktop;
} app_config;

extern app_config g_config;
