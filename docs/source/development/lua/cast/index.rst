Cast
====

The Cast functions are used for pilight specific casts between variable types.

.. warning::

   The cast library was created to be backwards compatible with the event parser, but is a possible candidate to be made obsolete in the future.

.. c:function:: boolean pilight.cast.toboolean(number | string variable)

   Casts a string or number to boolean

	 .. note:: Compared to default Lua

      Casting to boolean is not supported in lua

.. c:function:: number pilight.cast.tonumber(number | boolean variable)

   Casts a boolean or number to boolean

   .. code-block:: lua

      tonumber('') == nil; pilight.cast.tonumber('') == 0
      tonumber('false') == nil; pilight.cast.tonumber('false') == 0
      tonumber(false) == nil; pilight.cast.tonumber(false) == 0
      tonumber(true) == nil; pilight.cast.tonumber(true) == 1

.. c:function:: string pilight.cast.tostring(number | boolean variable)

   Casts a string or boolean to boolean

   .. code-block:: lua

      tostring(10) == '10'; pilight.cast.tostring(10) == '10.000000'
      tostring(10.0) == '10'; pilight.cast.tostring(10.0) == '10.000000'