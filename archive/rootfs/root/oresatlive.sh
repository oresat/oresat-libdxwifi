#!/bin/sh
sleep 10
echo Shutting down wlan0
ifconfig wlan0 down
sleep 1
iw dev wlan0 del
sleep 1
modprobe -r ath9k_htc
sleep 3
echo Starting up wlan0
modprobe ath9k_htc
sleep 3
#iw phy phy0 interface add wlan0 type managed
ifconfig wlan0 up
sleep 1
echo Setting bitrate to 1mbps
iw dev wlan0 set bitrates legacy-2.4 1
sleep 1
ifconfig wlan0 down
sleep 1
echo Enabling monitor mode
iw dev wlan0 set monitor none 
sleep 1
ifconfig wlan0 up
sleep 1
echo setting frequency to 2422MHz - channel 3 
iwconfig wlan0 channel 3

sleep 3
echo Starting wifi broadcast
/root/wifibroadcast/sharedmem_init_tx
sleep 1
/root/jpeg_stream_tx_stdout.py | /root/wifibroadcast/tx_rawsock -p 0 -b 8 -r 2 -f 1400 -t 1 -d 1 -y 0 wlan0

