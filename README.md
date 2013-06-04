=======
<a href="http://flattr.com/thing/1106962/" target="_blank">
<img src="http://api.flattr.com/button/flattr-badge-large.png" alt="Flattr this" title="Flattr this" border="0" /></a>
<br />
I bought one of these Arduino 433.92Mhz sender and receiver kits for controlling my Klik Aan Klik Uit and Elro devices. 
They are called "433MHz Superheterodyne 3400 RF Transmitter and Receiver link kit" and can be found on ebay for about $10.
<br /><br />
This new code uses lirc for the interaction with the hardware. The advantage is that we can now use reliable existing code to build 
the 433.92Mhz programs on. The downside is that this new code is not entirely standalone.
To control the new receiver, you have to have the lirc_rpi kernel module loaded. This kernel
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
The receiver will read from `/dev/lirc0` by default, but if you want it to read other sockets, run the receiver as follows:
```
root@pi:~# receive --socket=/dev/lirc1
```
The output of the receiver will be as follow:
```
root@pi:~# receive
# KaKu: ./send -p kaku_switch -i 100 -u 15 -f
id: 100, unit: 15, state: off
id: 100, unit: 15, state: off
id: 100, unit: 15, state: off
id: 100, unit: 15, state: off
id: 100, unit: 15, state: off

# KaKu: ./send -p kaku_dimmer -i 100 -u 15 -d 15
id: 100, unit: 15, dim: 15
id: 100, unit: 15, dim: 15
id: 100, unit: 15, dim: 15
id: 100, unit: 15, dim: 15
id: 100, unit: 15, dim: 15

# Elro: ./send -p elro -i 10 -u 15 -t
id: 10, unit: 15, state: on
id: 10, unit: 15, state: on
id: 10, unit: 15, state: on
id: 10, unit: 15, state: on
id: 10, unit: 15, state: on
id: 10, unit: 15, state: on
id: 10, unit: 15, state: on
id: 10, unit: 15, state: on
```
The command line arguments are as follows:
```
root@pi:~# ./receive -h
Usage: receive [options]
         -h --help              display usage summary
         -v --version           display version
         -d --socket=socket     read from given socket
```
The sender will send from `/dev/lirc0` by default, but if you want it to read other sockets, run the sender as follows:
```
root@pi:~# send --socket=/dev/lirc1
```
The command line arguments depend on the protocol used e.g.::
```
root@pi:~# ./send -h
Usage: send -p protocol [options]
         -h --help                      display this message
         -v --version                   display version
         -p --protocol=protocol         the device that you want to control
         -s --socket=socket             read from given socket
         -r --repeat=repeat             number of times the command is send

The supported protocols are:
         kaku_switch                    KlikAanKlikUit Switches
         kaku_dimmer                    KlikAanKlikUit Dimmers
         kaku_old                       Old KlikAanKlikUit Switches
         elro                           Elro Switches
         raw                            Raw codes
root@pi:~# ./send -p kaku_switch -h
Usage: send -p kaku_switch [options]
         -h --help                      display this message
         -v --version                   display version
         -p --protocol=protocol         the device that you want to control
         -s --socket=socket             read from given socket
         -r --repeat=repeat             number of times the command is send

        [kaku_switch]
         -t --on                        send an on signal
         -t --off                       send an off signal
         -u --unit=unit                 control a device with this unit code
         -i --id=id                     control a device with this id
         -a --all                       send command to all devices with this id
```
Examples are:
```
root@pi:~# ./send -p kaku_switch -t 1 -u 1 -t
root@pi:~# ./send -p kaku_dimmer -t 1 -u 1 -d 15
root@pi:~# ./send -p elro -t 1 -u 1 -t
```
To control devices that are not yet supported one can use the `raw` protocol. This protocol allows the sending of raw codes.
To figure out what the raw codes of your devices are you can run the debugger first. When you run the debugger if will wait
for you to press a button for the device you want to control. Once you held the button long enough, the debugger will
print all necessary information in order to create a new protocol, or to control the device using the raw codes.
```
root@pi:~# ./debug
header[0]:      286
header[1]:      2825
low:            271
high:           1355
footer:         11302
rawLength:      132
binaryLength:   33
Raw code:
286 2825 286 201 289 1337 287 209 283 1351 287 204 289 1339 288 207 288 1341 289 207 281 1343 284 205 292 1346 282 212 283 1348 282 213 279 1352 282 211 281 1349 282 210 283 1347 284 211 288 1348 281 211 285 1353 278 213 280 1351 280 232 282 1356 279 213 285 1351 276 215 285 1348 277 216 278 1359 278 216 279 1353 272 214 283 1358 276 216 276 1351 278 214 284 1357 275 217 276 1353 270 217 277 1353 272 220 277 1351 275 220 272 1356 275 1353 273 224 277 236 282 1355 272 1353 273 233 273 222 268 1358 270 219 277 1361 274 218 280 1358 272 1355 271 243 251 11302
Binary code:
000000000000000000000000010100011
```
You can now use the raw code to control your device:
```
root@pi:~# ./send -p raw -c "286 2825 286 201 289 1337 287 209 283 1351 287 204 289 1339 288 207 288 1341 289 207 281 1343 284 205 292 1346 282 212 283 1348 282 213 279 1352 282 211 281 1349 282 210 283 1347 284 211 288 1348 281 211 285 1353 278 213 280 1351 280 232 282 1356 279 213 285 1351 276 215 285 1348 277 216 278 1359 278 216 279 1353 272 214 283 1358 276 216 276 1351 278 214 284 1357 275 217 276 1353 270 217 277 1353 272 220 277 1351 275 220 272 1356 275 1353 273 224 277 236 282 1355 272 1353 273 233 273 222 268 1358 270 219 277 1361 274 218 280 1358 272 1355 271 243 251 11302"
```
