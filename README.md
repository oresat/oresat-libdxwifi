# oresat-dxwifi-software

[![Build/Test](https://github.com/oresat/oresat-dxwifi-software/actions/workflows/build_test.yml/badge.svg)](https://github.com/oresat/oresat-dxwifi-software/actions/workflows/build_test.yml)

Software to construct, transmit, and receive FEC encoded data packets via packet injection and monitor mode using the AR9271 chip.

<details open="open">
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#prerequisites">Prerequisites</a>
    </li>
    <li>
      <a href="#building">Building</a>
    </li>
    <li>
      <a href="#installation-and-packaging">Installation and Packaging</a>
    </li>
    <li>
      <a href="#usage">Usage</a>
      <ul>
        <li><a href="#transmit--receive">Transmit / Receive</a></li>
        <li><a href="#encode--decode">Encode / Decode</a></li>
      </ul>
    </li>
    <li><a href="#whats-in-this-repo">What's in this Repo?</a></li>
    <li><a href="#system-tests">System Tests</a></li>
    <li><a href="#cross-compilation">Cross Compilation</a></li>
  </ol>
</details>


## Prerequisites

- 2 linux capable devices (Raspberry PI, Beaglebone Black, Your laptop etc.)
- 2 WiFi adapters capable of monitor mode (Only tested on Atheros AR9271 chipset)
- [CMake](https://cmake.org/)
- [`libpcap-dev`](https://www.tcpdump.org/), [`libgpiod-dev`](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/), [`iw`](https://wireless.wiki.kernel.org/en/users/documentation/iw), [`ip`](https://linux.die.net/man/8/ip)

## Building

These programs use [cmake](https://cmake.org/) to generate the correct build files. If 
you need to generate a build for a specific platform then I suggest checking out the docs. 
For basic GNU Makefiles the following should get you going:

First, make sure the submodules are updated:
```
git submodule update --init --recursive
```

Then generate build scripts and make the executables

```bash
mkdir build
cd build && cmake ..
make 
```

Alternatively, to make multiple out-of-source builds you can do the following 
with any of the default configurations: *Debug, Release, RelWithDebInfo, MinSizeRel*
```
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
```

The executables are located in `bin` and libraries in `lib`. You can also build a 
specific target with 

```
cmake --build --target dxwifi
```

**Note**: If you're using VSCode I **highly** recommend just using the 
[CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
extension. 

## Installation and Packaging

With the project built and cmake files present in the `build` directory you do the following:

```
make install
```

This will install the `rx`, `tx`, `encode`, `decode` and `startmonitor` programs into `/usr/bin` as well as define the `oresat-dxwifi-txd.service`
daemon that you can start with `systemctl start oresat-dxwifi-txd.service`.

To build debian packages to install you can run the following in the `build` directory:

```
cpack  # or `make package`
```

This will create two debian packages `oresat-dxwifi-rx_<version>_<arch>.deb` and `oresat-dxwifi-tx_<version>_<arch>.deb`. The **RX** package just 
installs the `rx` program. The **TX** package installs `tx`, `startmonitor` and the `oresat-dxwifi-txd.service` daemon.

**Note**: The `oresat-dxwifi-txd.service` daemon is currently configured for testing on OreSat 0, as such all it does is transmit a test sequence of bytes ad infinitum or until stopped. 

## Usage

### _Transmit / Receive_

On the receiver run
```
sudo ./startMonitor.sh mon0
sudo ./rx --dev mon0 copy.md
```

On the transmitter 
```
sudo ./startMonitor.sh mon0
sudo ./tx --dev mon0 README.md
```

Congratulations you just copied this readme to the receiving device! 
(*Note* change `mon0` to the name of *your* network device)

Both the receiver and transmitter support a variety of different options. You can get a full list
of options with the `--help` option. 

Here's an example where we set the receiver to output received files in the `test/` directory with the file format of `test_n.raw`,
timeout after 10 seconds without receiving a packet, log everything, strictly order the packets and fill in missing packets with noise, and 
only capture packets whose addressed to 11:22:33:44:55:66.
```
sudo ./rx --dev mon0 -v 6 --ordered --add-noise --timeout 10 --extension raw --prefix test --filter="wlan addr1 11:22:33:44:55:66" test/
```

And for the transmitter we set it to transmit everything in the `dxwifi` directory matching the glob pattern `*.md` and listen for new files, timeout after 20 seconds
of no new files, transmit each file into 512 byte blocks, send 5 redundant control frames, and delay 10ms between each tranmission block and 10ms between each file transmission.
```
sudo ./tx --dev mon0 --blocksize 512 --redundancy 5 --delay 10 --file-delay 10 --filter "*.md" --include-all --watch-timeout 20 dxwifi/
``` 

**Note**: When doing multi-file transmission like the example above, it's critical to set the `--file-delay` and `--redundancy` parameters 
to something reasonable for your channel. If these parameters are not set then file boundaries will not be clearly delimited to the receiver.

### *Encode / Decode*

To FEC encode a file at a specific code rate: 

```
./encode <input filename> -o <output filename> -c <code rate>
```

```-o <output filename>``` Writes to file specified.  
**Note**:	If flag is not given, will write directly to standard out.

```-c <number>``` Set code rate to a number between 0.0 and 1.0.
	
  The code rate is the rate that repair symbols will be added to the source data.
  
  **Note**: This will spit out an error if the code rate is too low for the data being encoded.
  OpenFEC can only handle a maximum of 50,000 repair symbols before it will internally throw an error.
  
  **Workaround**: increase the code rate (for example, if the initial code rate was .50, change the code rate to .75).
  If more than 50,000 repair symbols are needed, ```OF_LDPC_STAIRCASE_MAX_NB_ENCODING_SYMBOLS_DEFAULT``` must be modified in ```openfec/src/lib_stable/ldpc_staircase/of_codec_profile.h``` 
    
To decode an FEC encoded file and apply the erasure and error correcting codes:
```
./decode <input filename> -o <output filename>
```
**Note**: If input file was not correctly FEC-encoded, Decode will throw an error trying to map file to memory.

```-o <output filename>``` Writes to file specified.  
	If flag is not given, will write directly to standard out.

**TODO**: Add usage instructions for `error-simulator`.

## What's in this Repo?

```
.
├── archive     <-- Previous DxWiFi capstone work
├── config      <-- Configuration files for rsyslog, debian installer, etc.
├── dxwifi      <-- Source code for DxWifi suite of programs
│   ├── decode
│   ├── encode
│   ├── rx
│   └── tx
├── libdxwifi   <-- Shared library code for the DxWiFi suite
├── LICENSE
├── openfec     <-- http://openfec.org/, used in DxWiFi's error correction
├── patches     <-- Firmware patches for the Atheros AR9271
├── platform    <-- Cross compilation scripts
├── README.md   
├── rscode      <-- Reed Solomon codec submodule
├── startmonitor.sh <-- Script to enable a WiFi card into monitor mode
└── test        <-- DxWiFi system tests and test data
    └── data
```

## System Tests

To run the system tests first you'll need to compile the project with `DXWIFI_TESTS` defined.
The project Cmake file defines two build configurations with this macro enabled, `TestDebug` and `TestRel`.
Building with this macro defined will force `tx`/`rx` to run in *offline* mode causing them to write or read 
packetized data from a `savefile`. With the correct binaries built, simply run the following to execute the tests:

```
python -m unittest
```

## Cross Compilation

Currently DxWiFi only supports cross-compilation for the `armv7l` architecture. To setup cross-compilation for `armv7l` you'll first
need to setup your toolchain. 

```bash
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf qemu-user-static debootstrap -y
sudo qemu-debootstrap --arch armhf buster /mnt/data/armhf http://deb.debian.org/debian/
sudo chroot /mnt/data/armhf
apt-get install libpcap-dev
```

This will install the armhf version of `libpcap-dev` in `mnt/data/armhf/lib/arm-linux-gnueabihf`. Then you can generate the 
correct build scripts with: 

```
cmake -S . -B build -DPLATFORM=armhf
```
