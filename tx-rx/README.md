## tx-rx

Software to construct, transmit, and receive data packets via packet injection and monitor mode using the AR9271 chip.

## Prerequisites

- 2 linux capable devices (Raspberry PI, Beaglebone Black, Your laptop etc.)
- 2 WiFi adapters capable of monitor mode (Only tested on Atheros AR9271 chipset)
- [CMake](https://cmake.org/)
- [`libpcap-dev`](https://www.tcpdump.org/), [`iw`](https://wireless.wiki.kernel.org/en/users/documentation/iw), [`ip`](https://linux.die.net/man/8/ip)

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

**Note**: If you're using VSCode I **highly** recommend just using the 
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

Here's an example where we set the receiver to output received files in the `test/` directory with the file format of `test_n.raw`,
timeout after 10 seconds without receiving a packet, log everything, strictly order the packets and fill in missing packets with noise, and 
only capture packets whose addressed to 11:22:33:44:55:66.
```
sudo ./rx --dev mon0 -v 6 --ordered --add-noise --timeout 10 --extension raw --prefix test --filter 'wlan addr1 11:22:33:44:55:66` test/
```

And for the transmitter we set it to transmit everything in the `dxwifi` directory matching the glob pattern `*.md` and listen for new files, timeout after 20 seconds
of no new files, transmit each file into 512 byte blocks, send 5 redundant control frames, and delay 10ms between each tranmission block and 10ms between each file transmission.
```
sudo ./tx --dev mon0 --blocksize 512 --redundancy 5 --delay 10 --file-delay 10 --filter "*.md" --include-all --watch-timeout 20 dxwifi/
``` 

**Note**: When doing multi-file transmission like the example above, it's critical to set the `--file-delay` and `--redundancy` parameters 
to something reasonable for your channel. If these parameters are not set then file boundaries will not be clearly delimited to the receiver.

## Tests

To run the system tests first you'll need to compile the project with `DXWIFI_TESTS` defined.
The project Cmake file defines two build configurations with this macro enabled, `TestDebug` and `TestRel`.
Building with this macro defined will force `tx`/`rx` to run in *offline* mode causing them to write or read 
packetized data from a `savefile`. With the correct binaries built, simply run the following to execute the tests:

```
python -m unittest
```
