Introduction
============

The pilight plugin has been created especially for the pilight software. It provides an API to access devices with ZigBee Home Automation (HA) and ZigBee Light Link (ZLL).

As hardware the [RaspBee](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/raspbee?L=1) ZigBee Shield for Raspberry Pi is used to directly communicate with the ZigBee devices.

The pilight plugin requires the [deCONZ software](http://www.dresden-elektronik.de/funktechnik/products/software/pc/deconz?L=1). 

The pilight plugin opens a socket on port TCP 5008 and listens for incoming connections. The plugin allows to send commands and receive responses to and from ZigBee devices like Philips Hue lights. The plugin translates the incoming commands to APS-Data and passes the data to the RaspBee firmware.

## Preparation and configuration steps

In the standard Raspbian Linux distribution the serial interface /dev/ttyAMA0 is set up as a serial console, i.e. for boot message output. Since the RaspBee ZigBee firmware uses the same interface to communicate with the control software, the following changes must be done to “free” the UART.

In the file /boot/cmdline.txt:

If present remove the text `console=serial0,115200` or `console=ttyAMA0,115200`

Raspbian wheezy: comment out the following line in `/etc/inittab`

     #Spawn a getty on Raspberry Pi serial line
    T0:23:respawn:/sbin/getty -L ttyAMA0 115200 vt100

Raspian jessie: 

    sudo systemctl disable serial-getty@ttyAMA0.service

Raspberry Pi3 jessie:
in the file /boot/config.txt add the line

    enable_uart=1

Reboot

## Installation and compilation steps

##### Install deCONZ and development package
Download and install Qt 4.8

    sudo apt-get install libqt4-core

1. Download deCONZ package


    wget http://www.dresden-elektronik.de/rpi/deconz/deconz-latest.deb
    -or-
    wget http://www.dresden-elektronik.de/rpi/deconz/deconz-2.04.35.deb

2. Install deCONZ package


    sudo dpkg -i deconz-2.04.35.deb
  
3. Download deCONZ development package


    wget http://www.dresden-elektronik.de/rpi/deconz-dev/deconz-dev-latest.deb
    -or-
    wget http://www.dresden-elektronik.de/rpi/deconz-dev/deconz-dev-2.04.35.deb

4. Install deCONZ development package


    sudo dpkg -i deconz-dev-2.04.35.deb
  
5. Install Qt4 development packages


    sudo apt-get install qt4-qmake libqt4-dev

6. (Optional) Install GCFFlasher tool to install new firmware or restart RaspBee


    wget http://www.dresden-elektronik.de/rpi/gcfflasher/gcfflasher-latest.deb
    -or-
    wget http://www.dresden-elektronik.de/rpi/gcfflasher/gcfflasher-2.10.deb
    sudo dpkg -i gcfflasher-2.10.deb


##### Compile the plugin
 
1. Compile the plugin


    cd libs/zigbee
    qmake-qt4 && make

2. Copy the plugin to the deConz plugins folder


    cp libpilight_plugin.so /usr/share/deCONZ/plugins

3. Optional remove or move the existing rest plugin (because for some strange reason the rest plugin removes bindings on all devices which makes reporting impossible)


    mv /usr/share/deCONZ/plugins/libde_rest_plugin.so /usr/share/deCONZ/libde_rest_plugin.so_NOT_USED

4. Copy startup script


    sudo cp deconz /etc/init.d
    sudo chmod +x /etc/init.d/deconz
    sudo update-rc.d deconz defaults

5. Start deConz (parameter --dbg-info=1 only needed for debug messages). It might be necessary to start with UI when starting and forming the ZigBee network for the first time. Thereafter the UI is not needed anymore.

with UI (requires X-server)

    deCONZ --dbg-info=1

without UI
	
    sudo service deconz start
  
-or-

    DISPLAY=:0.0 deCONZ --dbg-info=1

Hardware requirements
---------------------

* Raspberry Pi
* [RaspBee](http://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/raspbee?L=1) ZigBee Shield for Raspberry Pi
* or a [ConBee](https://www.dresden-elektronik.de/funktechnik/solutions/wireless-light-control/conbee/?L=1) USB dongle

Usage
=====

The plugin is started when the deConz software is started. The deConz software must be running before starting pilight.

    pilight-send -p zigbee_dimmer -I <hostname>  -a 
    pilight-send -p zigbee_ctrl -I <hostname> -g


For testing, use netcat to connect to port 5008.

Example: `nc localhost 5008`


Command | Description 
------- | ----------- 
`r <shortaddr> <ep> <cluster> <attrid>` | read attributes
`b <shortaddr> <ep>` | read basic attributes 
`b <shortaddr> <ep> <cluster>` | read basic attributes on cluster
`m <profile> <cluster>` | send match descriptor request (discover cluster)
`p <shortaddr>` | permit Joining on device (coordinator = 0)
`zclattr <shortaddr> <ep> <cluster> <command>` | send ZCL attribute request 
`zclcmd <shortaddr> <ep> <cluster> <command>`  | send ZCL command request 
`zclcmdgrp <groupaddr> <ep> <cluster> <command>` | send ZCL command request to group 
`zdpcmd <shortaddr> <cluster> <command>` | send ZDP command request 
`zclattrmanu <shortaddr> <ep> <cluster> <manufacturer id> <command>` | send ZCL attribute request manufacturer specific
`zclcmdmanu <shortaddr> <ep> <cluster> <manufacturer id> <command>`  | send ZCL command request manufacturer specific


Response | Description
--------| -----------
<-LQI 0x84182600xxxxxxxx   06 0 1 0x001FEE00xxxxxxxx 0x4157 1 1 2 01 02 56 | LQI neighbor reponse
<-ZCL attribute report 0x001FEE00xxxxxxxx 0x0006 1 00 00 10 00 | ZCL attribute report (from cluster 0x0006)
<-ZCL serverToClient 0x00124B00xxxxxxxx 1 for cluster 0x0500 10 00 00 00 00 00 | ZCL attribute (with extaddr)
<-ZCL serverToClient 0x3B58 1 for cluster 0x0500 10 00 00 00 00 00 | ZCL attribute (with shortaddr)
<-APS attr 0x001FEE00xxxxxxxx 5 0x0702 0x0000 0x25 0E DA 76 57 00 00 00 04 00 2A 00 00 00 | APS data 
 
    <-LQI [source addr] [neighborTableEntries] [start] [count] [extaddr] [shortaddr] [device type] [rxOnWhenIdle] [relationship] [permitJoin] [depth] [lqi]
    <-APS attr [source extaddr] [endpoint] [cluster] [attributeid] [typeid] [attr value] [...more attributes] 



LQI | Description
--- | -----------
source addr | LQI neighbor table from <source addr>
neighborTableEntries | total table entries
start | current table entry start index
count | number of table entries in this frame
extaddr | table entry extended address
shortaddr | table entry short address
device type | 0 = coordinator, 1 = router, 2 = end device, 3 = unknown
rxOnWhenIdle | receiver on when idle 0 or 1
relationship | relationship 0 = neighbor is the parent, 1 = child, 2 = sibling, 3 = None of the above, 4 = previous child
permitJoin | permit Join 0 = neighbor is not accepting join requests, 1 = accepting join requests
depth | The tree depth of the neighbor device
lqi | The estimated link quality (range 0x00 - 0xff)
  

Supported ZigBee devices
========================

Successfully tested with pilight

Device | Vendor
------ | ------
Light Philps Hue White | Philips, LWB006
Light | OSRAM, Classic B40 TW - LIGHTIFY
Smart Plug | OSRAM, Plug 01
Movement Sensor | Bitron Home, 902010/22
Motion Sensor | Philips Hue motion sensor
Smoke Detektor with siren | Bitron Home, 902010/24
Smart Plug with Metering | Bitron Home, 902010/25
Thermostat | Bitron Home, 902010/32
Switch | ubisys, S2 (5502)
Button | Philips Hue dimmer switch
Button | [Xiaomi Smart Wireless Switch](https://www.banggood.com/Original-Xiaomi-Smart-Wireless-Switch-p-1045081.html)
Temperature | Xiaomi Temperature and Humidity Smart Sensor

Reference Manuals
-----------------
[Bitron](http://www.bitronvideo.eu/index.php/service/bitron-home/downloads/)

[Ubisys](http://www.ubisys.de/en/smarthome/products-s2.html)

Reset ZigBee device to factory default
--------------------------------------

[Philips Hue Light](http://www2.meethue.com/de-de/support/faq-detail/?productCategoryId=131288)
Can I factory reset a Hue light with the Hue dimmer switch?
Yes you can, press and hold the ON and OFF button simultaneously until the LED light on the dimmer switch turns green (Note: the lamp is blinking during this process. Hue Beyond and Hue Phoenix are excluded).

Osram Lightify Light turn the light on and off five times and wait 5 seconds inbetween. 


[Bitron Video](http://www.bitronvideo.eu/index.php/service/bitron-home/zb-registrierung/)
Press and hold the button for 10 seconds.


Troubleshooting
---------------

After a while a device keeps responding with an error status 0xD0 when reading attributes or sending other commands. Sometimes this just gets resolved without doing anything but waiting. However, sometimes it turns out that the device shortaddress has changed for some unknown reason. Oberserving the LQI neibortable entries shows the new shortaddress.

<pre>
zclattr <b>0x4157</b> 5 0x0702 0000000004 --> send OK
<-APS-DATA.confirm FAILED status 0xD0, id = 0x25, srcEp = 0x01, dstcEp = 0x05, dstAddr = <b>0x4157</b>
<-LQI 0x00178801xxxxxxxx   07 0 1 0x001FEE00xxxxxxxx <b>0xA855</b> 1 1 3 01 01 F7
zclattr <b>0xA855</b> 5 0x0702 0000000004 --> send OK
</pre>

Restart RaspBee
---------------


