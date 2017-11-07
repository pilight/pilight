.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

Dim
===

.. rubric:: Description

Dims a device to a dimlevel or starts a progression from one dimlevel to another.

.. rubric:: Options

+----------+------------------+---------------------+-------------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                       |
+----------+------------------+---------------------+-------------------------------------------------------+
| DEVICE   | |yes|            | |yes|               | Device(s) to change                                   |
+----------+------------------+---------------------+-------------------------------------------------------+
| TO       | |yes|            | |no|                | New dimlevel                                          |
+----------+------------------+---------------------+-------------------------------------------------------+

.. versionadded:: 7.0

+----------+------------------+---------------------+-------------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                       |
+----------+------------------+---------------------+-------------------------------------------------------+
| FOR      | |no|             | |no|                | Determine how long this new state lasts before        |
|          |                  |                     | changing back to the previous dimlevel and state      |
+----------+------------------+---------------------+-------------------------------------------------------+
| AFTER    | |no|             | |no|                | Time to wait before setting the new state             |
+----------+------------------+---------------------+-------------------------------------------------------+
| IN       | |no|             | |no|                | To be combined with FROM. Duration of the dimming     |
|          |                  |                     | process to change the dimlevel FROM x TO y            |
+----------+------------------+---------------------+-------------------------------------------------------+
| FROM     | |no|             | |no|                | To be combined with IN. The dimlevel the progression  |
|          |                  |                     | to the target dimlevel should start at                |
+----------+------------------+---------------------+-------------------------------------------------------+

.. note:: Units for ``FOR``, ``AFTER``, and ``IN``

   - MILLISECOND
   - SECOND
   - MINUTE
   - HOUR
   - DAY

.. rubric:: Examples

.. code-block:: console

   IF 1 == 1 THEN dim DEVICE mainlight TO 15
   IF 1 == 1 THEN dim DEVICE mainlight AND bookshelf AND hall TO 10

.. versionadded:: 7.0

.. code-block:: console

   IF 1 == 1 THEN dim DEVICE mainlight TO 15 FOR 10 MINUTE
   IF 1 == 1 THEN dim DEVICE mainlight TO 15 AFTER 30 SECOND
   IF 1 == 1 THEN dim DEVICE mainlight TO 15 AFTER 30 SECOND FOR 10 MINUTE
   IF 1 == 1 THEN dim DEVICE mainlight TO 15 FROM 0 IN 45 MINUTE
   IF 1 == 1 THEN dim DEVICE mainlight TO 15 FROM 0 IN 45 MINUTE FOR 15 MINUTE AFTER 5 MINUTE
   IF 1 == 1 THEN dim DEVICE mainlight TO 15 FOR 10 SECOND AFTER 30 MINUTE