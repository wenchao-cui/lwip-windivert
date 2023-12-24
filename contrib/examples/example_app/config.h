#pragma once

typedef struct _app_config
{
    //�Ƿ�����802.1x
    char enable_8021x;

    //�Ƿ���������ǽ����
    char no_firewall;

    //�Ƿ�����sdp
    char enable_sdp;
    char sdp_host[32];
    unsigned short sdp_port;
    char sdp_username[256];
    char sdp_password[256];

    //�ն˹ܿط�����
    char ops_host[32];
    unsigned short ops_port;

    //״̬
    char sdp_ok;

    //��ȡ�û������ַ�˳�
    char get_user_desktop;
} app_config;

extern app_config g_config;
