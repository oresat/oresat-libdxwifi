# Atheros htc_9271 Firmware Patches

Collection of prebuilt firmware binaries with different injection rates. To use simply 
run the following:

```
sudo cp htc_9271-<version>-<phy>-<rate>.fw /lib/firmware/htc_9271.fw
```

Connect the Atheros device to your system and the firmware will be loaded. 

To build your own firmware with a different injection rate, simply clone the [open-ath9k-htc-firmware](https://github.com/qca/open-ath9k-htc-firmware) 
and modify the `ar5416_11ng_table` table in `target_firmware/wlan/ar5416_phy.c`. The default injection rate is the first entry on the table, so to 
change it you'll simply need to move your desired rate to the top of list. Build instructions for the firmware can be found in the
[open-ath9k-htc-firmware](https://github.com/qca/open-ath9k-htc-firmware) repo. 

**Note**: The `htc_9271-1.4-cck-1Mb.fw` file is the default firmware.
