# oresat-dxwifi-software <!-- omit in toc -->

## Table of Contents <!-- omit in toc -->
- [Introduction](#introduction)
- [What's In This Repo?](#whats-in-this-repo)
- [Prerequisites](#prerequisites)
- [Building](#building)
  - [Cross Compilation](#cross-compilation)
- [Installation and Packaging](#installation-and-packaging)
- [Usage](#usage)
  - [Transmit / Receive](#transmit--receive)
  - [Encode / Decode](#encode--decode)
- [Testing](#testing)

## Introduction

[![Build/Test](https://github.com/oresat/oresat-dxwifi-software/actions/workflows/build_test.yml/badge.svg)](https://github.com/oresat/oresat-dxwifi-software/actions/workflows/build_test.yml)

This repository contains software to construct, transmit, and receive data packets with forward error correction (FEC) via packet injection and monitor mode using the AR9271 chip. We leverage packet injection and monitoring using the [PCAP](https://www.tcpdump.org/manpages/pcap.3pcap.html) library to send and receive custom raw data frames over the air without the need for a traditional access point connection.

The transmit software is responsible for crafting custom packets in user space and attaching correct headers before handing them off to PCAP. In particular, each custom packet includes these three components:

- [Radiotap header](https://www.radiotap.org/): the “de facto” standard for 802.11 frame injection and reception, these fields control transmission parameters like the data rate, and can alert the driver that we will not expect ACKs. 
- [IEEE80211 MAC Layer header](https://witestlab.poly.edu/blog/802-11-wireless-lan-2/): we are primarily concerned with the address section of this header, which we can stuff with a fake MAC address the receiver can listen for.
- Payload: The actual block of data to send. 

When a packet is captured (based on the fake MAC address) and forwarded to PCAP, we receive a raw byte array in user space. After unpacking the payload, we have the original transmitted data.

As with any wireless communication link, the connection between OreSat and the ground will not be perfect. We anticipate two types of errors: bit errors, where bits in the received data are randomly flipped; and packet loss errors, in which entire packets of data are not received on the ground.

In most applications, these errors can be avoided by having the receiver detect lost packets or bit errors (e.g. with checksums) and request retransmission when necessary. However, since OreSat Live is a unidirectional link, there will be no way for the ground stations to request packet retransmission. We therefore implement forward error correction (FEC), in which OreSat sends packets encoded with redundant information in such a way that the receiver can reconstruct the original data even in the face of bit errors and packet loss.

We base our FEC implementation on the [approach](https://upcommons.upc.edu/bitstream/handle/2117/90146/MasterThesisMatiesPonsSubirana.pdf) developed at the Universitat Politècnica de Catalunya. Two separate FEC encodings are concatenated to create a transmission that is robust to both kinds of errors. First, data is split into blocks and put through a [low-density parity-check (LDPC)](https://en.wikipedia.org/wiki/Low-density_parity-check_code) encoder built on [OpenFEC](http://openfec.org/), which is able to correct for packet loss errors. The encoded data is then pushed through a [Reed-Solomon](https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction) encoder built on [rscode](https://github.com/hqm/rscode), which is designed to correct single bit errors. These doubly-encoded packets are then transmitted, received on the other end, and decoded to reconstruct the original data.

A diagram of the process is available [here](https://drive.google.com/file/d/1OS1jDQYomK5IMrGjk3r-uisx0R9NhZec/view?usp=sharing).

## What's In This Repo?

```
.
├── .github           <-- GitHub workflow specification for automatic build / test
├── archive           <-- Previous DxWiFi capstone work
├── config            <-- Configuration files for rsyslog, Debian installer, etc.
├── dxwifi            <-- Source code for DxWifi suite of programs
├── libdxwifi         <-- Shared library code for the DxWiFi suite
├── openfec           <-- OpenFEC library (http://openfec.org/)
├── patches           <-- Firmware patches for the Atheros AR9271
├── platform          <-- Cross compilation scripts
├── rscode            <-- Reed-Solomon library (submodule)
├── test              <-- System tests and test data
├── .gitignore
├── .gitmodules
├── CMakeLists.txt
├── LICENSE           <-- GNU GPL v3
├── README.md         <-- You are here
└── startmonitor.sh   <-- Script to put a WiFi card in monitor mode
```

## Prerequisites

- 2 Linux devices (Raspberry Pi, BeagleBone Black, laptop, etc.)
- 2 WiFi adapters that support monitor mode (tested with Atheros AR9271)
- [CMake](https://cmake.org/)
- [`libpcap-dev`](https://www.tcpdump.org/), [`libgpiod-dev`](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/), [`iw`](https://wireless.wiki.kernel.org/en/users/documentation/iw), [`ip`](https://linux.die.net/man/8/ip)
- [Python 3.6](https://www.python.org/) or higher (for test scripts)

## Building

**Note:** if you're using VSCode, you should probably use the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
extension.

These programs use [CMake](https://cmake.org/) to generate the correct build files. The following should get you going, but if you need to generate a build for a specific platform then you should probably check out the documentation.

First, make sure the `rscode` submodule is up-to-date.
```
git submodule update --init --recursive
```

Then you can generate build scripts and make the executables.

```
mkdir build
cd build && cmake ..
make
```

Alternatively, to make multiple out-of-source builds you can do the following
with any of the default configurations: *Debug, Release, RelWithDebInfo, MinSizeRel, TestDebug, TestRel*.
```
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
```

The executables are located in `bin` and libraries in `lib`. You can also build a
specific target as follows.

```
cmake --build --target dxwifi
```

### Cross Compilation

Currently this only supports the `armv7l` architecture. You'll first
need to setup your toolchain.

```
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

## Installation and Packaging

With the project built and CMake files present in the `build` directory you do the following:

```
make install
```

This will install the `rx`, `tx`, `encode`, `decode` and `startmonitor` programs into `/usr/bin` and define a daemon with `oresat-dxwifi-txd.service`, which you can start with `systemctl`.

To build debian packages to install you can run the following in the `build` directory:

```
cpack  # or `make package`
```

This will create two debian packages `oresat-dxwifi-rx_<version>_<arch>.deb` and `oresat-dxwifi-tx_<version>_<arch>.deb`. The former installs just the `rx` program, and the latter installs installs `tx`, `startmonitor` and the `oresat-dxwifi-txd.service` daemon.

**Note:** The `oresat-dxwifi-txd.service` daemon is currently configured for OreSat0. As such, all it does is transmit a test sequence of bytes _ad infinitum_ or until stopped.

## Usage

### Transmit / Receive

On the receiver, run:
```
sudo ./startMonitor.sh mon0
sudo ./rx --dev mon0 copy.md
```

On the transmitter, run:
```
sudo ./startMonitor.sh mon0
sudo ./tx --dev mon0 README.md
```

Congratulations, you just copied this README to the receiving device!
(Be sure to change `mon0` to the name of your network device.)

Both the receiver and transmitter support a variety of different options. You can get a full list of options and descriptions by running with `--help`.

Here's an example where we set the receiver to output received files in the `test` directory with format `test_n.raw`, timing out after 10 seconds without receiving a packet, logging everything, strictly ordering the packets, filling in missing packets with noise, and only capturing packets addressed to `11:22:33:44:55:66`.
```
sudo ./rx --dev mon0 -v 6 --ordered --add-noise --timeout 10 --extension raw --prefix test --filter="wlan addr1 11:22:33:44:55:66" test/
```

For the transmitter, we set it to transmit everything in the `dxwifi` directory matching the glob pattern `*.md`, listening for new files, timing out after 20 seconds
without a new file, splitting each file into 512 byte blocks, sending 5 redundant control frames, and delaying 10ms between each tranmission block and 10ms between each file transmission.
```
sudo ./tx --dev mon0 --blocksize 512 --redundancy 5 --delay 10 --file-delay 10 --filter "*.md" --include-all --watch-timeout 20 dxwifi/
```

When doing multi-file transmission like in the example above, it's critical to set the `--file-delay` and `--redundancy` parameters
to something reasonable for your channel. If these parameters are not set then file boundaries will not be clearly delimited to the receiver.

To exercise the software's error-correcting capabilities, the `tx` program can introduce artifical bit errors and packet losses. To control the rate of occurrence, use the `--error-rate` and `--packet-loss` (respectively) with values between 0 and 1.

These programs can also be run in "offline" mode for testing purposes. Use the `--savefile` flag to save output to or read input from a file instead of transmitting over the air.

### Encode / Decode

**Note:** As of Release 1.0, the `tx` and `rx` programs automatically perform forward error correction encoding internally with preset defaults. The below documentation is provided if manual encoding and decoding is still necessary.

To encode a file at a specific code rate:

```
./encode <input filename> -o <output filename> -c <code rate between 0.0 and 1.0>
```

The code rate is the rate at which repair symbols will be added to the source data. Note that this will spit out an error if the code rate is too low for the data being encoded. Also note that OpenFEC can only handle a maximum of 50,000 repair symbols before it will internally throw an error. If more than 50,000 repair symbols are needed, ```OF_LDPC_STAIRCASE_MAX_NB_ENCODING_SYMBOLS_DEFAULT``` must be modified in ```openfec/src/lib_stable/ldpc_staircase/of_codec_profile.h```

To decode an encoded file:
```
./decode <input filename> -o <output filename>
```
If input file was not correctly encoded (or is corrupted beyond recognition), this will throw an error.

## Testing

**Note:** these test scripts require Python 3.6 or higher.

To run the system tests first you'll need to compile the project with `DXWIFI_TESTS` defined.
The project Cmake file defines two build configurations with this macro enabled, `TestDebug` and `TestRel`.
Building with this macro defined will force `tx`/`rx` to run in *offline* mode causing them to write or read
packetized data from a `savefile`. With the correct binaries built, simply run the following to execute the tests:

```
python -m unittest
```

There is also a script `sweep.py` to automatically iterate through code rate, error rate, and packet loss rate parameters. To use (from the repository root):

```
python test/sweep.py  --code-rate   | -c [min] [max] [step]
                      --error-rate  | -e [min] [max] [step]
                      --packet-loss | -p [min] [max] [step]
                      --source      | -s [source file path]
                      --output      | -o [output directory path]
```

This will perform all combinations of code rates, error rates, and packet loss rates (offline) and save the outputs in subdirectories in the specified output directory.