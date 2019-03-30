.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

Label
=====

.. rubric:: Description

Changes the text and color of a generic label device.

.. versionadded:: nightly 

Optionally change the background color and blinking.

.. rubric:: Options

+----------+------------------+---------------------+---------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                   |
+----------+------------------+---------------------+---------------------------------------------------+
| DEVICE   | |yes|            | |yes|               | Device(s) to change                               |
+----------+------------------+---------------------+---------------------------------------------------+
| TO       | |yes|            | |no|                | New text                                          |
+----------+------------------+---------------------+---------------------------------------------------+
| COLOR    | |no|             | |no|                | New color. If omitted, color is set to black      |
+----------+------------------+---------------------+---------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                   |
+----------+------------------+---------------------+---------------------------------------------------+
| FOR      | |no|             | |no|                | | Determine how long this new label lasts         |
|          |                  |                     | | before we change back to the previous label     |
+----------+------------------+---------------------+---------------------------------------------------+
| AFTER    | |no|             | |no|                | After how long do we want the new label to be set |
+----------+------------------+---------------------+---------------------------------------------------+

.. note:: Units for ``FOR`` and ``AFTER``

   - MILLISECOND
   - SECOND
   - MINUTE
   - HOUR
   - DAY

.. versionchanged:: nightly

+----------+------------------+---------------------+---------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                   |
+----------+------------------+---------------------+---------------------------------------------------+
| TO       | |no|             | |no|                | New text. See note.                               |
+----------+------------------+---------------------+---------------------------------------------------+


.. versionadded:: nightly

+----------+------------------+---------------------+---------------------------------------------------+
| **Name** | **Required**     | **Multiple Values** | **Description**                                   |
+----------+------------------+---------------------+---------------------------------------------------+
| BGCOLOR  | |no|             | |no|                | New background color. See note.                   |
+----------+------------------+---------------------+---------------------------------------------------+
| BLINK    | |no|             | |no|                | Blinking label text ``on`` or ``off``. See note   |
+----------+------------------+---------------------+---------------------------------------------------+

.. note:: for ``TO``, ``COLOR``, ``BGCOLOR`` and ``BLINK``

   At least one of these options must be given.
   If one or more of these options are omitted, their current value(s) will be preserved.

.. rubric:: Examples

.. code-block:: console

   IF 1 == 1 THEN label DEVICE tempLabel TO on
   IF 1 == 1 THEN label DEVICE tempLabel AND humiLabel TO No information
   IF 1 == 1 THEN label DEVICE tempLabel TO 23.5 FOR 10 SECOND
   IF 1 == 1 THEN label DEVICE tempLabel TO Bell rang AFTER 30 SECOND
   IF 1 == 1 THEN label DEVICE tempLabel TO None FOR 10 MINUTE AFTER 30 SECOND

.. versionchanged:: 8.1.0.

.. code-block:: console

   IF 1 == 1 THEN label DEVICE tempLabel TO on
   IF 1 == 1 THEN label DEVICE tempLabel AND humiLabel TO 'No information'
   IF 1 == 1 THEN label DEVICE tempLabel TO 23.5 FOR '10 SECOND'
   IF 1 == 1 THEN label DEVICE tempLabel TO 'Bell rang' AFTER '30 SECOND'
   IF 1 == 1 THEN label DEVICE tempLabel TO None FOR '10 MINUTE' AFTER '30 SECOND'

.. versionadded:: nightly

.. code-block:: console

   IF 1 == 1 THEN label DEVICE tempLabel TO 'Door was opened' BLINK on FOR 1 MINUTE
   IF 1 == 1 THEN label DEVICE tempLabel TO 'Door was closed' COLOR white BGCOLOR green BLINK off
   IF 1 == 1 THEN label DEVICE tempLabel BLINK on
