# Introduction
The OreSat Live system will use wifi to stream images of Oregon from low earth orbit to student-built handheld receivers on the ground.

Wifi is not designed for the high speeds and long distances involved with satellite communication.  To overcome the wifi radio and protocol limitations this project configures an Atheros AR9721 WiFi chip to transmit using 1Mbps DSSS modulation and defines a simple image transfer protocol for use with one-way communication.

The current proof-of-concept uses a Raspberry Pi with Pi Camera and an Atheros 9271 USB wireless module.  The final OreSat hardware will be based on an Octavo SoC with a Cortex A8 running Linux on a custom PCB containing an Atheros 9271 wifi chip.  Hardware development is being done in the [oresat-dxwifi-hardware](https://github.com/oresat/oresat-dxwifi-hardware) project.

This software is for use on the **satellite** only.

# Related Projects
[Mini OreSat](https://github.com/oresat/oresat-live-mini-oresat) combines the OreSat Live software with a Raspberry Pi Zero W + Pi Camera to create platform for hobbyist and classroom experiments with high-altitude ballooning.

[OreSat Handheld Ground Station](https://github.com/oresat/oresat-live-handheld-ground-station) enables students to build their own low cost ground station to receive ballon or satellite images and share them with their classmates over wifi.

# Hardware Setup
1. Connect the Pi Camera to the Raspberry Pi camera connector
2. Connect a USB AR9721-based wifi adapter to the Raspberry Pi

# Software Setup
1. [Download the SD card image oresat_live_14_jun_2019.img](https://drive.google.com/a/pdx.edu/uc?id=1UZVqeXtGgvxExQUCeqXXN3y88YrLU3So&export=download)
2. Write the SD card image to a 2GB or larger card.  The [Raspberry Pi documentation](https://www.raspberrypi.org/documentation/installation/installing-images/) explains the procedure.

# Startup Procedure
1. Install the SD card in Raspberry Pi
2. Connect the power
3. Verify the ground station is able to receive images

# Software Development
If you would like to build the system from source follow the [Build Instructions](BUILD.md)
