Hardware
========

- `Disabled`_
- `433.92Mhz`_
   - `433gpio`_
   - `433nano`_
- `API`_
   - `shelly`_
   - `tasmota`_
   - `mqtt`_

.. deprecated:: 8.1.5

- `433lirc`_

Introduction
------------

.. warning::

   Make sure pilight is not running before editing your configuration or else all changes will be lost.

Since pilight 3.0 the feature to send and receive from multiple hardware modules has been introduced. This means you can control various different frequencies at the same time. However, only one module per frequency is supported. Each hardware module has its own syntax as listed below.

Disabled
--------

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "none": { }
     }
   }

433.92Mhz
---------

.. _433gpio:
.. rubric:: Direct GPIO Access

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "433gpio": {
         "sender": 0,
         "receiver": 1
       }
     }
   }

The default configuration to be used with the pilight PCB. When using custom wiring, refer to http://www.wiringx.org for the pin numbering of the various supported devices. If you want to disable the sender or receiver pin, set it to
-1.

.. deprecated:: 8.1.5

.. _433lirc:
.. rubric:: Lirc Kernel Module

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "433lirc": {
         "socket": "/dev/lirc0"
       }
     }
   }

.. versionchanged:: 8.0

You must now specify which GPIO platform pilight is running on. Refer to the settings page for more information.

.. _433nano:
.. rubric:: pilight USB Nano

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "433nano": {
         "comport": "/dev/ttyUSB0"
       }
     }
   }

The comport value needs to correspond to a valid COM device on your platform. On Windows this value is generally formatted as COM1, on Linux as /dev/ttyUSB0, and on FreeBSD /dev/cuau0.

API
---

.. versionadded:: nightly

You must now specify which GPIO platform pilight is running on. Refer to the settings page for more information.

.. _shelly:
.. rubric:: Shelly cloud devices

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "shelly": {
       }
     }
   }

For using the Shelly hardware module, the MQTT functionality has to be enabled in both pilight and your Shelly devices. Make sure the ip points to your pilight instance, and the port to the configured pilight MQTT port. Make sure the rest of the options are as they are by default.

.. _tasmota:
.. rubric:: Tasmota devices

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "tasmota": {
       }
     }
   }

For using the Tasmota hardware module, the MQTT functionality has to be enabled in both pilight and your Tasmota devices. Make sure the ip points to your pilight instance, and the port to the configured pilight MQTT port. Make sure the rest of the options are as they are by default.

.. _mqtt:
.. rubric:: MQTT devices

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "mqtt": {
       }
     }
   }

The MQTT hardware module just broadcasts all MQTT messages that are received by pilight.
