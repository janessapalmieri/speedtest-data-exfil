/*
This LKM modifies TCP PSH/ACK packets over port 8080 to calculate the exfiltration capacity of a client within a single Speedtest session.
This code should work with any Ookla Speedtest server using HTTP (e.g. http://speedtest.midco.net) or the Speedtest-CLI.
Tested on Ubuntu 24.04 with version 6.8 kernel and Ubuntu 26.04 with version 7.0 kernel.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/inet.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <net/tcp.h>
#include <net/checksum.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Janessa Palmieri");
MODULE_DESCRIPTION("Kernel module to calculate Speedtest exfiltration capacity.");

//const vars to modify; client.sh modifies these
#define SOURCE_IP "192.168.X.X"

//const vars to skip OPTIONS and POST packets over port 8080 seen with HTTP Speedtest servers
#define SEARCH_STR_OPTIONS "OPTIONS"
#define SEARCH_LEN_OPTIONS 7

#define SEARCH_STR_POST "POST"
#define SEARCH_LEN_POST 4

//avoid file exceeding TCP payload size
#define MAX_FILE_SIZE 1400

//total number of bytes exfiled
static unsigned long total_exfiled_bytes = 0;

//netfilter hook options
static struct nf_hook_ops *nfho = NULL;

//function that overwrites the TCP payload with random bytes and calculates the maximum number of bytes that can be exfiltrated in a single speedtest.
static unsigned int max_bytes_exfiled(struct sk_buff *skb, int offset, int len) {
	char *buf;

	//allocate heap memory for random bytes; atomic context
	buf = kmalloc(len, GFP_ATOMIC);
	if (!buf)
		return NF_ACCEPT;

	//fill buffer with random bytes and write into TCP payload
	get_random_bytes(buf, len);
	skb_store_bits(skb, offset, buf, len);

	//free heap memory and update total exfiltrated bytes
	kfree(buf);
	total_exfiled_bytes += len;
	return NF_ACCEPT;
}

//hook function that filters on the PSH/ACK upload speed packets, calls PoC functions, and recalculates checksums.
static unsigned int hook_func(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
	struct iphdr *ip_header;
	struct tcphdr *tcp_header;

	int ip_hdr_len;				//IP header length
	int tcp_hdr_len;			//TCP header length
	int tcp_payloadoffset;			//start of TCP payload
	int tcp_payloadlen;			//TCP payload length
	int total_len;				//total length of packet
	unsigned char *payload_pointer;		//pointer to TCP payload

	//convert source IP to bytes
	__be32 source_ip;
	char ip_source[] = SOURCE_IP;
	__be32 client_ip = in_aton(ip_source);

	//check if packet is valid
	if (!skb)
		return NF_ACCEPT;

	//ensure linear data
	if (skb_linearize(skb) < 0)
		return -ENOMEM;

	//extract IP header
	ip_header = ip_hdr(skb);
	ip_hdr_len = ip_header->ihl * 4;

	//only process TCP packets
	if (ip_header->protocol != IPPROTO_TCP)
		return NF_ACCEPT;

	//extract TCP header
	tcp_header = tcp_hdr(skb);
	tcp_hdr_len = tcp_header->doff * 4;

	//extract source IP address
	source_ip = ip_header->saddr;

	//total packet length
	total_len = ntohs(ip_header->tot_len);

	//filter on PSH/ACK upload packets over port 8080 - may hit other TCP traffic matching this condition
	if (ntohs(tcp_header->dest) == 8080 &&
		source_ip == client_ip &&
		tcp_header->fin == 0 &&
		tcp_header->syn == 0 &&
		tcp_header->rst == 0 &&
		tcp_header->ack == 1 &&
		tcp_header->psh == 1 &&
		total_len > 1500) {

		//integer offset of TCP payload
		tcp_payloadoffset = ip_hdr_len + tcp_hdr_len;

		//grab pointer to start of TCP payload
		payload_pointer = (unsigned char*)tcp_header + tcp_hdr_len;

		//skip OPTIONS and POST packets
		if (memcmp(payload_pointer, SEARCH_STR_OPTIONS, SEARCH_LEN_OPTIONS) == 0 ||
			memcmp(payload_pointer, SEARCH_STR_POST, SEARCH_LEN_POST) == 0)
			return NF_ACCEPT;

		//TCP payload length = total packet - IP header - TCP header
		tcp_payloadlen = total_len - ip_hdr_len - tcp_hdr_len;

		//call function that overwrites entire TCP payload with random bytes and calculates the max amount of bytes that can be exfiled in a single Speedtest
		max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);

		//zero out checksum
		tcp_header->check = 0;

		//IPv4: calculate partial checksum over TCP data
		if (ip_header->version == 4) {
			tcp_header->check = tcp_v4_check(
				skb->len - ip_hdr_len,
				ip_header->saddr, ip_header->daddr,
				csum_partial(tcp_header, skb->len - ip_hdr_len, 0));
		}
		skb->ip_summed = CHECKSUM_NONE;

		//recalculate IP header checksum
		if (ip_header->version == 4) {
			ip_header->check = 0;
			ip_header->check = ip_fast_csum((unsigned char *)ip_header, ip_header->ihl);
		}
	}
	return NF_ACCEPT;
}

//at module load time
static int __init LKM_init(void)
{
	//allocate and initialize netfilter hook options
	nfho = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);

	nfho->hook	= (nf_hookfn*)hook_func;	//hook function
	nfho->hooknum	= NF_INET_POST_ROUTING;		//outgoing packets
	nfho->pf	= PF_INET;			//IPv4
	nfho->priority	= NF_IP_PRI_FIRST;		//max hook priority

	//register netfilter hook
	nf_register_net_hook(&init_net, nfho);
	return 0;
}

//at module unload time
static void __exit LKM_exit(void)
{
	//unregister hook and free file data buffer
	nf_unregister_net_hook(&init_net, nfho);
	pr_info("Total bytes exfiled: %lu bytes (%lu.%lu MB)\n", total_exfiled_bytes, total_exfiled_bytes / 1000000, (total_exfiled_bytes % 1000000) / 100000);
}

module_init(LKM_init);
module_exit(LKM_exit);
