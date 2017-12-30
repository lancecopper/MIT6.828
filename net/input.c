#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

	int ret, n, i;
	struct jif_pkt *pkt;
	for(i = 0; i < QUEUE_SIZE; i++){
		if((ret = sys_page_alloc(0, PACKET_BUF(i), PTE_P | PTE_U | PTE_W)) < 0)
			panic("sys_page_alloc: %e", ret);
	}
	i = 0;
	while(true)
	{
		pkt = PACKET_BUF(i);
		while(true){
			ret = sys_try_rx_packet(&pkt->jp_data, &pkt->jp_len);
			if(ret == -1)
				sys_yield();
			else if(ret < 0)
				panic("sys_try_rx_packet: %e", ret);
			else{
				break;
			}
		}
		ipc_send(ns_envid, NSREQ_INPUT, pkt, PTE_P | PTE_U);
		i = (i + 1) % QUEUE_SIZE;
	}
}
