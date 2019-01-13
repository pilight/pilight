.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

Mqtt_publish
============

.. rubric:: Description

Publishes an mqtt message.

.. rubric:: Options

+----------+------------------+---------------------+------------------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                            |
+----------+------------------+---------------------+------------------------------------------------------------+
| HOST     | |no|             | |no|                | Broker's hostname. If omitted, hardware setting is used    |
+----------+------------------+---------------------+------------------------------------------------------------+
| PORT     | |no|             | |no|                | Broker's port number. If omitted, hardware setting is used |
+----------+------------------+---------------------+------------------------------------------------------------+
| USER     | |no|             | |no|                | Username. If omitted, hardware setting is used             |
+----------+------------------+---------------------+------------------------------------------------------------+
| PASSWORD | |no|             | |no|                | Password. If omitted, hardware setting is used             |
+----------+------------------+---------------------+------------------------------------------------------------+
| TOPIC    | |yes|            | |no|                | Topic to publish for                                       |
+----------+------------------+---------------------+------------------------------------------------------------+
| MESSAGE  | |yes|            | |no|                | Message to be published                                    |
+----------+------------------+---------------------+------------------------------------------------------------+
| QOS      | |no|             | |no|                | 0 or 1. If omitted, QOS0 is used as default                |
+----------+------------------+---------------------+------------------------------------------------------------+

.. note:: Valid values for ``QOS``

   Only QOS0 and QOS1 are supported
.. rubric:: Examples

.. code-block:: console

   IF 1 == 1 THEN mqtt_publish HOST localhost PORT 1883 USER username PASSWORD password TOPIC pilight/topic1 MESSAGE 'message text' QOS 1 -->all settings specified
   IF 1 == 1 THEN mqtt_publish TOPIC pilight/topic1 MESSAGE 'message text' --> using defaults for all optional settings
