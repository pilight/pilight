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
The command line arguments are as follows:
```
root@pi:~# ./send -h
Usage: send [options]
         -h --help                      display this message
         -v --version                   display version
         -s --socket=socket             read from given socket
         -p --protocol=protocol         which device are you trying to control
         -t --on                        send an on signal
         -t --off                       send an off signal
         -u --unit=unit                 control a device with this unit code
         -i --id=id                     control a device with this id
         -a --all                       send command to all devices with this id
         -d --dimlevel                  send a specific dimlevel
         -r --repeat=repeat             number of times the command is send
```
Examples are:
```
root@pi:~# ./send -p kaku_switch -t 1 -u 1 -t
root@pi:~# ./send -p kaku_dimmer -t 1 -u 1 -d 15
root@pi:~# ./send -p elro -t 1 -u 1 -t
```