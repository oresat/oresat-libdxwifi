# Hardware
Some packages will need to be installed, which will require Internet access.  We recommend developing on a Raspberry Pi version which has onboard Ethernet.
Alternately, you can use a USB Ethernet dongle on a Raspberry Pi Zero W.  A USB to serial cable such as [Adafruit product 954](https://www.adafruit.com/product/954) is also recommended.

# Theory of Operation
On boot up /etc/rc.local calls /root/oresatlive.sh

oresatmini.sh configures the wifi interface, then starts jpeg_stream_tx_stdout.py and wifibroadcast tx.

jpeg_stream_tx_stdout.py repeatedly takes a JPEG photo with the Pi Camera, adds a header with image size and integrity check information and then outputs a stream of bytes to stdout.  The byte stream is piped into wifibroadcast tx which adds forward error correction information, splits up the data to fit into wifi sized packets, and then sends the packets to the wifi chip for transmission.

# Install Raspbian on SD Card
Download [2018-11-13-raspbian-stretch-lite.img](https://downloads.raspberrypi.org/raspbian_lite/images/raspbian_lite-2018-11-15/2018-11-13-raspbian-stretch-lite.zip)

Install the Raspian Lite image onto a 2GB or larger SD card.  The Raspberry Pi documentation explains the proceedure:
https://www.raspberrypi.org/documentation/installation/installing-images/


# Copy Git content
This Git repository is structured like the file system on the SD card.  Copy the files in the repo to the same location on your SD card.


# Boot configuration
Be sure to copy /boot/config.txt from the Git repo to the boot partition on your SD card.  The following lines should appear at the end of the file: 
```
gpu_mem=256
enable_uart=1
```


# Enable SSH
sudo raspi-config
choose interface options
enable ssh


# Enable serial console (optional)
Having access to a serial console can be handy for debug.  Connect a [USB serial cable](https://elinux.org/RPi_Serial_Connection) to the Raspberry Pi.

You will need to use terminal emulator software such as `screen` or `minicom` to access the serial console.

```
sudo screen /dev/ttyUSB0 115200
```


# Enable Pi Camera
```
sudo raspi-config
choose interface
enable camera
apt install python-picamera
```
# Test Pi Camera
```
raspistill -v -o test.jpg
```

# Compile wifibroadcast
```
apt-get install libpcap-dev
cd /root/wifibroadcast
make
```


You may now reboot the platform and OreSat Live should start automatically.
