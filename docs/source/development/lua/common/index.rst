Common
======

Various usefull functions

.. c:function:: number pilight.common.random(number min, number max)

   Returns a random number between min and max.

.. c:function:: number pilight.common.explode(string source, string delimiter)

   Returns an table of strings, each of which is a substring of string formed by splitting it on boundaries formed by the string delimiter.

   .. code-block:: lua

      local array = pilight.common.explode("foo bar", " ");
      print(array[1]); -- foo
      print(array[2]); -- bar

      array = pilight.common.explode("127.0.0.1", ".");
      print(array[1]); -- 127
      print(array[2]); -- 0
      print(array[3]); -- 0
      print(array[4]); -- 1