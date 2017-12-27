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
	int32_t reqno;
	static envid_t nsenv;
	if (nsenv == 0)
		nsenv = ipc_find_env(ENV_TYPE_NS);
	do{
		reqno = ipc_recv((int32_t *)&whom, &nsipcbuf, 0);
	}while(whom!= nsenv || reqno != NSREQ_OUTPUT);
	sys_tx_packet(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
}
