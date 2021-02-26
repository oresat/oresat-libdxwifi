## tx-rx

Software to construct, transmit, and receive data packets via packet injection and monitor mode using the AR9271 chip.

## Prerequisites

- 2 linux capable devices (Raspberry PI, Beaglebone Black, Your laptop etc.)
- 2 WiFi adapters capable of monitor mode (Only tested on Atheros AR9271 chipset)
- [CMake](https://cmake.org/)
- [libpcap](https://www.tcpdump.org/)

## Building tx-rx

The tx-rx programs uses [cmake](https://cmake.org/) to generate the correct build files. If 
you need to generate a build for a specific platform then I suggest checking out the docs. 
For basic GNU Makefiles the following should get you going:

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

**Note**: If you're using VSCode I *highly* recommend just using the 
[CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
extension. 

## Usage

On the receiver
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

Here's an example where we set the receiver to open test.cap in append mode, timeout
after 10 seconds without receiving a packet, log everything, and only capture packets whose addressed to 11:22:33:44:55:66
```
sudo ./rx --dev mon0 -vvvvvv --append --timeout 10 --filter 'wlan addr1 11:22:33:44:55:66' test.cap
```

And for the transmitter we set it to transmit everything being read from stdin by 
omitting the file argument, set the blocksize to 200 bytes, address the packets to 
11:22:33:44:55:66, log everything, and specify that the packets should be strictly ordered
```
sudo ./tx --dev mon0 --blocksize 200 --address 11:22:33:44:55:66 --ordered -vvvvvv
``` 
