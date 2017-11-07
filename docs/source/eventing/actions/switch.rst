.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

Switch
======

.. rubric:: Description

Changes the state of a switch, relay or dimmer.

.. rubric:: Options

+----------+------------------+---------------------+---------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                   |
+----------+------------------+---------------------+---------------------------------------------------+
| DEVICE   | |yes|            | |yes|               | Device(s) to change                               |
+----------+------------------+---------------------+---------------------------------------------------+
| TO       | |yes|            | |no|                | New state                                         |
+----------+------------------+---------------------+---------------------------------------------------+

.. versionadded:: 7.0

+----------+------------------+---------------------+---------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                   |
+----------+------------------+---------------------+---------------------------------------------------+
| FOR      | |no|             | |no|                | Determine how long this new state lasts before    |
|          |                  |                     | changing back to the previous state               |
+----------+------------------+---------------------+---------------------------------------------------+
| AFTER    | |no|             | |no|                | Time to wait before setting the new state         |
+----------+------------------+---------------------+---------------------------------------------------+

.. note:: Units for ``FOR`` and ``AFTER``

   - MILLISECOND
   - SECOND
   - MINUTE
   - HOUR
   - DAY

.. rubric:: Examples

.. code-block:: console

   IF 1 == 1 THEN switch DEVICE light TO on
   IF 1 == 1 THEN switch DEVICE mainlight AND bookshelf AND hall TO on

.. versionadded:: 7.0

.. code-block:: console

   IF 1 == 1 THEN switch DEVICE light TO on FOR 10 MINUTE
   IF 1 == 1 THEN switch DEVICE light TO on AFTER 30 MILLISECOND
   IF 1 == 1 THEN switch DEVICE light TO off FOR 10 SECOND AFTER 30 MINUTE