.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

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

+--------------------------------------+----------------+
| **Brand**                            | **Protocol**   |
+--------------------------------------+----------------+
| Nexus Weather Stations               | nexus          |
+--------------------------------------+----------------+
| Digoo DG-R8H/DG-R8S Weather Stations | dgr8h          |
+--------------------------------------+----------------+
| Sencor SWS 21TS Weather Stations     | sencor21ts     |
+--------------------------------------+----------------+


.. rubric:: Sender Arguments

*None*

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
      "devices": {
        "weather": {
          "protocol": [ "nexus" ],
          "id": [{
            "id": 97,
            "channel": 1
          }],
          "temperature": 18.90,
          "humidity": 41.00,
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
| id               | 0 - 255         |
+------------------+-----------------+
| channel          | 0 - 3           |
+------------------+-----------------+
| temperature      | -39.9 - 59.9    |
+------------------+-----------------+
| humidity         | 10 - 99         |
+------------------+-----------------+
| battery          | 0 - 1           |
+------------------+-----------------+

.. rubric:: Optional Settings

:underline:`Device Settings`

+--------------------+-------------+------------+---------------------------+
| **Setting**        | **Default** | **Format** | **Description**           |
+--------------------+-------------+------------+---------------------------+
| temperature-offset | 0           | number     | Correct temperature value |
+--------------------+-------------+------------+---------------------------+
| humidity-offset    | 0           | number     | Correct humidity value    |
+--------------------+-------------+------------+---------------------------+

:underline:`GUI Settings`

+----------------------+-------------+------------+-----------------------------------------------------------+
| **Setting**          | **Default** | **Format** | **Description**                                           |
+----------------------+-------------+------------+-----------------------------------------------------------+
| temperature-decimals | 2           | number     | How many decimals the GUIs should display for temperature |
+----------------------+-------------+------------+-----------------------------------------------------------+
| humidity-decimals    | 2           | number     | How many decimals the GUIs should display for humidity    |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-temperature     | 1           | 1 or 0     | Don't display the temperature value                       |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-humidity        | 1           | 1 or 0     | Don't display the humidity value                          |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-battery         | 1           | 1 or 0     | Don't display the battery value                           |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-id              | 1           | 1 or 0     | Don't display the sensor id                               |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-channel         | 1           | 1 or 0     | Don't display the sensor channel                          |
+----------------------+-------------+------------+-----------------------------------------------------------+

.. rubric:: Protocol

The transmitter sends 72 pulses, each 2 pulses making up a bit, so a message has 36 bits.
The first pulse is 500us. The second pulse can be 1000us ('zero' bit), 2000us ('one' bit) or 4000us (sync bit). The sync bit separates identical messages, of which there can be 11 of them repeated in a transmission.

Message structure from https://forum.pilight.org/showthread.php?tid=3351&pid=22798#pid22798 :

.. code-block:: console

	ID ID ID CC         8-bit ID, the two least significant might encode the channel
	BF CC               4 bits of flags:
							B  =1 battery good
							F  =1 forced transmission
							CC =  channel, zero based
	TT TT TT TT TT TT   12 bits signed integer, temp in Celsius/10
	11 11               const / reserved
	HH HH HH HH         8 bits, either
							- humidity (or 1111 xxxx if not available); or
							- a CRC, e.g. Rubicson, algorithm in source code linked above

See also:

- https://github.com/aquaticus/nexus433
- https://github.com/merbanan/rtl_433/blob/master/src/devices/nexus.c
