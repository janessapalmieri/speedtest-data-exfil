# Data Exfiltration through Ookla's Speedtest
<!-- ABOUT THE PROJECT -->
## About the Project

This project demonstrates data exfiltration through Ookla's Speedtest. Speedtest servers using HTTP (e.g. http://speedtest.midco.net) conduct Speedtests over plaintext communications. The TCP payloads from the client -> server can be overwritten without corrupting the Speedtest because there are no data integrity checks. This makes Speedtest a strong C2 vector candidate, providing a unique opportunity to exfiltrate large datasets. 

<!-- GETTING STARTED -->
## Getting Started

`speedtest-exfil.c` is a Linux Kernel Module (LKM) utilizing the Netfilter framework as a proof-of-concept for exfiltrating data via Speedtest traffic. This LKM was tested on Ubuntu 24.04 with kernel version 6.8. 

### Prerequisites

It is highly recommended to run this code within a virtual machine with at least 16GB of RAM.

### Installation

1. Clone the repo
   ```sh
   git clone https://github.com/janessapalmieri/speedtest-data-exfil.git
   ```
2. Modify the `speedtest-exfil.c` to include your source IP address and the test file you want to exfiltrate. 
3. Make
   ```sh
   make
   ```
4. Insert LKM
   ```sh
   insmod speedtest-exfil.ko
   ```
5. Navigate to a Speedtest HTTP server and hit Go
6. Observe the packets using Wireshark (receiver script coming soon!)
7. Remove LKM 
   ```sh
   rmmod speedtest-exfil.ko
   ```
8. Clean
   ```sh
   make clean
   ```   
<!-- USAGE EXAMPLES -->
## Usage

Use this space to show useful examples of how a project can be used. Additional screenshots, code examples and demos work well in this space. You may also link to more resources.

<!-- ACKNOWLEDGMENTS -->
## Acknowledgments

I want to thank my mentor, Dr. Andrew Kramer of Dakota State University, for providing the original idea that inspired this project and for their continuous guidance and support throughout my career in cybersecurity. 




