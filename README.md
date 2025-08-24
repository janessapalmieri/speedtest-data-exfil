
<!-- ABOUT THE PROJECT -->
## About The Project

This project demonstrates data exfiltration through Ookla's Speedtest. Speedtest servers using HTTP (e.g. http://speedtest.midco.net) conduct Speedtests over plaintext communications. The TCP payloads from the client -> server can be overwritten without corrupting the Speedtest because there are no data integrity checks. This makes Speedtest a strong C2 vector candidate, providing a unique opportunity to exfiltrate large datasets. 

<!-- GETTING STARTED -->
## Getting Started

`speedtest-exfil.c` is a Linux Kernel Module (LKM) utilizing the Netfilter framework as a proof-of-concept for exfiltrating data via Speedtest traffic. This LKM was tested on Ubuntu 24.04 with kernel version 6.8. 

### Prerequisites

This is an example of how to list things you need to use the software and how to install them.
* npm
  ```sh
  npm install npm@latest -g
  ```

### Installation

1. Get a free API Key at [https://example.com](https://example.com)
2. Clone the repo
   ```sh
   git clone https://github.com/github_username/repo_name.git
   ```
3. Install NPM packages
   ```sh
   npm install
   ```
4. Enter your API in `config.js`
   ```js
   const API_KEY = 'ENTER YOUR API';
   ```
5. Change git remote url to avoid accidental pushes to base project
   ```sh
   git remote set-url origin github_username/repo_name
   git remote -v # confirm the changes
   ```

<!-- USAGE EXAMPLES -->
## Usage

Use this space to show useful examples of how a project can be used. Additional screenshots, code examples and demos work well in this space. You may also link to more resources.

_For more examples, please refer to the [Documentation](https://example.com)_


<!-- ACKNOWLEDGMENTS -->
## Acknowledgments

* []()
* []()
* []()

<p align="right">(<a href="#readme-top">back to top</a>)</p>



