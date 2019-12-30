.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

Shelly Plug S
=============

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

Shelly

.. rubric:: Sender Arguments

.. code-block:: console
   :linenos:

   -i --id=id             control a device with this id
   -t --on                send an on signal
   -f --off               send an off signal

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "switch": {
         "protocol": [ "shellyplug-s" ],
         "id": [{
           "id": "A01BC2",
         }],
         "state": "off",
         "energy": 1234,
         "power": 1000,
         "overtemperature": 0,
         "temperature": 23.00,
         "connected": 0
       }
     },
     "gui": {
       "switch": {
         "name": "Switch",
         "group": [ "Living" ],
         "media": [ "all" ]
       }
     }
   }

+------------------+-----------------+
| **Option**       | **Value**       |
+------------------+-----------------+
| id               | as hardcoded    |
+------------------+-----------------+
| state            | on / off        |
+------------------+-----------------+
| energy           | 0 - 999999999   |
+------------------+-----------------+
| power            | 0 - 3500        |
+------------------+-----------------+
| temperature      | -99 - 99        |
+------------------+-----------------+
| overtemperature  | 0 or 1          |
+------------------+-----------------+
| connected        | 0 or 1          |
+------------------+-----------------+

.. rubric:: Optional Settings

:underline:`GUI Settings`

+----------------------+-------------+------------+-----------------------------------------------------------+
| **Setting**          | **Default** | **Format** | **Description**                                           |
+----------------------+-------------+------------+-----------------------------------------------------------+
| readonly             | 1           | 1 or 0     | Disable controlling this device from the GUIs             |
+----------------------+-------------+------------+-----------------------------------------------------------+