/*
 * Author: jawsper
 * Author: drbytes for the NTP part (https://tkkrlab.nl/wiki/DoorAccess)
 * Copyright (c) 2014 All rights reserved.
 * License: GPL Version 3
 *
 * This file is released under GPL-v3.0, the license is found at http://www.gnu.org/copyleft/gpl.html
 * 
 */

#include "c_types.h" // uint8_t
#include "osapi.h" // os_sprintf

#include "mem.h"
#include "ip_addr.h"
#include "espconn.h"
#include "at.h"
#include "at_ntp.h"

#define NTP_PORT 123
#define NTP_ADDR "10.42.1.1"
#define NTP_PACKET_SIZE 48

static struct espconn *pCon = 0;
uint8_t packetBuffer[NTP_PACKET_SIZE];

void close_conn()
{
	// close this stuff
	espconn_delete(pCon);
	os_free(pCon->proto.udp);
	os_free(pCon);
	pCon = 0;
}

static void ICACHE_FLASH_ATTR udp_recv(void *arg, char *pdata, unsigned short len)
{
	char temp[128];
	
	unsigned long timeStamp = pdata[40];     //the timestamp starts at byte 40 of the received packet and is four bytes long.
	timeStamp = timeStamp << 8 | pdata[41];
	timeStamp = timeStamp << 8 | pdata[42];
	timeStamp = timeStamp << 8 | pdata[43];
	timeStamp -=  2208988800UL;						//timestamp is seconds since 1-1-1900. Add 70 years to get epoch, time since 1-1-1970
	timeStamp += 3UL;								//add 2 seconds because we requested a packet about 2 seconds ago and another second for network delays/fractions etc (estimate).
	
	os_sprintf(temp, "TIME: %lu\r\n", timeStamp);
	uart0_sendStr(temp);
	
	close_conn();
	uart0_sendStr("Connection closed\r\n");
}

static void ICACHE_FLASH_ATTR udp_sent_cb(void *arg)
{
	// not relevant
}

void at_exeCmdNtp(uint8_t id)
{
	if(pCon != 0)
	{
		at_backError;
		return;
	}
	uart0_sendStr("Sending NTP packet!\r\n");
	pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
	pCon->type = ESPCONN_UDP;
	pCon->state = ESPCONN_NONE;
	pCon->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pCon->proto.udp->local_port = espconn_port();
	pCon->proto.udp->remote_port = NTP_PORT;
	uint32_t ip;
	ip = ipaddr_addr(NTP_ADDR);
	os_memcpy(pCon->proto.udp->remote_ip, &ip, 4);
	
    espconn_regist_recvcb(pCon, udp_recv);
    espconn_regist_sentcb(pCon, udp_sent_cb);
	espconn_create(pCon);
	
	os_memset(packetBuffer, 0, NTP_PACKET_SIZE);	// clear buffer
	// Initialize values needed to form NTP request
	packetBuffer[0] = 0b11100011;   	// LI, Version, Mode
	packetBuffer[1] = 0;     			// Stratum, or type of clock
	packetBuffer[2] = 6;     			// Polling Interval
	packetBuffer[3] = 0xEC;  			// Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;
	// all NTP fields have been given values, now
	
	espconn_sent(pCon, packetBuffer, NTP_PACKET_SIZE);
	
	uart0_sendStr("UDP packet sent!\r\n");
}
void at_exeCmdNtpCancel(uint8_t id)
{
	if(pCon != 0) close_conn();
	uart0_sendStr("OK\r\n");
}