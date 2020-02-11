Table
=====

.. versionadded:: 8.1.3

The pilight metatable can be used using the table object.
The main difference from the default lua tables is that
in pilight lua tables, the order in which keys are set
is the same as the order in which the keys are iterated.

.. versionadded:: nightly

Some pilight lua interfaces can be stored in pilight
metatables. Supported are the ``async.thread``, ``async.timer``,
``async.event``, and the ``network.mqtt`` interfaces.

pilight interfaces can only be stored as values assigned
to a key as shown in the example below. To restore a pilight
interface, pass the stored value to the interface constructor.

.. code-block:: lua

   local timer = pilight.async.timer();
   -- Create a new mqtt instance
   local mqtt = pilight.network.mqtt();
   local data = timer.getUserdata();
   -- Store the mqtt instance in the timer userdata table
   data['mqtt'] = mqtt();

   -- This doesn't work. The mqtt instance
   -- is not assigned to a key.
   -- timer.setUserdata(mqtt());

   -- later on

   -- Get the timer userdata table
   local data = timer.getUserdata();
   -- Restore the mqtt instance by passing the stored mqtt
   -- value to the mqtt constructor.
   local mqtt = pilight.network.mqtt(data['mqtt']);

.. deprecated:: 8.1.5

To get the length of a pilight lua table you need to use the special ``.__len()`` function.

.. code-block:: lua

   local table = pilight.table();
   table[0] = 'foo';
   print(table.__len());

.. versionchanged:: 8.1.5

.. note::

   The ``push``, ``pop``, ``shift``, and ``unshift`` functions only work on numeric tables.

.. c:function:: number len()

   Returns the number of values stored in the table

   .. code-block:: lua

      local table = pilight.table();
      table[0] = 'foo';
      print(table.len());

.. versionadded:: 8.1.5

.. c:function:: boolean push(string | number | boolean | table)

   Pushes a value at the end of the table

   .. code-block:: lua

      local table = pilight.table();
      table.push(1);

.. c:function:: boolean shift(string | number | boolean | table)

   Adds a value at the beginning of the table

   .. code-block:: lua

      local table = pilight.table();
      table.shift(1);

.. c:function:: userdata pop()

   Retrieves the value from the end of the table

   .. code-block:: lua

      local table = pilight.table();
      print(table.pop());

.. c:function:: userdata unshift()

   Retrieves the value from the beginning of the table

   .. code-block:: lua

      local table = pilight.table();
      print(table.unshift());
