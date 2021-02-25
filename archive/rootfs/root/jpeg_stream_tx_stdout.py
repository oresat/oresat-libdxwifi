#!/usr/bin/python

import io
import socket
import struct
import time
import picamera
import sys
import hashlib
import binascii

sync_byte = '\xAA'

# based on "4.9. Capturing to a network stream", sockets replaced by stdin, stdout
# https://picamera.readthedocs.io/en/release-1.10/recipes1.html

with picamera.PiCamera() as camera:
    camera.resolution = (1280, 720)
    camera.rotation = 180
    # Wait for the automatic gain control to settle
    time.sleep(10)
    # Now fix the values
    #sys.stderr.write("auto shutter_speed: %f\n" % camera.shutter_speed) 
    #camera.shutter_speed = 100
    #camera.shutter_speed = camera.exposure_speed
    #camera.exposure_mode = 'off'
    #g = camera.awb_gains
    #camera.awb_mode = 'off'
    #camera.awb_gains = g


    image_num = 0
    stream = io.BytesIO()
    for foo in camera.capture_continuous(stream, 'jpeg', quality=10):
        image_num = image_num + 1
        file_size = stream.tell()
        hash = hashlib.md5(stream.getvalue()).digest()
        sys.stderr.write("Image %d size: %d md5: %s\n" % (image_num, file_size, binascii.hexlify(hash)))
        #header: 16 sync bytes followed by file size repeated twice then md5 hash then sequence number
        sys.stdout.write(sync_byte * 16) 
        sys.stdout.write(struct.pack('<LL16sL', file_size, file_size, hash, image_num))

        sys.stdout.write(stream.getvalue())
        stream.truncate()
        time.sleep(2.0)
