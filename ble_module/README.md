# BLE Module Firmware

The Bluetooth Low Energy module on the Happy Bubbles detectors is a the Xuntong PTR5518, based on the Nordic nRF51822 chipset. It can run any firmware written for that Nordic BLE chip. The firmware on it currently scans for BLE advertisements and sends them over a 115200 baud UART/serial line to the ESP8266.


## Flashing

### Connection

There are two ways to flash the BLE module. The easiest is by using a Tag-Connect connector http://www.tag-connect.com/TC2030-IDC-NL

The second is by soldering to the SWD pins on the PTR5518 module. For this, 3 wires are needed: ground, SWDIO, and SWCLK.

![Alt text](/ble_module/ble_flashing.jpg?raw=true "SWD")

### Software

These instructions are designed for a Raspberry Pi v3 running Raspbian, with an STLink V2 USB adapter.

#### Dependencies

```
sudo apt-get update
sudo apt-get install git autoconf libtool make pkg-config libusb-1.0-0 libusb-1.0-0-dev srecord tmux npm vim
cd ~
mkdir code
cd code
git clone git://git.code.sf.net/p/openocd/code openocd-code
cd openocd-code
./bootstrap
./configure --enable-sysfsgpio --enable-bcm2835gpio
make
sudo make install
sudo cp /usr/local/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Once those are loaded, running `./flash` should flash the BLE module to the latest firmware when connected correctly.
