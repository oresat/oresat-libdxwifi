#   I'm not sure if this is the best way, but this is the way I setup my 
#   cross-compilation toolchain
#   
#   ```
#   sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf qemu-user-static debootstrap -y
#   sudo qemu-debootstrap --arch armhf buster /mnt/data/armhf http://deb.debian.org/debian/
#   sudo chroot /mnt/data/armhf
#   apt-get install libpcap-dev
#   apt-get install libgpiod-dev
#   ```

set(CMAKE_SYSTEM_NAME       Linux)
set(CMAKE_SYSTEM_PROCESSOR  arm)

set(CMAKE_C_COMPILER    arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER  arm-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH ${LINUX_ARM_ENV_ROOT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
