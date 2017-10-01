.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

Logilink (EC0005)
====================

+------------------+-------------+
| **Feature**      | **Support** |
+------------------+-------------+
| Sending          | |yes|       |
+------------------+-------------+
| Receiving        | |yes|       |
+------------------+-------------+
| Config           | |yes|       |
+------------------+-------------+

.. rubric:: Supported Brands

+----------------------+------------------+
| **Brand**            | **Protocol**     |
+----------------------+------------------+
| Logilink             | logilink_switch2 |
+----------------------+------------------+

.. rubric:: Sender Arguments

.. code-block:: console
   :linenos:

   -s --systemcode=systemcode     control a device with this systemcode
   -u --unitcode=unitcode         control a device with this unitcode
   -t --on                        send an on signal
   -f --off                       send an off signal

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "dimmer": {
         "protocol": [ "logilink_switch2" ],
         "id": [{
           "systemcode": 772700,
           "unitcode": 0
         }],
         "state": "off"
       }
     },
     "gui": {
       "Lamp": {
         "name": "TV Backlight",
         "group": [ "Living" ],
         "media": [ "all" ]
       }
     }
   }

+------------------+-----------------+
| **Option**       | **Value**       |
+------------------+-----------------+
| systemcode       | 0 - 2097151     |
+------------------+-----------------+
| unitcode         | 0 - 7           |
+------------------+-----------------+
| state            | on / off        |
+------------------+-----------------+

.. rubric:: Optional Settings

:underline:`GUI Settings`

+----------------------+-------------+------------+-----------------------------------------------------------+
| **Setting**          | **Default** | **Format** | **Description**                                           |
+----------------------+-------------+------------+-----------------------------------------------------------+
| readonly             | 1           | 1 or 0     | Disable controlling this device from the GUIs             |
+----------------------+-------------+------------+-----------------------------------------------------------+
| confirm              | 1           | 1 or 0     | Ask for confirmation when switching device                |
+----------------------+-------------+------------+-----------------------------------------------------------+

.. rubric:: Protocol

This protocol sends 50 pulses like this

.. code-block:: console

   284 852 852 284 852 284 284 852 284 852 852 284 284 852 284 852 852 284 852 284 284 852 284 852 284 852 852 284 852 284 284 852 284 852 852 284 852 284 284 852 852 284 284 852 284 852 284 852 284 9656

It has no ``header`` and the last 2 pulses are the ``footer``. These are meant to identify the pulses as genuine, and the protocol also has some bit checks to filter false positives. We don't use them for further processing. The next step is to transform this output into 12 groups of 4 pulses (and thereby dropping the ``footer`` pulses).

.. code-block:: console

   284 852
   852 284 
   852 284 
   284 852 
   284 852 
   852 284 
   284 852 
   284 852 
   852 284 
   852 284 
   284 852 
   284 852 
   284 852 
   852 284 
   852 284 
   284 852 
   284 852 
   852 284 
   852 284 
   284 852 
   284 852 
   284 852 
   284 852 
   852 284 


If we now look at carefully at these groups you can distinguish two types of groups:

- ``284 852``
- ``852 284``

So the first group is defined by a high 4th pulse and the second group has a low 4th pulse. In this case we say a high 4th pulse means a 0 and a low 4th pulse means a 1. We then get the following output:

.. code-block::console

   01100100110001100110 000 1

Each (group) of numbers has a specific meaning:

- SytemCode: 0 till 19
- UnitCode: 20 till 22
- State: 23 (inverse state)

.. code-block: console

   01100100110001100110 000 1

- The ``SystemCode`` is defined as a binary number
- The ``UnitCode`` is defined as a binary number and represents the button pressed, e.g. A, B or ALL
- The ``State`` defines whether a switch state is Opened or Closed

So this code represents:

- SystemCode: 31 (inversed)
- UnitCode: 1 (inversed)
- State: On (inverse state)
- Check: On

So this code represents:

- SystemCode: 412774  
- State: Opened
- Button: All 