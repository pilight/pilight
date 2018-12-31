.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

min
===

.. rubric:: Description

Returns the smaller of the two numbers given

.. rubric:: Parameters

+----------+------------------+---------------------+
| **#**    | **Required**     | **Description**     |
+----------+------------------+---------------------+
| 1        | |yes|            | First number        |
+----------+------------------+---------------------+
| 2        | |yes|            | Second number       |
+----------+------------------+---------------------+

.. rubric:: Return value

The smaller number

.. rubric:: Examples

.. code-block:: console

   IF MIN(-1, 0) == -1 THEN ...
   IF MIN(10, 11) == 10 THEN ...
   IF MIN(0, dimmer.dimlevel - 1) == -1 THEN ...