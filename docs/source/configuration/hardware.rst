Hardware
========

- `Disabled`_
- `433.92Mhz`_
   - `433lirc`_
   - `433gpio`_
   - `433nano`_
- `MQTT`_
   - `mqtt`_
      - `mqtt-listener`_
   
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

.. versionadded:: nightly

MQTT
----

The hardware module is doing all the mqtt protocol stuff required to establish mqtt connections and subscriptions and publishing and receiving of mqtt messages.

When pilight starts, the module will automatically be initialized and then clients can subscribe for messages published by the broker and/or publish messages themselves.
When an attempt is made to subscribe or publish for the first time, the hardware module creates a socket, makes a network connection and does a CONNECT request and, for subscribe the mqtt module also does a SUBSCRIBE request. The client is added to the client pool and the client is informed about the status via a callback. The clientid is the unique key to the clients data in the client pool. Therefor the clientids must be unique.

Each client can individulally specify a keepalive time and the established connection and/or subscription will be kept alive at least for the given time. If the keepalive time is set to ``0`` (zero), the connection and/or subscription will be kept alive "forever" and will be used for new publish request and reception of messages.

If a connection cannot be established or dies for whatever reason, the affected client(s) will immediately be notified and then can try to subscribe or publish again.

Currently three mqtt client types are available for pilight:

- An mqtt_switch protocol that can be used to control external mqtt enabled devices and can handle feedback from those devices. Connection and or subscription are kept alive "forever".
- An mqtt_publish action that can be used to simply publish mqtt messages from rules. Connection is kept alive just long enough to be able to publish the message.
- An mqtt-listener which is embedded in the mqtt_sub module and can control several pilight devices. Connection and subscription are kept alive "forever".

.. _mqtt: 
.. rubric:: mqtt

.. code-block:: json
   :linenos:

   {
     "hardware": {
       "mqtt": {
         "host": "localhost",
         "port": 1883
         "user": "username",
         "pass": "password",
         "listener_topic": "pilight/control/#,pilight/command/#"
       }
     }
   }   

All settings are mandatory but ``host``, ``port``, ``user`` and ``pass`` can be overridden by the individual clients.
If the broker doesnt require authentication, the ``user`` and ``pass`` settings still must be present, but may be configured as empty strings.

.. _mqtt-listener:
.. rubric:: mqtt-listener

The mqtt module has a built in client (mqtt-listener) that can control several of pilight's devices, based on messages being received for the topic(s) given in the ``listener_topic`` setting of the mqtt module.

The listener_topic can be an empty string, just one topic, or comma separated list of topics. When listener_topic is an empty string the mqtt-listener will not be activated. With the configuration example shown above, the built in client will automatically be activated and will receive all messages published with topics starting with ``pilight/control`` or ``pilight/command``. The mqtt_listener will try to handle these messages if the last part of the topic is in the format ``.../action/device``, where ``action`` is one of those in the table below and ``device`` is the name of a device existing in the config. The message itself is handled as the new value or state for that device.

Eg. If a message ``on`` is received with topic ``pilight/control/switch/livingroom`` the mqtt-listener will try to switch device ``livingroom`` to ``on``.

+------------+------------------+---------------------+------------------------------------------------------------------------+
| **action** | **device type**  |  **valid message**  | **Description**                                                        |
+------------+------------------+---------------------+------------------------------------------------------------------------+
| switch     | switch or dimmer | ``on`` or ``off``   | switches the device on or off                                          |
+------------+------------------+---------------------+------------------------------------------------------------------------+
| screen     | screen           | ``up`` or ``down``  | switches the screen up or down                                         |
+------------+------------------+---------------------+------------------------------------------------------------------------+
| dim        | dimmer           | number, see below   | sets the device's dimlevel                                             |
+------------+------------------+---------------------+------------------------------------------------------------------------+
| label      | label            | see below           | sets the device's label, color and/or background color and/or blinking |
+------------+------------------+---------------------+------------------------------------------------------------------------+
| weather    | weather          | see below           | sets the device's temperature and/or humidity and/or battery state     |
+------------+------------------+---------------------+------------------------------------------------------------------------+

Actions not mentioned in the table above will be ignored.

.. note:: Valid message format for ``dim``

The mqtt-listener verifies that the message for dimmer is a just a positive number. It does NOT check if the number is within the min and max dimlevel range. The dimmer protocol itself will decline values being out of range.

.. note:: Valid message format for ``label``

The format of a valid message for label is ``text^color^bgcolor^blink``. All values are optional, but at least one must be given.
Eg ``This is the text^#000000^#FF0000^on`` will give a blinking label text 'This is the text' in white on a red background.

If an optional value is omitted, the separating caret must still be present, but trailing carets can be omitted.
Eg ``This is the text`` will just set the label text and leaves the visual attributes unchanged. ``^^^on`` will leave text, color and backgroud color unchanged and makes the label text blink.
Note that the label text must not contain any caret characters (^), otherwise the result of the action will be unpredictable.

Valid values for ``label``

+------------------+----------------------------+
| **Item**         | **Valid value**            |
+------------------+----------------------------+
| text             | any string or number       |
+------------------+----------------------------+
| color            | any color                  |
+------------------+----------------------------+
| bgcolor          | any color                  |
+------------------+----------------------------+
| blink            | ``on`` or ``off``          |
+------------------+----------------------------+

.. note:: Valid message format for ``weather``

The format of a valid message for weather is ``temperature^humidity^battery``. All values are optional, but at least one must be present.
Eg ``7.5^76^1`` will set the temperature to 7.5, the humidity to 76 and the battery state to normal.

If one or more of the values are omitted, the separating carets must still be present, but trailing carets can be omitted.
Eg ``7.5`` will just set the temperature and leaves the other values unchanged. ``^^0`` will leave temperature and humidity unchanged, and sets the battery state to low.

Valid values for ``weather``

+------------------+----------------------------+
| **Item**         | **Valid value**            |
+------------------+----------------------------+
| temperature      | any number                 |
+------------------+----------------------------+
| humidity         | 1 - 99                     |
+------------------+----------------------------+
| battery          | 0 or 1                     |
+------------------+----------------------------+
