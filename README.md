![Happy Bubbles Logo](happy_bubbles_logo.png?raw=true)

# Happy Bubbles - Bluetooth Beacon Presence Detection Device

This device is in the "Happy Bubbles" series of open hardware and open source IoT and home-automation devices. It is a WiFi-connected device that listens for Bluetooth Low Energy advertisements, from proximity beacons, and passes them on over MQTT.

It is designed to be used as a home-automation presence detection system. If you install the detectors through-out a home and family members carry beacons around the house, you can program your home automation hubs to take certain actions depending on who entered or left certain rooms.  

The detectors can be purchased from the Happy Bubbles shop at https://www.happybubbles.tech/shop/

There is more information about how the presence detection system works at https://www.happybubbles.tech/presence/ and how the detectors themselves work at https://www.happybubbles.tech/presence/detector

## How to set it up and use it

#### 0. Power it on
1. Just plug in a microUSB cable

#### 1. Connect it to your wifi network
1. With the board powered up, the Happy Bubbles presence detector device will automatically start an access point you can connect to called "happy-bubbles-ble" and the LED should be orange to tell you you're in "config mode". If not, just hold the "config" button down for 5 seconds or so.
2. Connect to this access point with your computer
3. Open a browser and visit http://192.168.4.1/
4. You'll now be at the Happy Bubbles config screen! You can change the hostname of the device here.
5. On the left side with the navigation bar, go to "WiFi Setup". It will populate a bunch of wifi access points it can see. Find yours and type in the password. You should see it get an IP address from it, and now you're connected!

#### 2. Make it hit your server
1. From the same config mode, go to "MQTT Setup". In there, you can set which MQTT server the device should talk to. 

### Credits

This project wouldn't have been possible without my standing on the shoulders of giants such as:
 * [NodeMCU](https://github.com/nodemcu/nodemcu-devkit-v1.0) - The ESP8266-based hardware that this project uses.
 * [esp-link](https://github.com/jeelabs/esp-link) - The firmware for this project is a modified subset of the what JeeLabs' "esp-link" project has built and made this whole thing really nice and easy to work with. 
 * [Espressif](http://espressif.com) - for making the amazing ESP8266 project that enables things like this to even be possible. A wonderful product from a wonderful company.
 * [esphttpd](http://www.esp8266.com/wiki/doku.php?id=esp-httpd) - SpriteTM made an awesome little HTTP webserver for the ESP8266 that powers the config menu of the esp-link and the Happy Bubbles NFC device
 * [Xuntong Technology](http://www.freqchina.com/en/) - The company that makes the PTR5518 Bluetooth Low Energy module used in this product
 * [Dangerous Prototypes](http://dangerousprototypes.com/) - Their DirtyPCBs service is awesome and this project wouldn't be possible without it. They are a wonderful community and do amazing work to bridge the Chinese tech resources to hackers worldwide
 * [FlyLin Consulting](http://flylin.co/) - For helping enormously to assembly the first batch of boards that are for sale 
 * everyone in the ESP8266 community who shares their code and advice.

