from scapy.all import sniff, TCP, Raw, IP
import os
import re

OUTFILE = "exfiled_testfile.txt" #output file that stores the exfiltrated file from the client
FILTER = "tcp and src host 127.0.0.1 and port 8080 and tcp[13] & 0x18 == 0x18" #filters on upload speed packets; change to your client IP if the receiver script is running on a passive observer 

pattern = re.compile(r"\*([A-Za-z0-9 ]+)\*") #lil regex to read bytes between two "*"

def handle_packet(pkt):
    if pkt.haslayer(TCP) and pkt.haslayer(Raw): #Check for TCP payload
        payload = bytes(pkt[Raw].load)  
        ascii_text = payload.decode('ascii', errors='ignore') #decode to ascii 
        matches = pattern.findall(ascii_text) #check TCP payload for data with "*"

        if matches:
            with open(OUTFILE, "a") as f:
                for m in matches:
                    f.write(m + "\n")
            print(f"Exfiled Data: {len(matches)} value(s) from {pkt[IP].src}") 
            raise KeyboardInterrupt #only reads a single packet with the exfiltrated file then quits. 

def main():
    abs_path = os.path.abspath(OUTFILE)
    print(f"Starting sniff (filter='{FILTER}'). Writing exfiled data to {abs_path}")
    sniff(iface="ens18", filter=FILTER, prn=handle_packet, store=0) #change it to your interface

if __name__ == "__main__":
    main()
