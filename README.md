=======
### If you like my solutions, then don't hesitate to donate:
<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=curlymoo1%40gmail%2ecom&lc=NL&item_name=CurlyMoo&no_note=1&no_shipping=1&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted" target="_blank">
<img alt="PayPal" title="PayPal" border="0" src="https://www.paypalobjects.com/nl_NL/NL/i/btn/btn_donateCC_LG.gif" ></a><br />
<hr>
I bought one of these Arduino 433.92Mhz sender and receiver kits for controlling my Klik Aan Klik Uit and Elro devices. 
They are called "433MHz Superheterodyne 3400 RF Transmitter and Receiver link kit" and can be found on ebay for about $10.
<!--<br /><br />
Please make sure to use a low-pass filter when you connect the receiver to your Raspberry Pi. A low-pass filter, will filter
most of the noise so only the actual signals are passed through to the GPIO pins. This code will also work without a low-pass filter,
but then you can't use it together with lirc and/or XBMC while receiving. A low-pass filter is created like this:
<br />
<img src="http://provideyourown.com/wp-content/uploads/tech/CRLowPass1.png" alt="Low-pass filter" title="Low-pass filter" border="0" /><br />
I used a 220 Ohm resistor and a 100nF capacitor. I haven't experimented with other values. -->
<hr>
__Don't forget to use the command `sudo` prior to all commands below when you're not logged in as root__
<hr>
To fully benefit from my code, you should build a low-pass filter to make sure no noise is being received by the receiver.
This filter only costs about $1 and works absolutely perfect. All components are commonly used and can be found on ebay or at a local DIY shops.<br />
<img src="http://i.imgur.com/yRp532m.jpg" alt="Low-pass filter" title="Low-pass filter" border="0" />
<hr>
Those who are not using a low-pass filter are adviced to use this code with the lirc kernel module.
The downside of using the lirc kernel module is that this it is not entirely standalone, because you have to have the lirc_rpi kernel module loaded. This kernel
module is shipped with the standard raspberry pi kernel. To load this module, just run:
```
modprobe lirc_rpi
```
The module will use GPIO 18 as the receiver pin and GPIO 17 as the sender pin. 
To let the module use other pins, load the module with the following arguments:
```
modprobe lirc_rpi gpio_in_pin=18 gpio_out_pin=25 debug=1
```
If you want to load this module at start up, just add the same lines to `/etc/modules` except the `modprobe` part.
<br />
If the kernel module has been succesfully loaded, you should see a new lirc socket appear in `/dev/`:
```
root@pi:~# ls -Al /dev/lirc*
crw-rw---T 1 root video 249, 0 jan  1  1970 /dev/lirc0
crw-rw---T 1 root video 249, 1 jan  1  1970 /dev/lirc1
lrwxrwxrwx 1 root root      21 jan  1  1970 /dev/lircd -> ../var/run/lirc/lircd
```
If you don't use the lirc kernel module, then make sure you have set the `GPIO_IN_PIN` and `GPIO_OUT_PIN` in the `libs/settings.h` to the right values.
<br />
The core of this program is the pilight-daemon. This will run itself in the background. You can then use the pilight-receiver or the pilight-sender
to connect to the pilight-daemon to receive or send codes. The pilight-daemon also has the possibility to automatically invoke another script.
So you can use the pilight-daemon to log incoming codes.<br />
To alter the default behavior of the daemon, a settings file can be created.<br />
The default location to where this file needs to be stores is `/etc/pilight/settings.json`.<br />
All options and its default values are show below:<br />
```
{
	"port": 5000,
	"mode": "server",
	"log-level": 4,
	"pid-file": "/var/log/daemon/pilight.pid",
	"config-file": "/etc/pilight/config.json",
	"log-file": "/var/log/pilight.log",
	"process-file": "",
	"send-repeats": 10,
	"receive-repeats": 1,
	"socket": "/dev/lirc0",
	"use-lirc": 0,
	"gpio-sender": 0,
	"gpio-receiver": 1,
	"webserver-enable": 1,
	"webserver-root": "/usr/local/share/pilight/",
	"webserver-port": 5001
}
```
__port__: change the default port the daemon will run at<br />
__mode__: should the daemon be ran as main server or as a client<br />
__server__: the server the client should connect to [ x.x.x.x, xxxx ] (only in client mode)<br />
__log-level__: The default log level of the daemon 1 till 5<br />
__pid-file__: The default location of the process id file<br />
__config-file__: The default location of the config file (only in server mode)<br />
__log-file__: The default location of the log id file<br />
__process-file__: The optional process file<br />
__send-repeats__: How many times should a code be send<br />
__receive-repeats__: How many times should a code be recieved before marked valid<br />
__socket__: What lirc socket should we read from (use-lirc: 1)<br />
__use-lirc__: Use the lirc_rpi kernel module or plain gpio access<br />
__gpio-sender__: To what pin is the sender connected (use-lirc: 0)<br />
__gpio-receiver__: To what pin is the reciever connected (use-lirc: 0)<br />
__webserver-enable__: Enable the built-in webserver<br />
__webserver-port: On what port does the webserver need to run<br />
__webserver-root: The webserver root path<br />
<hr>
The output of the receiver will be as follow:
```
root@pi:~# pilight-send -p kaku_switch -i 100 -u 15 -f
```
```
root@pi:~# pilight-receiver
{
	"origin": "sender",
	"protocol": "arctech_switches",
	"code": {
		"id": 100,
		"unit": 15,
		"state": off
	}
}

```
```
root@pi:~# pilight-send -p kaku_dimmer -i 100 -u 15 -d 15
```
```
root@pi:~# pilight-receiver
{
	"origin": "sender",
	"protocol": "arctech_dimmers",
	"code": {
		"id": 100,
		"unit": 15,
		"state": on,		
		"dimlevel": 15
	}
}
```
```
root@pi:~# pilight-send -p elro -i 10 -u 15 -t
```
```
root@pi:~# pilight-receiver
{
	"origin": "sender",
	"protocol": "sartano",
	"code": {
		"id": 10,
		"unit": 15,
		"state": off
	}
}
```
<hr>
The sender will pilight-send will send codes to the pilight-daemon:
```
root@pi:~# ./pilight-send -p kaku_switch -i 1 -u 1 -t
```
The command line arguments depend on the protocol used e.g.:
```
root@pi:~# pilight-send -H
Usage: pilight-send -p protocol [options]
         -H --help                      display this message
         -V --version                   display version
         -S --server=127.0.0.1          connect to server address
         -P --port=5000                 connect to server port
         -p --protocol=protocol         the protocol that you want to control

The supported protocols are:
         coco_switch                    CoCo Technologies Switches
         nexa_switch                    Nexa Switches
         dio_switch                     D-IO (Chacon) Switches
         kaku_switch                    KlikAanKlikUit Switches
         kaku_dimmer                    KlikAanKlikUit Dimmers
         cogex                          Cogex Switches
         kaku_old                       Old KlikAanKlikUit Switches
         elro                           Elro Switches
         select-remote                  SelectRemote Switches
         impuls                         Impuls Switches
         relay                          Control connected relay's
         raw                            Raw codes
root@pi:~# pilight-send -p kaku_switch -h
Usage: pilight-send -p kaku_switch [options]
         -H --help                      display this message
         -V --version                   display version
         -S --server=127.0.0.1          connect to server address
         -P --port=5000                 connect to server port
         -p --protocol=protocol         the protocol that you want to control

        [kaku_switch]
         -t --on                        send an on signal
         -f --off                       send an off signal
         -u --unit=unit                 control a device with this unit code
         -i --id=id                     control a device with this id
         -a --all                       send command to all devices with this id
```
Examples are:
```
root@pi:~# pilight-send -p kaku_switch -t 1 -u 1 -t
root@pi:~# pilight-send -p kaku_dimmer -t 1 -u 1 -d 15
root@pi:~# pilight-send -p elro -t 1 -u 1 -t
```
<hr>
To control devices that are not yet supported one can use the `raw` protocol. This protocol allows the sending of raw codes.
To figure out what the raw codes of your devices are you can run the debugger first. When you run the debugger it will wait
for you to press a button for the device you want to control. Once you held the button long enough to control the device 
using the raw codes.
```
root@pi:~# pilight-debug
header:      	10
pulse:          5
footer:         38
rawLength:      132
binaryLength:   33
Raw code:
286 2825 286 201 289 1337 287 209 283 1351 287 204 289 1339 288 207 288 1341 289 207 281 1343 284 205 292 1346 282 212 283 1348 282 213 279 1352 282 211 281 1349 282 210 283 1347 284 211 288 1348 281 211 285 1353 278 213 280 1351 280 232 282 1356 279 213 285 1351 276 215 285 1348 277 216 278 1359 278 216 279 1353 272 214 283 1358 276 216 276 1351 278 214 284 1357 275 217 276 1353 270 217 277 1353 272 220 277 1351 275 220 272 1356 275 1353 273 224 277 236 282 1355 272 1353 273 233 273 222 268 1358 270 219 277 1361 274 218 280 1358 272 1355 271 243 251 11302
Binary code:
000000000000000000000000010100011
```
You can now use the raw code to control your device:
```
root@pi:~# pilight-send -p raw -c "286 2825 286 201 289 1337 287 209 283 1351 287 204 289 1339 288 207 288 1341 289 207 281 1343 284 205 292 1346 282 212 283 1348 282 213 279 1352 282 211 281 1349 282 210 283 1347 284 211 288 1348 281 211 285 1353 278 213 280 1351 280 232 282 1356 279 213 285 1351 276 215 285 1348 277 216 278 1359 278 216 279 1353 272 214 283 1358 276 216 276 1351 278 214 284 1357 275 217 276 1353 270 217 277 1353 272 220 277 1351 275 220 272 1356 275 1353 273 224 277 236 282 1355 272 1353 273 233 273 222 268 1358 270 219 277 1361 274 218 280 1358 272 1355 271 243 251 11302"
```
<hr>
The learner does the same as the debugger but is more extensive. It will try to figure out as much as possible about your protocol.
At this moment only switches are supported, so not dimmers or others devices. Just follow the steps of the learner and when you
where successfull, it will print the following information (in case of Klik Aan Klik Uit):
```
root@pi:~# pilight-learn
1. Please send and hold one of the OFF buttons. Done.

2. Please send and hold the ON button for the same device
   as for which you send the previous OFF button. Done.

3. Please send and hold (one of the) ALL buttons.
   If you're remote doesn't support turning ON or OFF
   all devices at once, press the same OFF button as in
   the beginning. Done.

4. Please send and hold the ON button with the lowest ID. Done.

5. Please send and hold the ON button with the second to lowest ID. Done.

6. Please send and hold the ON button with the highest ID. Done.

--[RESULTS]--

header:         10
pulse:          5
footer:         38
rawLength:      132
binaryLength:   33

on-off bit(s):  27
all bit(s):     26
unit bit(s):    28 29 30 31

Raw code:
276 2833 275 216 285 1356 279 215 282 1356 273 221 276 1352 276 216 282 1356 272 219 275 1353 272 228 266 1365 269 219 279 1360 270 225 269 1361 269 224 269 1359 273 242 280 1358 271 224 313 1343 271 226 274 1365 269 222 274 1366 269 225 272 1362 266 228 267 1366 262 227 265 1362 267 223 274 1363 267 227 268 1364 262 230 264 1366 267 224 274 1371 266 234 263 1362 266 226 275 1366 265 227 274 1364 265 228 264 1365 263 235 262 1384 251 1373 264 239 265 229 262 1367 264 1368 260 257 267 1368 264 232 306 1361 264 230 265 1360 268 230 264 1363 259 235 263 11306
Raw simplified:
On:     010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001010000010100000100010001010001
Off:    010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001010000010100000100010001010001
All:    010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001010001000001000100010001000101
Unit 1: 010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001010000010100000100010001010001
Unit 2: 010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001010000010100000100010100000101
Unit 3: 010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001000100010001010000010100010001000100010001
Binary code:
On:     000000000000000000000000010100011
Off:    000000000000000000000000010000011
All:    000000000000000000000000011000001
Unit 1: 000000000000000000000000010100011
Unit 2: 000000000000000000000000010100101
Unit 3: 000000000000000000000000010111111
```
If may be possible that the learner prints out different values as shown here. This may happen in your device is limited in the amount of values it can send.
The only variable that isn't recorded, is the ID. Most of the times, the ID is stored in the remaining (sequence) of bits. In case of Klik Aan Klik Uit, the ID is stored
in bits 0 till 25. Also notice that both the debugger and the learner are highly experimental.
<hr>
To use the controller, a config file is needed. This looks like this:<br />
_The `type` and the `order` setting will automatically be added by the pilight-daemon_
```
{
	"living": {
		"name": "Living",
		"order": 1,
		"bookshelve": {
			"name": "Book Shelve Light",
			"order": 1,
			"protocol": "kaku_switch",
			"type": 1,
			"id": 1234,
			"unit": 0,
			"state": "off",
			"values": [ "on", "off" ]
		},
		"main": {
			"name": "Main",
			"order": 2,
			"protocol": "kaku_dimmer",
			"type": 1,
			"id": 1234,
			"unit": 1,
			"state": "on",
			"dimlevel", 0,
			"values": [ "on", "off" ]
		},
		"television": {
			"name": "Television",
			"order": 3,
			"protocol": "relay",
			"type": 1,
			"gpio": 3,
			"state": "off",
			"values": [ "on", "off" ]
		} 
	},
	"bedroom": {
		"name": "Bedroom",
		"order": 2,
		"main": {
			"name": "Main",
			"order": 1,
			"protocol": "elro",
			"type": 1,
			"id": 5678,
			"unit": 0,
			"state": "on",
			"values": [ "on", "off" ]
		}
	},
	"garden": {
		"name": "Garden",
		"order": 3,
		"weather": {
			"name": "Weather Station",
			"order": 1,
			"protocol": "alecto",
			"type": 3,
			"id": 100,
			"humidity": 50,
			"temperature": 1530,
			"battery": 1
		}
	}		
}
```
To control a device, you can know easily use the `pilight-control`:
```
root@pi:~# pilight-control -l living -d bookshelve
```
The config file will automatically be updated with the status of the configured devices. So while sending, but also while receiving.<br />
To force the device in a specific state:
```
root@pi:~# pilight-control -l living -d bookshelve -s on
```
To also simultaniously change the value a device such as a dimmer you can use:
```
root@pi:~# pilight-control -l living -d main -s on -v dimlevel=1
```
<hr>
## NODES
One major feature of this program is that the daemon can run in `client` mode. This means the daemon will connect to another daemon that is considered the `server`.
Every action that happens to one of the daemons is automatically communicated to the others. That means that areas with low coverages can be covered by putting multiple "nodes" throughout the house / office.
