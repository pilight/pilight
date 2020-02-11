MQTT API
========

.. note:: Work in progress

   The MQTT functionality is under development. Feature may be added and/or changed when in nightly status.

pilight communicates over several topics. The meta-data topics are retained so any client will receive those immediately after subscribing. These topics are:

   .. code-block:: console

      pilight/sys/port 5000
      pilight/sys/ip 192.168.1.2
      pilight/sys/version 8.1.5

      pilight/sys/cpu 0.347882

The cpu topic is communicated every three seconds.

pilight device updates are communicated using the following topics:

   .. code-block:: console

      pilight/device/+
      pilight/device/+/+

The first wildcard contains the device name as configured in the pilight config. The second wildcard contains the value that was updated. The toplevel wildcard communicated a composition of these values in a json format. So, in case of the datetime device, the following information is communicated:

   .. code-block:: console

      pilight/device/time {"timestamp":1575720044,"year":2019,"month":12,"day":7,"hour":13,"minute":0,"second":44,"weekday":7,"dst":0}
      pilight/device/time/timestamp 1575720044
      pilight/device/time/year 2019
      pilight/device/time/month 12
      pilight/device/time/day 7
      pilight/device/time/hour 13
      pilight/device/time/minute 0
      pilight/device/time/second 44
      pilight/device/time/weekday 7
      pilight/device/time/dst 0

To control a pilight device, the same topic structure can be used. A specific device value can be set using the value topics:

   .. code-block:: console

      pilight/device/dimmer/dimlevel 10

To control multiple values at once, the json format can be used:

   .. code-block:: console

      pilight/device/dimmer {"dimlevel":10,"state":"on"}

The pilight mqtt broker communicated by using two topics:

   .. code-block:: console

      pilight/mqtt/+

The wildcard of this topic is the client id. The payload can either be ``connected`` or ``disconnected`` and either one will be send as the payload.