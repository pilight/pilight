Table
=====

.. versionadded:: 8.1.3

The pilight metatable can be used using the table object.
The main difference from the default lua tables is that
in pilight lua tables, the order in which keys are set
is the same as the order in which the keys are iterated.

To get the length of a pilight lua table you need to use the special ``.__len()`` function.

.. code-block:: lua

   local table = pilight.table();
   table[0] = 'foo';
   print(table.__len());