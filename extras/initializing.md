# Initializing a iotsa board

These are random notes to help with initializing a board to complete "production settings" (with hostname, ssid, https certificate). To be completed at some point.

- Build the hardware
- Build the correct software in platformio
- Connect the hardware
- Flash the initial software
- Connect a serial console with something like

```
cu -s 115200 -l /dev/tty.usb12345
```

- Find the network, select the network, set hostname and wifi password

```
iotsaControl networks
iotsaControl --ssid config-iotsa8787b --target 192.168.4.1 --protocol https --noverify wifiConfig ssid=NETWORK ssidPassword=NETWORKPASSWD config hostName=HOSTNAME reboot=1
make-igor-signed-cert.sh HOSTNAME.local
iotsaControl --target immersiondoor.local --protocol https configWait xConfig users username=ADMIN password=ADMINPASSWD
```

but note the last doesn't work currently (requires POST, not PUT).