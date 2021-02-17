.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

Fanju 3378
==========

+------------------+-------------+
| **Feature**      | **Support** |
+------------------+-------------+
| Sending          | |no|        |
+------------------+-------------+
| Receiving        | |yes|       |
+------------------+-------------+
| Config           | |yes|       |
+------------------+-------------+

.. rubric:: Supported Brands

+------------------+------------------+
| **Brand**        | **Protocol**     |
+------------------+------------------+
| Fanju            | fanju            |
+------------------+------------------+

.. rubric:: Sender Arguments

*None*

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "weather": {
         "protocol": [ "fanju" ],
         "id": [{
           "id": 136,
           "channel": 1
         }],
         "temperature": 15.0,
         "humidity": 99.00,
         "battery": 1
        }
     },
     "gui": {
       "weather": {
         "name": "Weather Station",
         "group": [ "Outside" ],
         "media": [ "all" ]
       }
     }
   }

+------------------+-----------------+
| **Option**       | **Value**       |
+------------------+-----------------+
| id               | 0 - 9           |
+------------------+-----------------+
| channel          | 0 - 3           |
+------------------+-----------------+
| temperature      | -20 - 50        |
+------------------+-----------------+
| humidity         | 20 - 99         |
+------------------+-----------------+
| battery          | 0 - 1           |
+------------------+-----------------+

.. rubric:: Optional Settings

:underline:`Device Settings`

+--------------------+-------------+------------+---------------------------+
| **Setting**        | **Default** | **Format** | **Description**           |
+--------------------+-------------+------------+---------------------------+
| humidity-offset    | 0           | number     | Adjust humidity value     |
+--------------------+-------------+------------+---------------------------+
| temperature-offset | 0           | number     | Adjust temperature value  |
+--------------------+-------------+------------+---------------------------+

:underline:`GUI Settings`

+----------------------+-------------+------------+-----------------------------------------------------------+
| **Setting**          | **Default** | **Format** | **Description**                                           |
+----------------------+-------------+------------+-----------------------------------------------------------+
| temperature-decimals | 1           | number     | How many decimals the GUIs should display for temperature |
+----------------------+-------------+------------+-----------------------------------------------------------+
| humidity-decimals    | 1           | number     | How many decimals the GUIs should display for humidity    |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-temperature     | 1           | 1 or 0     | '1' display the temperature value                         |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-humidity        | 1           | 1 or 0     | '1' display the humidity value                            |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-battery         | 1           | 1 or 0     | '1' display the battery value                             |
+----------------------+-------------+------------+-----------------------------------------------------------+

.. rubric:: Comments

This weather station is available at AliExpress. Description for the device could be: “Fanju Wireless Digital Temperature Humidity Wireless Sensor meter In/Outdoor used with Weather Station”, and “Match 3378/74/79C/65/89 Indoor Receiver”. The sensor can be identified by the option to choose from three different channels.

Humidity and Battery are assumed to be supported by this protocol, but might not be. The temperature value may be a half to one degree off and the humidity value seems to vary regularly. Note that the ID of the sensor is a rolling-code, i.e. it changes every time the device is powered off (c.q. batteries are changed).

.. rubric:: Protocol

This protocol has similarities with the auriol protocol.

The message comprises 10 nibbles, resulting in a total length of 40 bits.
The bits have the following specific meaning:

               1    1    2    2    2    3    3
0    4    8    2    6    0    4    8    2    6

0010 0001 1000 1100 0110 0101 1000 0011 0101 0010 
2    1    8    12   6    5    8    3    5    2   
AAAA AAAA BBBB xyzz CCCC CCCC CCCC DDDD DDDD EEFF

A - rolling-code
B - checksum
B - x 0=scheduled, 1=requested transmission
    y 0=ok, 1=battery low
	zz <unknown>
C - temperature in Fahrenheit (binary)
D - humidity (BCD)
E - <unknown>
F - channel

.. rubric:: Checksum

The checksum is a Linear Feedback Shift Register (LFSR) Hash. Starting with a checksum value of 0x0 and mask value 0xC, for each '1' bit, starting with bit position 0, the mask is shifted right one bit and the checksum is XORed with this mask value. If the shifted right bit was '1', the checksum value is additionally XORed with 0x9. Note that the end nibble is put in place of the checksum nibble, prior to the hashnumber calculation.

This protocol was created for pilight with the help of pilight-debug.
