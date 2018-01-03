.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

+------------------+-------------+
| **Feature**      | **Support** |
+------------------+-------------+
| Sending          | |yes|       |
+------------------+-------------+
| Receiving        | |yes|       |
+------------------+-------------+
| Config           | |yes|       |
+------------------+-------------+

.. rubric:: Supported Devices

+----------------------+----------------------+
| **Brand**            | **Protocol**         |
+----------------------+----------------------+
| REV                  | Switch 8462          |
|                      |                      |
|                      | Remote 8471          |
+----------------------+----------------------+

.. rubric:: Sender Arguments

.. code-block:: console
   :linenos:

   -i --id=id             control a device with this id/code
   -u --unit=unit         control a device with this unit/button
   -a --all=1/2           control all devices with unitcode 0-3 (when -a 1) or unitcode 4-7 (when -a 2)
   -t --on                send an on signal
   -f --off               send an off signal

   provide either --unit or --all

.. Note:: usage of the all-parameter

 - when using the --all parameter, the --unit parameter is ignored.
 - the all-parameter needs a value, either 1 or 2
 - when passing 1, all switches with unit 0 or 1 or 2 or 3 are switched to the provided state
 - when passing 2, all switches with unit 4 or 5 or 6 or 7 are switched to the provided state


.. rubric:: Config Sample 1

.. code-block:: json
   :linenos:

   {
     "devices": {
       "radio": {
         "protocol": [ "rev5_switch" ],
         "id": [{
           "id": "A",
           "unit": 1
         }],
         "state": "off"
       }
     },
     "gui": {
       "radio": {
         "name": "Kitchen radio",
         "group": [ "Kitchen" ],
         "media": [ "all" ]
       }
     }
   }

.. rubric:: Config Sample 2

.. code-block:: json
   :linenos:

   {
     "devices": {
       "radio": {
         "protocol": [ "rev5_switch" ],
         "id": [{
           "id": "A",
           "unit": 1
         }],
         "all": 1 /*when all is specified, the unit value is ignored */
         "state": "off"
       }
     },
     "gui": {
       "radio": {
         "name": "Kitchen radio",
         "group": [ "Kitchen" ],
         "media": [ "all" ]
       }
     }
   }

+------------------+-------------------+
| **Option**       | **Value**         |
+------------------+-------------------+
| id               | A - P             |
|                  |                   |
|                  | 0n - 15n          |
+------------------+-------------------+
| unit             | 0 - 7             |
+------------------+-------------------+
| all              | 1 - 2             |
+------------------+-------------------+
| state            | on / off          |
+------------------+-------------------+

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

.. Note:: protocol features are limited

 The protocol was reverse engineered using a remote that supports only 8 devices.
 The switch supports up to 16 devices, so the unused bits in the protocol might be used to control more devices.
 Because of lack of testing devices, the protocol just supports 8 devices per channel/id.


This protocol sends 50 pulses like this

.. code-block:: console

   1299 432 1299 432 1299 432 1299 432 1299 432 1299 432 432 1299 432 1299 1299 432 1299 432 1299 432 1299 432 432 1299 432 1299 1299 432 1299 432 432 1299 432 1299 432 1299 432 1299 432 1299 432 1299 432 1299 432 1299 432 15029 

It has no ``header`` and the last 2 pulses are the ``footer``. These are meant to identify the pulses as genuine The next step is to transform this output into 12 groups of 4 pulses (and thereby dropping the ``footer`` pulses).

.. code-block:: console

    1299   432	1299   432
    1299   432	1299   432
    1299   432	1299   432
     432  1299   432  1299
    1299   432  1299   432
    1299   432  1299   432
     432  1299   432  1299
    1299   432  1299   432
     432  1299   432  1299
     432  1299   432  1299
     432  1299   432  1299
     432  1299   432  1299
     432  15029

If we now look carefully at these groups you can distinguish two types of groups:

- ``432	1299 432 1299``
- ``1299 432 1299 432``

So the first group is defined by a high 4th pulse and the second group has a low 4th pulse. In this case we say a high 4th pulse means a 1 and a low 4th pulse means a 0. We then get the following output:

.. code-block:: console

   0001 0010 11 11 

Each (group) of numbers has a specific meaning:

- Id: 0 till 3
- Unit: 4 till 7
- Operation: 8 till 9
- unused: 10-11  (always 1)

- The ``Id`` is defined as a binary number
- The ``Unit`` specifies which switch is addressed

  - Bit 4 and Bit 7 define the unit as a binary number (4=MSB and 7=LSB)
  - Bit 5 is always zero
  -  Bit 6 is 0 when all units 0-3 are addressed and 1 when all units 4-7 are addressed

- The ``Operation`` is a binary number that specifies the operation the switch should perform (8=MSB and 9=LSB)

  - 0 => turn all switches in the current group ON   (check state of BIT 6)
  - 3 => turn all switches in the current group OFF  (check state of BIT 6)
  - 1 => turn the switch OFF
  - 2 => turn the switch ON

So this code represents:

.. code-block:: console

   0001 0010 11 11 

- Id: B
- All: 2   (switches with unit 4 till 7)
- State: Off

Another example:

.. code-block:: console

   0000 1000 10 11

- Id: A
- Unit: 1
- State: On

For the id parameter, you can use either a letter between A-P or a number between 0-15.
When using the number as a parameter, a letter (e.g 'n') must be appended.
The mapping between letter and number is:

+----+-----+----+-----+
| A  | 0n  | I  | 2n  |
+----+-----+----+-----+
| B  | 1n  | J  | 3n  |
+----+-----+----+-----+
| C  | 4n  | K  | 6n  |
+----+-----+----+-----+
| D  | 5n  | L  | 7n  |
+----+-----+----+-----+
| E  | 12n | M  | 14n |
+----+-----+----+-----+
| F  | 13n | N  | 15n |
+----+-----+----+-----+
| G  | 8n  | O  | 10n |
+----+-----+----+-----+
| H  | 9n  | P  | 11n |
+----+-----+----+-----+