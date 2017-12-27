#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	uint32_t whom;
	int ret;
	while(true)
	{
		if(ipc_recv((int32_t *)&whom, &nsipcbuf, 0) != NSREQ_OUTPUT || whom != ns_envid)
			continue;
		while(true){
			ret = sys_try_tx_packet(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
			if(ret == -1)
				sys_yield();
			else if(ret < 0)
				panic("sys_try_tx_packet: %e", ret);
			else
				break;
		}
	}
}
