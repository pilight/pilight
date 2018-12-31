.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

max
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

   IF MAX(-1, 0) == 0 THEN ...
   IF MAX(10, 11) == 11 THEN ...
   IF MAX(0, dimmer.dimlevel - 1) == 0 THEN ...