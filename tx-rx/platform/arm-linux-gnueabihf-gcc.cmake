#   I'm not sure if this is the best way, but this is the way I setup my 
#   cross-compilation toolchain
#   
#   ```
#   sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf qemu-user-static debootstrap -y
#   sudo qemu-debootstrap --arch armhf buster /mnt/data/armhf http://deb.debian.org/debian/
#   sudo chroot /mnt/data/armhf
#   apt-get install libpcap-dev
#   ```
#   
#   This will install the arm version of libpcap-dev in mnt/data/armhf/lib/arm-linux-gnueabihf
#   Cmake should be able to find the .so but if it can't you may have to modify the find_library
#   directive in the root cmake file
#
#   Alternatively you can build libpcap-dev from source and point cmake to it like this:
#   
#   ```
#   cd /usr/arm-linux-gnueabihf/
#   export PCAP_VERSION=1.9.1
#   wget http://www.tcpdump.org/release/libpcap-$PCAP_VERSION.tar.gz
#   tar xvf libpcap-$PCAP_VERSION.tar.gz
#   cd libpcap_$PCAP_VERSION
#   export CC=arm-linux-gnueabihf-gcc
#   export CFLAGS='-Os'
#   sudo ./configure --host=arm-linux-gnueabihf --with-pcap=linux
#   make
#   ```

set(CMAKE_SYSTEM_NAME       Linux)
set(CMAKE_SYSTEM_PROCESSOR  arm)

set(CMAKE_C_COMPILER    arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER  arm-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH ${LINUX_ARM_ENV_ROOT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
