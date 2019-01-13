.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

Mqtt_switch
===========

Behaves like a normal switch within pilight, but instead of sending and receiving via rf, it publishes an mqtt message when its state changes and changes its state if an ``on`` or ``off`` message is received via mqtt.

+------------------+-------------+
| **Feature**      | **Support** |
+------------------+-------------+
| Sending          | |no|        |
+------------------+-------------+
| Receiving        | |no|        |
+------------------+-------------+
| Config           | |yes|       |
+------------------+-------------+

.. rubric:: Supported Brands

*None*

.. rubric:: Sender Arguments

.. code-block:: console

   -t --on                  send an on signal
   -f --off                 send an off signal
   -i --id=id               control a device with this id

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "mqtt1": {
         "protocol": [ "mqtt_switch" ],
         "id": [{
           "id": 1
         }],
         "state": "off"
         "pub_host": "localhost",
         "pub_port": 1883
         "pub_user": "username",
         "pub_pass": "password",
         "pub_topic": "pilight/device/send/mqtt1",
         "sub_host": "localhost",
         "sub_port": 1883
         "sub_user": "username",
         "sub_pass": "password",
         "sub_topic": "pilight/device/receive/mqtt1",
         "qos": "1"
      }
     },
     "gui": {
       "switch": {
         "name": "Mqtt Switch1",
         "group": [ "Living" ],
         "media": [ "all" ]
       }
     }
   }


+------------------+----------------------------+
| **Option**       | **Value**                  |
+------------------+----------------------------+
| id               | 0 - 99999                  |
+------------------+----------------------------+
| state            | on / off                   |
+------------------+----------------------------+
| pub_topic        | valid topic for publish    |
+------------------+----------------------------+
| sub_topic        | valid topic for subscribe  |
+------------------+----------------------------+
| qos              | 0 or 1                     |
+------------------+----------------------------+

.. note:: Only qos 0 and qos 1 are currently supported

.. rubric:: Optional Settings

:underline:`Device Settings`

+------------------+--------------------------------+
| **Option**       | **Value**                      |
+------------------+--------------------------------+
| pub_host         | valid hostname for publish     |
+------------------+--------------------------------+
| pub_port         | port number for publish        |
+------------------+--------------------------------+
| pub_user         | user name for publish          |
+------------------+--------------------------------+
| pub_pass         | password for publish           |
+------------------+--------------------------------+
| sub_host         | valid hostname for subscribe   |
+------------------+--------------------------------+
| sub_port         | port number for subscribe      |
+------------------+--------------------------------+
| sub_user         | user name for subscribe        |
+------------------+--------------------------------+
| sub_pass         | password for subscribe         |
+------------------+--------------------------------+

.. note:: In most cases, just one broker will be used and, if required by the broker, with just one username/password. In those cases the optional settings can be omitted from the mqtt_switch device config and the corresponding settings of the hardware module will be used.

:underline:`GUI Settings`

+------------------+-------------+------------+-----------------------------------------------+
| **Setting**      | **Default** | **Format** | **Description**                               |
+------------------+-------------+------------+-----------------------------------------------+
| readonly         | 0           | 1 or 0     | Disable controlling this device from the GUIs |
+------------------+-------------+------------+-----------------------------------------------+
| confirm          | 0           | 1 or 0     | Ask for confirmation when switching device    |
+------------------+-------------+------------+-----------------------------------------------+
