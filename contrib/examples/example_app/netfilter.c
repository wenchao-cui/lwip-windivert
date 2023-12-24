#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "windivert.h"
//#pragma comment(lib,"WinDivert.lib")

#include "log.h"
#include "config.h"

#define MAXBUF              0xFFFF
#define INET6_ADDRSTRLEN    45
#define IPPROTO_ICMPV6      58

//#include "../plugin.h"

typedef int (*WINDIVERT_INPUT)(unsigned char* payload, int len);

int g_exit = 0;

HANDLE handle = INVALID_HANDLE_VALUE;

WINDIVERT_ADDRESS recv_addr;

void print_packet(void* payload ,int len, int outbound) {

	PWINDIVERT_IPHDR ip_header;
	PWINDIVERT_IPV6HDR ipv6_header;
	PWINDIVERT_TCPHDR tcp_header;
	PWINDIVERT_UDPHDR udp_header;

	UINT payload_len;

	char src_ip[128];
	char dest_ip[128];


	// Print info about the matching packet.
	WinDivertHelperParsePacket(payload, len, &ip_header, &ipv6_header,
		NULL, NULL, NULL, &tcp_header, &udp_header, NULL,
		&payload_len, NULL, NULL);

	if (ip_header != NULL)
	{

		WinDivertHelperFormatIPv4Address(WinDivertHelperNtohl(ip_header->SrcAddr), src_ip, sizeof(src_ip));
		WinDivertHelperFormatIPv4Address(WinDivertHelperNtohl(ip_header->DstAddr), dest_ip, sizeof(dest_ip));
	}
	else if (ipv6_header != NULL) {

		WinDivertHelperNtohIPv6Address(ipv6_header->SrcAddr, (UINT*)src_ip);
		WinDivertHelperFormatIPv6Address((UINT*)src_ip, src_ip, sizeof(src_ip));

		WinDivertHelperNtohIPv6Address(ipv6_header->DstAddr, (UINT*)dest_ip);
		WinDivertHelperFormatIPv6Address((UINT*)dest_ip, dest_ip, sizeof(dest_ip));
	}

	if (tcp_header != NULL) {

		unsigned short src_port = WinDivertHelperNtohs(tcp_header->SrcPort);
		unsigned short dest_port = WinDivertHelperNtohs(tcp_header->DstPort);

		if(outbound)
			printf("WinDivertSend TCP %s:%d => %s:%d SEQ=%u checksum=0x%02x valid=%d\n"
				, src_ip, src_port
				, dest_ip, dest_port
				, tcp_header->SeqNum
				, tcp_header->Checksum, recv_addr.TCPChecksum);
		else
			printf("WinDivertRecv TCP %s:%d => %s:%d SEQ=%u checksum=0x%02x valid=%d\n"
				, src_ip, src_port
				, dest_ip, dest_port
				, tcp_header->SeqNum
				, tcp_header->Checksum, recv_addr.TCPChecksum);
	}


}

int windivert_poll(WINDIVERT_INPUT input_callback)
{
	INT16 priority = 0;
	unsigned char packet[MAXBUF];
	UINT packet_len;

	const char* err_str;


	//char* filter = "outbound and ip";
	char* filter = "tcp.DstPort==8888 || tcp.SrcPort==8888  ";
	//char* filter = "ip";

	// Divert traffic matching the filter:
	handle = WinDivertOpen(filter, WINDIVERT_LAYER_NETWORK, priority, 0);
	if (handle == INVALID_HANDLE_VALUE)
	{

		if (GetLastError() == ERROR_INVALID_PARAMETER &&
			!WinDivertHelperCompileFilter(filter, WINDIVERT_LAYER_NETWORK,
				NULL, 0, &err_str, NULL))
		{
			log_error("netfilter WinDivertOpen error: invalid filter \"%s\"\n", err_str);
			return -1;
		}


		log_error("netfilter WinDivertOpen error: failed to open the WinDivert device (%d)\n",
			GetLastError());

		return -1;
	}

	log_info("netfilter WinDivertOpen OK\n");

	// Main loop:
	while (g_exit == 0)
	{

		// Read a matching packet.
		if (!WinDivertRecv(handle, packet, sizeof(packet), &packet_len,
			&recv_addr))
		{
			log_error("netfilter WinDivertRecv warning: failed to read packet\n");
			continue;
		}

		print_packet(packet, packet_len, 0);

		//如果是本机发出来的包，有可能TCP checksum 还没有计算
		if (recv_addr.TCPChecksum == 0)
		{
			WinDivertHelperCalcChecksums(&packet, sizeof(packet), &recv_addr, 0);
		}

		//用户态协议栈自己处理
		input_callback(packet, packet_len);

		//全都丢包
	}


	log_info("netfilter WinDivertClose\n");

	WinDivertClose(handle);

	return 0;
}

int windivert_send(void* payload, int len)
{
	//INT16 priority = 0;

	//const char* err_str;
	WINDIVERT_ADDRESS send_addr = {
		.Layer = WINDIVERT_LAYER_NETWORK,
		.Outbound =1,
	};


	//char src_ip[128];
	//char dest_ip[128];

	if (handle == INVALID_HANDLE_VALUE) {
		return 0;
	}

	//print_packet(payload, len, 1);

	//memcpy((void*)&send_addr, (void*)&recv_addr, sizeof(WINDIVERT_ADDRESS));
	//For outbound injected packets, the IfIdx and SubIfIdx fields are currently ignored
	QueryPerformanceCounter((LARGE_INTEGER*) & send_addr.Timestamp);
	//Loopback
	//IPV6

	//如果开启了IP checksum offload 没必要计算IPChecksum
	//Header Checksum: 0x0000 incorrect, should be 0xa975(may be caused by "IP checksum offload"?)
	send_addr.IPChecksum = 1;
	send_addr.TCPChecksum = 1;
	send_addr.UDPChecksum = 1;

	//如果网卡没有做checksum offload 重新计算checksum
	if (send_addr.IPChecksum || send_addr.TCPChecksum) {
		WinDivertHelperCalcChecksums((PVOID)payload, MAXBUF, &send_addr, 0);
	}

	if (!WinDivertSend(handle, (PVOID)payload, len,
		NULL, &send_addr))
	{
		fprintf(stderr, "warning: failed to send TCP reset (%d)\n",
			GetLastError());
	}

	return len;
}

#ifdef _APP_STANDALONE
void main()
{
	do_netfilter("192.168.200.92");

	while (1)
	{
		Sleep(1000);
	}
}
#endif
