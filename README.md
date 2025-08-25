# Data Exfiltration through Ookla's Speedtest
<!-- ABOUT THE PROJECT -->
## About the Project
This project demonstrates the possibility of data exfiltration through Ookla's Speedtest traffic. Speedtest servers using HTTP (e.g. http://speedtest.midco.net) conduct Speedtests over plaintext. The TCP payloads from the client -> server can be overwritten without corrupting the Speedtest because no data integrity checks are in place. This makes Speedtest a strong C2 vector candidate, providing a unique opportunity to exfiltrate large datasets. 

<!-- GETTING STARTED -->
## How it Works
Speedtest measures upload and download speeds by sending large random data chunks to and from the server and client. These random data chunks are a sequence of bytes not checked by the client or server, and are just used to test the Internet speed.

When a Speedtest is initiated, the initial packets observed in the traffic are the ping echo request and reply, as latency is measured first. The client then upgrades the connection from HTTP to the WebSocket protocol. The client and the server use WebSocket to exchange Ping/Pong packets as a keep-alive check. The download speed is calculated with a series of ACKs and PSH/ACKS. The client sends an ACK packet, followed by one or multiple ACK packets and a PSH/ACK packet from the server. Since the client’s ACK contains no TCP payload, it cannot be used for data exfiltration. This sequence repeats until the download speed is calculated and displayed to the user. A similar sequence takes place after the download speed is measured, this time to calculate the upload speed. Now, the client is sending the PSH/ACK packets with large TCP payloads to the server, which can be manipulated for data exfiltration. 




<!-- GETTING STARTED -->
## Getting Started

`speedtest-exfil.c` is a Linux Kernel Module (LKM) utilizing the Netfilter framework as a proof-of-concept for exfiltrating data via Speedtest traffic. This LKM was tested on Ubuntu 24.04 with kernel version 6.8. * Code still being worked on. *

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




