# ZigBee protocol documentation

Protocols

1. zigbee_switch                  ZigBee switch on off
1. zigbee_dimmer                  ZigBee Level Control
1. zigbee_ctrl                    ZigBee Controller
1. zigbee_metering                ZigBee metering
1. zigbee_motion                  ZigBee motion
1. zigbee_temperature             ZigBee Temperature and Humidity
1. zigbee_thermostat              ZigBee thermostat
1. zigbee_button                  ZigBee Button or Philips Hue Dimmer Switch
1. zigbee_huemotion               ZigBee Philips Hue motion sensor
1. zigbee_shutter                 ZigBee Window Covering

## ZigBee devices successfully tested with pilight

- Philps Hue White Light
- Osram Light Classic B40 TW - LIGHTIFY
- Osram Smart Plug
- Bitron Home Movement Sensor 902010/22
- Bitron Home Smoke Detektor with siren 902010/24
- Bitron Home Smart Plug with Metering 902010/25
- Bitron Home Thermostat 902010/32
- ubisys Switch S2
- ubisys Shutter Contol J1
- Philips Hue dimmer switch
- Philips Hue motion sensor
- Button [Xiaomi Smart Wireless Switch SKU392536](https://www.banggood.com/Original-Xiaomi-Smart-Wireless-Switch-p-1045081.html)
- Temperature Xiaomi Temperature and Humidity Smart Sensor SKU394625
- Xiaomi Intelligent Door Window Sensor Control SKU318428

## Getting started

Install the pilight plugin in /usr/share/deCONZ/plugins directory where the RaspBee is installed. The RaspBee does not need to run on the same machine where pilight is running. The plugin libpilight_plugin.so must be compilied in the pilight/libs/zigbee/ directory.

Power on RaspBee and start deConz software.

Add the RaspBee hardware to config.json file. Use localhost if pilight and RaspBee are running on the same machine otherwise use IP address of the remote hardware.

        "hardware": {
                "433gpio": {
                        "sender": 0,
                        "receiver": 1
                },
                "zigbee": {
                        "location": "localhost"
                }
        },


Open the ZigBee network for new devices and permit joining.

    pilight-send -p zigbee_ctrl -j

This opens the network for 3 minutes. Now turn on the new ZigBee device. In general ZigBee devices are in the state factory new, when they have never joined any network before. When powered on the first time, they will start searching for networks that permit joining and join the network. If a device already has joined a network, it usually needs a reset before it can join a new network.

After the new device has joined the network, an entry must be added to config.json devices settings. Running the autodiscover option does create files in /tmp directory with snippets that can be added to the config.json file.

    pilight-send -p zigbee_ctrl -a

Note: After running the autodiscovery, it may be needed to restart the deConz software on the RasbBee.

    sudo service deconz stop
    sudo service deconz start

Check the pilight plugin on the RaspBee with the netcat utility on port 5008:

    nc localhost 5008

Replace localhost with the IP address of the host with the RaspBee.


Example output in /tmp directory after running autodiscovery:

    $ ls /tmp/pilight_zigbee_*
    /tmp/pilight_zigbee_dimmer-0xBACE-0x0B_json.config
    /tmp/pilight_zigbee_dimmer-0xD19C-0x03_json.config
    /tmp/pilight_zigbee_huemotion-0xE5BB-0x02_json.config
    /tmp/pilight_zigbee_metering-0x76AA-0x01_json.config
    /tmp/pilight_zigbee_metering-0xA855-0x05_json.config
    /tmp/pilight_zigbee_metering-0xDC6E-0x01_json.config
    /tmp/pilight_zigbee_motion-0x06DA-0x01_json.config
    /tmp/pilight_zigbee_switch-0x5211-0x03_json.config
    /tmp/pilight_zigbee_switch-0x76AA-0x01_json.config
    /tmp/pilight_zigbee_switch-0xA855-0x01_json.config
    /tmp/pilight_zigbee_switch-0xA855-0x02_json.config
    /tmp/pilight_zigbee_switch-0xDC6E-0x01_json.config
    /tmp/pilight_zigbee_thermostat-0x8655-0x01_json.config
    /tmp/pilight_zigbee_unknown-0x06DA-0x00_json.config



## Example config.json device entries 

Every ZigBee device has the following settings:

    "shortaddr": "0xBACE",
    "endpoint": 11
    "extaddr": "0x00178801xxxxxxxx",
    
    "ProfileId": "0xC05E",
    "ManufacturerName": "Philips",
    "ModelIdentifier": "LWB006",
    "DateCode": "20150413",
    "Version": "5.38.1.15095",

The shortaddr is the 16-Bit short address that is randomly generated when a device joins the ZigBee network. The shortadd could change if the device is reset. The endpoint id is specific for every device. The extaddr is the 64-Bit extended address (like MAC address) that is unique for every single device and always remains unchanged.

The ProfileId has the value 0x0104 for ZigBee Home Automation or 0xC05E for ZigBee Light Link. Other values are possible but untested with pilight.

The ManufacturerName, ModelIdentifier and DateCode values are read from the ZigBee basic cluster attributes 0x0003, 0x0004, 0x0005, 0x0006. The Version is read from basic cluster attribute 0x0400 but not every device has implemented that attribute.

Parameter poll-interval is optional on every device. If present then the values (state on/off, dimlevel, metering, thermostat temperature) are read from the device when poll-interval in seconds has elapsed.  

###### zigbee_switch

                "zigbee-0xDC6E-plug": {
                        "protocol": [ "zigbee_switch" ],
                        "id": [{
                                "shortaddr": "0xDC6E",
                                "endpoint": 1
                        }],
                        "extaddr": "0x00124B00xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "Bitron Home",
                        "ModelIdentifier": "902010/25",
                        "DateCode": "20150629",
                        "state": "on"
                },
                "zigbee-0xA855-0x01": {
                        "protocol": [ "zigbee_switch" ],
                        "id": [{
                                "shortaddr": "0xA855",
                                "endpoint": 1
                        }],
                        "extaddr": "0x001FEE00xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "ubisys",
                        "ModelIdentifier": "S2 (5502)",
                        "DateCode": "20160302-DE-FB0",
                        "state": "off"
                },

###### zigbee_dimmer

                "zigbee-0xBACE": {
                        "protocol": [ "zigbee_dimmer" ],
                        "id": [{
                                "shortaddr": "0xBACE",
                                "endpoint": 11
                        }],
                        "extaddr": "0x00178801xxxxxxxx",
                        "ProfileId": "0xC05E",
                        "ManufacturerName": "Philips",
                        "ModelIdentifier": "LWB006",
                        "DateCode": "20150413",
                        "Version": "5.38.1.15095",
                        "dimlevel": 33,
                        "state": "off"
                },
                "zigbee-0xD19C": {
                        "protocol": [ "zigbee_dimmer" ],
                        "id": [{
                                "shortaddr": "0xD19C",
                                "endpoint": 3
                        }],
                        "extaddr": "0x84182600xxxxxxxx",
                        "ProfileId": "0xC05E",
                        "ManufacturerName": "OSRAM",
                        "ModelIdentifier": "Classic B40 TW - LIGHTIFY",
                        "DateCode": "20140331CNEF****",
                        "Version": "V1.03.20",
                        "poll-interval": 600,
                        "colortemp": 0,
                        "dimlevel": 3,
                        "state": "off"
                },


Note: colortemp is not available on Philips Hue white 

###### zigbee_shutter

        "zigbee-0xD1F3-0x01": {
                "protocol": [ "zigbee_shutter" ],
                "id": [{
                        "shortaddr": "0xD1F3",
                        "endpoint": 1
                }],
                "extaddr": "0x001FEE00xxxxxxxx",
                "ProfileId": "0x0104",
                "ManufacturerName": "ubisys",
                "ModelIdentifier": "J1 (5502)",
                "DateCode": "20170712-DE-FB0",
                "WindowCoveringType": 0,
                "CurrentPositionLift": 0,
                "CurrentPositionTilt": 0,
                "CurrentPositionLiftPercentage": 0,
                "CurrentPositionTiltPercentage": 0,
                "OperationalStatus": 0,
                "InstalledClosedLimitLift": 0,
                "InstalledClosedLimitTilt": 0,
                "Mode": 0,
                "dimlevel": 0,
                "state": "off"
        }


###### zigbee_metering

                "zigbee-0xDC6E-0x01-metering": {
                        "protocol": [ "zigbee_metering" ],
                        "id": [{
                                "shortaddr": "0xDC6E",
                                "endpoint": 1
                        }],
                        "extaddr": "0x00124B00xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "Bitron Home",
                        "ModelIdentifier": "902010/25",
                        "DateCode": "20150629",
                        "logfile": "/run/shm/p-plug.log",
                        "CurrentSummationDelivered": 442000,
                        "InstantaneousDemand": 17
                },
                "zigbee-0xA855-0x05-metering": {
                        "protocol": [ "zigbee_metering" ],
                        "id": [{
                                "shortaddr": "0xA855",
                                "endpoint": 5
                        }],
                        "extaddr": "0x001FEE00xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "ubisys",
                        "ModelIdentifier": "S2",
                        "DateCode": "20160302-DE-FB0",
                        "poll-interval": 60,
                        "logfile": "/run/shm/p-plug2.log",
                        "CurrentSummationDelivered": 2541009,
                        "InstantaneousDemand": 0
                },

###### zigbee_motion
 
                "zigbee-0x3B58-0x01": {
                        "protocol": [ "zigbee_motion" ],
                        "id": [{
                                "shortaddr": "0x3B58",
                                "endpoint": 1
                        }],
                        "extaddr": "0x00124B00xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "Bitron Home",
                        "ModelIdentifier": "902010/22",
                        "DateCode": "20150216        ",
                        "state": "on",
                        "battery": 100,
                        "ZoneStatus": 17
                },
 
###### zigbee_temperature
 
                "zigbee-0xC2D6-0x01": {
                        "protocol": [ "zigbee_temperature" ],
                        "id": [{
                                "shortaddr": "0xC2D6",
                                "endpoint": 1
                        }],
                        "extaddr": "0x00158D00xxxxxxxx",
                        "ProfileId": "(null)",
                        "ManufacturerName": "(null)",
                        "ModelIdentifier": "(null)",
                        "DateCode": "(null)",
                        "temperature": 21.0,
                        "humidity": 47,
                        "logfile": "/run/shm/temp-1.log"
                },
 
 
###### zigbee_thermostat
 
                 "zigbee-0x8655-0x01": {
                        "protocol": [ "zigbee_thermostat" ],
                        "id": [{
                                "shortaddr": "0x8655",
                                "endpoint": 1
                        }],
                        "extaddr": "0x000D6F00xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "Bitron Home",
                        "ModelIdentifier": "902010/32",
                        "DateCode": "2016061416166267",
                        "Version": "V1b225-20151013",
                        "localtemperature": 21.1,
                        "heatingsetpoint": 17.5,
                        "dimlevel": 17,
                        "poll-interval": 300,
                        "scheduler": "1,2,3,4,5 00:00 20; 04:00 22; 06:00 17; 12:00 20; 18:00 17.5; 0,6 00:00 20",
                        "logfile": "/run/shm/t-thermo-1.log",
                        "state": "off"
                },

###### zigbee_huemotion

                "zigbee-0xE5BB-0x02": {
                        "protocol": [ "zigbee_huemotion" ],
                        "id": [{
                                "shortaddr": "0xE5BB",
                                "endpoint": 2
                        }],
                        "extaddr": "0x00178801xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "Philips",
                        "ModelIdentifier": "SML001",
                        "DateCode": "20160630",
                        "Version": "6.1.0.18912",
                        "logfile": "/run/shm/temp-2.log",
                        "occupancy": 0,
                        "illuminance": 30,
                        "temperature": 2515,
                        "OccupiedToUnoccupiedDelay": 0,
                        "testmode": 0,
                        "battery": 100,
                        "sensitivity": 2,
                        "state": "off"
                },
 
###### zigbee_button

                "button1": {
                        "protocol": [ "zigbee_button" ],
                        "id": [{
                                "shortaddr": "0x8701",
                                "endpoint": 2
                        }],
                        "extaddr": "0x00178801xxxxxxxx",
                        "ProfileId": "0x0104",
                        "ManufacturerName": "Philips",
                        "ModelIdentifier": "RWL021",
                        "DateCode": "20160302",
                        "Version": "5.45.1.17846",
                        "state": "on",
                        "battery": 100,
                        "execute": "/home/pi/button.sh",
                        "ButtonId": 1,
                        "ButtonState": 2,
                        "ButtonCount": 1
                },


The execute parameter can have an empty string. If not empty, then the program or script is always executed when a button is pressed. The script is called with the following parameters:

    /home/pi/button.sh <shortaddr> <endpoint> <ButtonId> <ButtonState> <ButtonCount> 

This allows the script to execute different actions depending on which button is pressed or how often the same button is pressed.

Be aware that this script is called every time a button is pressed, so the script could be running more than once at the same time.

Example script:

``` bash
#!/bin/sh

shortaddr=$1
ep=$2
id=$3
state=$4
count=$5

# button 1 press 1x
if [ "$id" = "1" -a "$state" = "2" -a "$count" = "1" ]; then
  # do something
fi
```

On Philips Hue Dimmer switch the ButtonId can take the values 1, 2, 3, 4. ButtonState has the follwing values. 

Value | description
----- | -----------
0 | starting pressing the button
1 | keeping the button pressed
2 | releasing the button after short push
3 | releasing after long pressing

ButtonCount is how many times the same button is pressed and released within 2 seconds.

The Xiaomi button is one button, however, when quickly pressing and releasing the button, the ButtonState can take the values 1, 2, 3, 4. When pressing quickly five or more times, then ButtonState has the value 128. If pressing and releasing slowly within 2 seconds, then ButtonCount keeps increasing.

The Xiaomi Intelligent Door Window Sensor Control sends ZCL reports from cluster 0x0006 with bool values 0=closed or 1=on/open. It is a magenetic sensor that behaves like a button switch being pressed off or on. For that reason the zigbee_button protocol also can process the Xioami magnetic sensor as well. Status off means door/window is closed and status on means door/window is open.

###### Example rules

The first example turns a plug on and off if a button on/off is pressed on a Philips Hue dimmer switch. 

The second example is a motion sensor that turns an Osram Lightify light on and off.

        "rules": {
                "buttonrule1": {
                        "rule": "IF button1.ButtonId == 1 AND button1.ButtonCount == 1 THEN switch DEVICE zigbee-0xDC6E-plug TO on",
                        "active": 1
                },
                "buttonrule2": {
                        "rule": "IF button1.ButtonId == 4 AND button1.ButtonCount == 1 THEN switch DEVICE zigbee-0xDC6E-plug TO off",
                        "active": 1
                },
                "lightswitch-1": {
                        "rule": "IF zigbee-0x3B58-0x01.state == off THEN switch DEVICE zigbee-0xD19C TO off",
                        "active": 1
                },
                "lightswitch-2": {
                        "rule": "IF zigbee-0x3B58-0x01.state == on THEN switch DEVICE zigbee-0xD19C TO on",
                        "active": 1
                }
        },

## ZigBee Protocols in pilight


    pilight-send -H | grep zigbee
         zigbee_button                  ZigBee Button or Philips Hue Dimmer Switch
         zigbee_ctrl                    ZigBee Controller
         zigbee_dimmer                  ZigBee Level_Control
         zigbee_huemotion               ZigBee Philips Hue motion sensor
         zigbee_metering                ZigBee metering
         zigbee_motion                  ZigBee motion
         zigbee_switch                  ZigBee switch on/off
         zigbee_temperature             ZigBee Temperature and Humidity
         zigbee_thermostat              ZigBee thermostat

###### zigbee_ctrl

The zigbee_ctrl has the purpose to send ZigBee specific commands directly to any device in the network. The target device must have already joined the network before it can receive commands and send responses.

    pilight-send -p zigbee_ctrl -H
    Usage: pilight-send -p zigbee_ctrl [options]
    ...

        [zigbee_ctrl]
         -c --command           send zigbee command directly. Use -c help for list of commands
         -j --permitjoin        open ZigBee network and permit joining for 3 minutes
         -k --identify          identify device (requires -s <shortaddr> -e <endpoint>)
         -a --autodiscover      get all devices on network
                 (optional on one device) -s <shortaddr> -e <endpoint>
         -l --list              list lqi entries (neighbor table)
         -f --discoverattributes <cluster_id> (requires -s <shortaddr> -e <endpoint>)
         -r --setrep <cluster_id>       configure reporting on target device
                 -t --attributeid <attribute_id> -w attrtype <attribute_type> -s <shortaddr> -e <endpoint>
                 -m --minimum_reporting_interval <minimum>  -n --maximum_reporting_interval <maximum>
                 -o --reportablechange <change_value>
         -g --getrep <cluster_id>       get reporting configuration on target device
                 -t --attributeid <attribute_id> -s <shortaddr> -e <endpoint>
         -b|-u --bind|unbind <clusterid> -s <shortaddr> -x <source_extaddr> -y <source_ep>
                -d <destination_extaddr> -e <destination_ep>    set binding for cluster id on target endpoint to destination
         -v --listbind   -s <shortaddr>         list bindings on target
         

###### zigbee_switch

Turn a device on or off.

    pilight-send -p zigbee_switch -H
    Usage: pilight-send -p zigbee_switch [options]
    ...

        [zigbee_switch]
         -t --on                        send an on signal
         -f --off                       send an off signal
         -m --toggle                    send a toggle signal
         -s --shortaddr=id              control a device with this short address
         -e --endpoint=id               control a device with this endpoint id
         -r --read                      read attribute value OnOff from device
         -i --identify                  send identify request
         -a --autodiscover              discover ZigBee On/Off devices and automatically import to config

###### zigbee_dimmer

Turn a device on or off, set dimlevel. On some lights set color temperature. 

    pilight-send -p zigbee_dimmer -H
    Usage: pilight-send -p zigbee_dimmer [options]
    ...

        [zigbee_dimmer]
         -t --on                        send an on signal
         -f --off                       send an off signal
         -m --toggle                    send a toggle signal
         -d --dimlevel=dimlevel         send a specific dimlevel
         -c --colortemp=colortemparture         set colortemperature on lights 1 - 65279 (warmwhite to cold, 0x00fa = 4000 K
         -s --shortaddr=id              control a device with this short address
         -e --endpoint=id               control a device with this endpoint id
         -r --read                      read attribute values from device
         -i --identify                  send identify request
         -a --autodiscover              discover ZigBee Level_Control devices

###### zigbee_shutter

This proctocol implements the Window Covering Cluster and was tested with ubisys J1. The calibration command in this protocol has manufacturer specific commands and attributes that are only implemented on ubisys J1. After calibration it is possible to use more advanced positioning features like open/close 25%. Without calibration only move up/down and stop commands are available.



###### zigbee_metering

Read simple metering devices. 

    pilight-send -p zigbee_metering -H
    Usage: pilight-send -p zigbee_metering [options]
    ...
    
        [zigbee_metering]
         metering
         -r --read                      read summation (total) amount of electrical energy delivered
                                and power currently delivered

###### zigbee_button

There is no command to control button devices.

    pilight-send -p zigbee_button -H
    Usage: pilight-send -p zigbee_button [options]
    ...

        [zigbee_button]
         protocol support for Philips Hue Dimmer Switch and Xiaomi Smart Wireless Switch
         use rules to process button actions


###### zigbee_motion

Read a motion sensor status. On smoke detektor with siren this command can start the alarm for 1 second.

    pilight-send -p zigbee_motion -H
    Usage: pilight-send -p zigbee_motion [options]
    ...
    
        [zigbee_motion]
         motion
         -r --read                      read if motion detected
         -a --alarm                     start alarm 1 second


###### zigbee_huemotion

Read the attribute values from Hue motion sensort. The attributes are Occupancy (1 = occupied, 0 = unoccupied), Illuminance (in Lux), and Temperature

    pilight-send -p zigbee_huemotion -H
    Usage: pilight-send -p zigbee_huemotion [options]
    ...

        [zigbee_huemotion]
         motion
         -r --read                      read attribute values from Hue motion sensor
         -s --shortaddr
         -e --endpoint


###### zigbee_thermostat

Read and configure a thermostat device.

    pilight-send -p zigbee_thermostat -H
    Usage: pilight-send -p zigbee_thermostat [options]
    ...

        [zigbee_thermostat]
         thermostat
         -r --read                      read thermostat settings
         -m --settime           set time setting (without daylight saving time)
         -y --sync                      sync time setting on thermostat
         -l --logfile                   set logfile
         -h --heatingsetpoint           set thermostat heating setpoint
         -n --on                        set thermostat scheduler on (enable) and turn on thermostat
         -f --off                       set thermostat scheduler off (disable) and turn off thermostat
         -c --clear                     clear thermostat wwekly scheduler
         -d --scheduler                 set thermostat scheduler (format: 0,1,..,6 HH:MM TT.T )
                     scheduler format: day_of_week[0-6](comma , separated)
                     day of week: sunday = 0, monday = 1, ... saturday = 6
                     setpoint_time_temparature: HH:MM TT.T (semicolon ; separated)
                     example scheduler format: 0,1,2,3,4 15:00 21.5; 23:00 17.2
                             set monday to friday on 15:00 21.5 C and on 23:00 17.2 C



###### zigbee_temperature

There is no command to read tempature devices.

    pilight-send -p zigbee_temperature -H
    Usage: pilight-send -p zigbee_temperature [options]
    ...

        [zigbee_temperature]
         Temperature and Humidity Sensor


## More examples with ZigBee devices

Open network for new devices and permit join.

    pilight-send -p zigbee_ctrl -j

List devices in network. The new device will show up. All output is show in the console with daemon running in debug mode `pilight-daemon -D` (you need to run pilight-send in one terminal window and pilight-daemon in another terminal).

    pilight-send -p zigbee_ctrl -l
    
    INFO: 1. shortaddr = 0xF5F1, endpoint =   4 (04), extaddr = 0x00124B00xxxxxxxx, profile = 0x0104, deviceid = 0x0009
       Cluster:
    INFO: 2. shortaddr = 0xF5F1, endpoint =   1 (01), extaddr = 0x00124B00xxxxxxxx, profile = 0x0104, deviceid = 0x0051
       Cluster: 0x0000 0x0003 0x0004 0x0005 0x0006 0x0702

Note: Cluster 0x0006 is OnOff (switch) cluster and 0x0702 is metering cluster.

A discovery creates a file in /tmp directory with the device settings. Add the file content to config.json device settings.

    pilight-send -p zigbee_ctrl -s 0xF5F1 -e 1 -a
    
    INFO: [ZigBee] parseCommand created config file: /tmp/pilight_zigbee_switch-0xF5F1-0x01_json.config

In order to also add an entry for the metering protocol, run a discovery on metering devices. Add the metering device to config.json file.

    pilight-send -p zigbee_metering -a
    
    INFO: [ZigBee] parseCommand created config file: /tmp/pilight_zigbee_metering-0xF5F1-0x01_json.config

Configure binding on the new device in order to use reporting.

Look for the coordinator extended address (the RaspBee MAC address 0x00212EFFxxxxxxxx is also printed on the label). The coordinator always has the short address 0x0000. 

    pilight-send -p zigbee_ctrl -l
    
    INFO: 1. shortaddr = 0x0000, extaddr = 0x00212EFFxxxxxxxx

Run the following command show existing bindings. This device has no bindings.

    pilight-send -p zigbee_ctrl -s 0xF5F1 -e 1 -v
    
    DEBUG: [ZigBee] BIND 0x00124B00xxxxxxxx 00 00 00

Run the following command to add binding on the 0x0702 metering cluster. Note the extra white space in the -x and -d parameter to make sure the value is treated as string and not number.

    pilight-send -p zigbee_ctrl -b 0x0702 -s 0xF5F1 -x " 0x00124B00xxxxxxxx" -y 1 -d " 0x00212EFFxxxxxxxx" -e 1

The devices now shows the binding on cluster 0x0702 from endpoint 0x01 to the coordinator endpoint 0x01.

    pilight-send -p zigbee_ctrl -s 0xF5F1 -e 1 -v
    
    DEBUG: [ZigBee] <-BIND 0x00124B00xxxxxxxx 1 0 1 0x00124B00xxxxxxxx 0x01 0x0702 0x00212EFFxxxxxxxx 0x01


###### Bitron Home Smoke Detektor with siren 902010/24

The Smoke Detektor with siren can start the siren with the following command using the `-a` option. In this way the smoke detektor siren can be triggered by other events.

    pilight-send -p zigbee_motion -s 0x06DA -e 1 -a 

###### Bitron Home Thermostat 902010/32

The Bitron Home Thermostat can be configured with a heating setpoint (temperature) to turn heating on or off. It is also possible to configure a schedule for every day of the week (0-6). The schedule can be turned on or off. 

The heating setpoint is changed to 20 degrees with the following command. 

    pilight-send -p zigbee_thermostat -s 0x8655 -e 1 -h 20

The scheduler on the thermostat needs the current time, timezone and daylight savings start/end. Run the following command to sync the time settings on the thermostat.

    pilight-send -p zigbee_thermostat -s 0x8655 -e 1 -y

The current time settings may deviate 5 minutes during a month. Use the following command to update current time (without daylight savings start/end).

    pilight-send -p zigbee_thermostat -s 0x8655 -e 1 -m

Configure reporting in order to send the tempature every 300 seconds interval. Also binding is needed for reporting.

Example: configure binding (needed for reporting) on cluster 0x0201, endpoint 1.

    pilight-send -p zigbee_ctrl -s 0x8655 -e 1 -b 0x0201 -x " 0x000D6F00xxxxxxxx" -y 1 -d " 0x00212EFFxxxxxxxx"

Example: configure reporting on cluster 0x0201, attribute id 0x0000 (Temperature), attribute type 0x29 (signed 16-bit), minimum interval 300, maximum interval 300 seconds.

    pilight-send -p zigbee_ctrl -s 0x8655 -e 1 -r 0x0201 -t 0x0000 -w 0x29 -m 300 -n 300

Get reporting configuration.

    pilight-send -p zigbee_ctrl -s 0x8655 -e 1 -g 0x0201 -t 0
    
    DEBUG: [ZigBee] <-REP config 0x000D6F00xxxxxxxx 1 0x0201 dir 00 min 0 max 300 id 0x0000 type 0x29 0A 00

Configure the scheduler with the following command. The scheduler string is also added to "scheduler" setting in config.json but only for information purpose. Changing the "scheduler" setting in config.json has no effect on the device.

Example schedule: Monday to Friday (1,2,3,4,5) on 00:30 set 20 degrees, on 04:00 set 22 degrees, on 08:00 set 17.0 degrees, on 12:00 set 20 degrees, on 20:00 set 18.0 degrees. Saturday and Sunday (0,6) on 00:00 set 20 degrees, on 18:00 set 17 degrees.

    pilight-send -p zigbee_thermostat -s 0x8655 -e 1 -d "1,2,3,4,5 0:30 20; 4:00 22; 8:00 17.0; 12:00 20;  20:00 18.0; 0,6 0:00 20; 18:00 17"


Overview clusters on Bitron Home Thermostat.

    pilight-send -p zigbee_ctrl -s 0x8655 -e 1 -a
    
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 ep 0x01 profile 0x0104 deviceid 0x0301 deviceversion 0x00
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x0001 POWER_CONFIGURATION_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x000A TIME_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x0020 unknown
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x0201 THERMOSTAT_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x0204 THERMOSTAT_UI_CONF_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 In  0x0B05 DIAGNOSTICS_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 Out 0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x000D6F00xxxxxxxx 0x8655 0x01 Out 0x0019 OTAU_CLUSTER_ID
    
    <-ZCL attr 0x8655 1 0x0201 0x0004 Bitron Home
    <-ZCL attr 0x8655 1 0x0201 0x0005 902010/32
    <-ZCL attr 0x8655 1 0x0201 0x0006 2016061416166267
    <-ZCL attr 0x8655 1 0x0201 0x4000 V1b225-20151013


###### Philips Hue Dimmer switch

The Philips Hue Dimmer switch must be configured to send a command when a button is pressed. Create a binding on from endpoint 0x02 cluster 0xFC00 to coordinator endpoint 0x01. Press a button to make sure that the command is received. The dimmer switch is battery driven and will sleep most of the time to save power.

    pilight-send -p zigbee_ctrl -s 0x8701 -b 0xFC00 -x " 0x00178801xxxxxxxx" -y 2 -d " 0x00212EFFxxxxxxxx" -e 1

Show the bindings:

    pilight-send -p zigbee_ctrl -s 0x8701 -e 2 -v
    DEBUG: [ZigBee] <-BIND 0x00178801xxxxxxxx 1 0 1 0x00178801xxxxxxxx 0x02 0xFC00 0x00212EFFxxxxxxxx 0x01
     
As a result the dimmer switch will now send ZCL packets to the coordinator when a button is pressed, released or keept pressing.

     <-ZCL serverToClient 0x00178801xxxxxxxx 2 for cluster 0xFC00 manufacturer 0x100B 03 00 00 30 00 21 00 00

###### Philips Hue Motion sensor

The Philips Hue Motion sensor must be configured for reporting. ZigBee reporting also requires that binding has been configured for the cluster that should have reporting enabled.

The device has Endpoints 1 and 2 with the following clusters.

    <-EP 0x00178801xxxxxxxx 0xE5BB   2 (02)
    <-EP 0x00178801xxxxxxxx 0xE5BB   1 (01)

    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB ep 0x02 profile 0x0104 deviceid 0x0107 deviceversion 0x00
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x02 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x02 In  0x0001 POWER_CONFIGURATION_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x02 In  0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x02 In  0x0406 OCCUPANCY_SENSING_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x02 In  0x0400 ILLUMINANCE_MEASUREMENT_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x02 In  0x0402 TEMPERATURE_MEASUREMENT_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x02 Out 0x0019 OTAU_CLUSTER_ID

    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB ep 0x01 profile 0xC05E deviceid 0x0850 deviceversion 0x02
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 Out 0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 Out 0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 Out 0x0004 GROUPS_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 Out 0x0006 ONOFF_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 Out 0x0008 LEVEL_CONTROL_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 Out 0x0300 COLOR_CONTROL_CLUSTER_ID
    <-CLUSTER 0x00178801xxxxxxxx 0xE5BB 0x01 Out 0x0005 SCENES_CLUSTER_ID

Configure binding and reporting for clusters 0x0406, 0x0400, 0x0402 and 0x0001 on Endpoint 2. In this example the RaspBee (coordinator with shortaddr 0x0000) has the extended address 0x00212EFFxxxxxxxx and Endpoint 0x01. The reporting minumum in this example is 0 seconds for occupancy (0x0406) and 60 seconds for illuminance (0x0400) and temperature (0x0402). The maximum is 600 seconds. 


    pilight-send -p zigbee_ctrl -s 0xE5BB -r 0x0406 -t 0x0000 -e 2 -m 1 -n 600 -w 0x18

    pilight-send -p zigbee_ctrl -s 0xE5BB -g 0x0406 -t 0x0000 -e 2
    <-REP config 0x00178801xxxxxxxx 2 0x0406 dir 00 min 1 max 600 id 0x0000 type 0x18

    pilight-send -p zigbee_ctrl -s 0xE5BB -r 0x0400 -t 0x0000 -e 2 -m 60 -n 600 -w 0x21 -o 2000

    pilight-send -p zigbee_ctrl -s 0xE5BB -g 0x0400 -t 0x0000 -e 2 
    <-REP config 0x00178801xxxxxxxx 2 0x0400 dir 00 min 60 max 600 id 0x0000 type 0x21 E8 03 

    pilight-send -p zigbee_ctrl -s 0xE5BB -r 0x0402 -t 0x0000 -e 2 -m 60 -n 600 -w 0x29 -o 20

    pilight-send -p zigbee_ctrl -s 0xE5BB -g 0x0402 -t 0x0000 -e 2
    <-REP config 0x00178801xxxxxxxx 2 0x0402 dir 00 min 60 max 300 id 0x0000 type 0x29 0A 00
    
    pilight-send -p zigbee_ctrl -s 0xE5BB -r 0x0001 -t 0x0021 -e 2 -m 7200 -n 7200 -w 0x20 -o 0xFF
    
    pilight-send -p zigbee_ctrl -s 0xE5BB -g 0x0001 -t 0x0021 -e 2
    <-REP config 0x00178801xxxxxxxx 2 0x0001 dir 00 min 7200 max 7200 id 0x0021 type 0x20 FF


    pilight-send -p zigbee_ctrl -s 0xE5BB -b 0x0406 -x " 0x00178801xxxxxxxx" -y 2 -d " 0x00212EFFxxxxxxxx" -e 1 

    pilight-send -p zigbee_ctrl -s 0xE5BB -b 0x0400 -x " 0x00178801xxxxxxxx" -y 2 -d " 0x00212EFFxxxxxxxx" -e 1

    pilight-send -p zigbee_ctrl -s 0xE5BB -b 0x0402 -x " 0x00178801xxxxxxxx" -y 2 -d " 0x00212EFFxxxxxxxx" -e 1
    
    pilight-send -p zigbee_ctrl -s 0xE5BB -b 0x0001 -x " 0x00178801xxxxxxxx" -y 2 -d " 0x00212EFFxxxxxxxx" -e 1
    
    pilight-send -p zigbee_ctrl -s 0xE5BB -v

    <-BIND 0x0017880102035825 4 0 3 0x00178801xxxxxxxx 0x02 0x0402 0x00212EFFxxxxxxxx 0x01
    <-BIND 0x0017880102035825 4 0 3 0x00178801xxxxxxxx 0x02 0x0406 0x00212EFFxxxxxxxx 0x01
    <-BIND 0x0017880102035825 4 0 3 0x00178801xxxxxxxx 0x02 0x0400 0x00212EFFxxxxxxxx 0x01
    
    pilight-send -p zigbee_ctrl -s 0xE5BB -e 2 -c "zdpcmd 0xE5BB 0x0033 03"
    
    <-BIND 0x0017880102035825 4 3 1 0x0017880102035825 0x02 0x0001 0x00212EFFFF00893F 0x01
    

Note: The last command in this example does list bindings starting from index 0x03. It seems that deconz limits the bindings to maximum 3 entries.

The cluster 0x0400 Illuminance has attribute 0x0000 MeasuredValue. 
MeasuredValue represents the Illuminance in Lux (symbol lx) as follows:
MeasuredValue = 10,000 x log10 Illuminance + 1



###### Xiaomi Smart Wireless Switch

The Xiaomi button does not need any configuration. It always sends a ZCL attribute report from cluster 0x0006 to the coordinator.

    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 00 

###### Xiaomi Temperature and Humidity Smart Sensor

The Xiaomi Temperature and Humididy Smart Sensor does not respond to any ZigBee commands. When pressing and holding the button for several seconds, the blue led will flash and the device will join the network. Make sure the network is open/permits joining. The sensor sends ZCL attribute reports from cluster 0x0402 (temperature) and 0x0405 (humidity) on a very iregular interval. Sometimes it sends reports after minutes, sometimes after 10 hours.

    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0402 1 00 00 29 71 09 
    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0405 1 00 00 21 CD 13 

I don't know how to configure the Xiaomi device, but when the device joins the network, it shows 3 endpoints (0x01, 0x02, 0x03) with many clusters including goups and scenes cluster. Probably it is possible to configure the sensor to recall scenes or to trigger an alarm depending on the tempature.

    <-EP 0x00158D00xxxxxxxx 0x9FB8   1 (01)
    <-EP 0x00158D00xxxxxxxx 0x9FB8   2 (02)
    <-EP 0x00158D00xxxxxxxx 0x9FB8   3 (03)
    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0402 1 00 00 29 3F 09
    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0405 1 00 00 21 DE 1A

    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 ep 0x01 profile 0x0104 deviceid 0x5F01 deviceversion 0x01
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 In  0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 In  0x0019 OTAU_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 In  0xFFFF unknown
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 In  0x0012 unknown
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 Out 0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 Out 0x0004 GROUPS_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 Out 0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 Out 0x0005 SCENES_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 Out 0x0019 OTAU_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 Out 0xFFFF unknown
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x01 Out 0x0012 unknown

    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 ep 0x02 profile 0x0104 deviceid 0x5F02 deviceversion 0x01
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x02 In  0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x02 In  0x0012 unknown
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x02 Out 0x0004 GROUPS_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x02 Out 0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x02 Out 0x0005 SCENES_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x02 Out 0x0012 unknown

    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 ep 0x03 profile 0x0104 deviceid 0x5F03 deviceversion 0x01
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x03 In  0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x03 In  0x000C unknown
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x03 Out 0x0004 GROUPS_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x03 Out 0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x03 Out 0x0005 SCENES_CLUSTER_ID
    <-CLUSTER 0x00158D00xxxxxxxx 0x9FB8 0x03 Out 0x000C unknown


###### Xiaomi Intelligent Door Window Sensor Control (magnetic sensor)

The Xiaomi magnetic sensor only sends ZCL reports after joining the network. Before joining the network, the blue led will flash when the magnet is moving close to or away from the contact. During joining, when pressing the button in the small hole, the blue led will flash several times. 

    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 00 
    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 01 

During joining, the following additional ZCL reports are send.

    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0000 1 05 00 42 12 6C 75 6D 69 2E 73 65 6E 73 6F 72 5F 6D 61 67 6E 65 74 
    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0000 1 01 00 20 0A 
    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0000 1 02 FF 4C 06 00 10 01 21 EF 0B 21 A8 01 24 00 00 00 00 00 21 00 01 20 58    

When pressing the small button in the hole, the ZCL report sends basic cluster (0x0000) attribute 0x0005 string "lumi.sensor_magnet".

    <-ZCL attribute report 0x00158D00xxxxxxxx 0x0000 1 05 00 42 12 6C 75 6D 69 2E 73 65 6E 73 6F 72 5F 6D 61 67 6E 65 74 

After joining, the blue led will not flash anymore during activity.

Use `pilight-send zigbee_ctrl -l` to get the short address. The output is written to the logfile or when running `pilight-daemon -D`. A file in /tmp directory is created. The endpoint should be assigned to 1.


###### ubisys shutter control J1

The ubisys J1 sends the follwing ZigBee responses after joining the network. The Endpoints and Clusters on each endpoint are send.

    <-EP 0x001FEE00xxxxxxxx 0xD1F3   1 (01)
    <-EP 0x001FEE00xxxxxxxx 0xD1F3   2 (02)
    <-EP 0x001FEE00xxxxxxxx 0xD1F3   3 (03)
    <-EP 0x001FEE00xxxxxxxx 0xD1F3 200 (C8)
    <-EP 0x001FEE00xxxxxxxx 0xD1F3 232 (E8)
    <-EP 0x001FEE00xxxxxxxx 0xD1F3 242 (F2)
    
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 ep 0x01 profile 0x0104 deviceid 0x0202 deviceversion 0x00
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x01 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x01 In  0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x01 In  0x0004 GROUPS_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x01 In  0x0005 SCENES_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x01 In  0x0102 unknown
    
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 ep 0x02 profile 0x0104 deviceid 0x0203 deviceversion 0x00
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x02 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x02 In  0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x02 Out 0x0005 SCENES_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x02 Out 0x0102 unknown
    
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 ep 0x03 profile 0x0104 deviceid 0x0501 deviceversion 0x00
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x03 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x03 In  0x0702 SIMPLE_METERING_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0x03 In  0x0B04 unknown
    
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 ep 0xC8 profile 0xC003 deviceid 0x0000 deviceversion 0x00
    
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 ep 0xE8 profile 0x0104 deviceid 0x0507 deviceversion 0x00
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0xE8 In  0x0000 BASIC_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0xE8 In  0x0015 unknown
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0xE8 In  0xFC00 unknown
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0xE8 Out 0x0003 IDENTIFY_CLUSTER_ID
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0xE8 Out 0x0019 OTAU_CLUSTER_ID
    
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 ep 0xF2 profile 0xA1E0 deviceid 0x0066 deviceversion 0x00
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0xF2 In  0x0021 unknown
    <-CLUSTER 0x001FEE00xxxxxxxx 0xD1F3 0xF2 Out 0x0021 unknown

Binding:
Notice: The ubisys J1 already has configured binding on cluster 0x0102 from Endpoint 0x02 to Endpoint 0x01. This cluster uses the binding table for managing command targets. When factory fresh, this cluster is bound to Endpoint #1 to enable local control.

The second binding in this example has been configured in order to enable reporting.

    <-BIND 0x001FEE00xxxxxxxx 2 0 2 0x001FEE00xxxxxxxx 0x02 0x0102 0x001FEE00xxxxxxxx 0x01
    <-BIND 0x001FEE00xxxxxxxx 2 0 2 0x001FEE00xxxxxxxx 0x01 0x0102 0x00212EFFxxxxxxxx 0x01

Reporting: 
This example shows how reporting is configured on Cluster 0x0102 Attributes 0x0003, 0x0004, 0x0008, 0x0009, 0x000A.

    pilight-send -p zigbee_ctrl -s 0xD1F3 -e 1 -g 0x0102 -t 0x0003
    
    <-REP config 0x001FEE00xxxxxxxx 1 0x0102 dir 00 min 60 max 600 id 0x0003 type 0x21 01 00 
    <-REP config 0x001FEE00xxxxxxxx 1 0x0102 dir 00 min 60 max 600 id 0x0004 type 0x21 01 00 
    <-REP config 0x001FEE00xxxxxxxx 1 0x0102 dir 00 min 60 max 600 id 0x0008 type 0x20 01 
    <-REP config 0x001FEE00xxxxxxxx 1 0x0102 dir 00 min 60 max 600 id 0x0009 type 0x20 01
    <-REP config 0x001FEE00xxxxxxxx 1 0x0102 dir 00 min 60 max 600 id 0x000A type 0x18 

