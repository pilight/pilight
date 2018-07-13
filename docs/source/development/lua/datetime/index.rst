Datetime
========

Various usefull functions for date and time manipulations

.. c:function:: table pilight.datetime.strptime(string datetime, string format)

   Parse a time/date string based on a given format. Refer to the `date_format <https://manual.pilight.org/eventing/functions/date_format.html>`_ page for the formatting characters supported.

   .. code-block:: lua

      local array = pilight.datetime.strptime("6 Dec 2001 12:33:45", "%H:%M:%S %d %b %Y");
      print(array['year']); -- 2001
      print(array['month']); -- 12
      print(array['day']); -- 6
      print(array['weekday']); -- 4
      print(array['hour']); -- 12
      print(array['min']); -- 33
      print(array['sec']); -- 45

      array = pilight.datetime.strptime("2016-10-16 21:04:36", "%Y-%m-%d %H:%M:%S");
      print(array['year']); -- 2016
      print(array['month']); -- 10
      print(array['day']); -- 16
      print(array['weekday']); -- 0
      print(array['hour']); -- 21
      print(array['min']); -- 4
      print(array['sec']); -- 36
