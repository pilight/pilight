Async
=====

The Async functions are used to asynchronously run code.

- `Timer`_
- `Thread`_

.. versionadded:: 8.1.5

- `Event`_

Timer
-----

A timer is used to schedule (repeated) callbacks to be called in the future.

API
^^^

.. c:function:: userdata pilight.async.timer()

   Creates a new timer object

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the timer object.

.. c:function:: boolean setCallback(string callback)

   The name of the callback being triggered by the timer. The timer object will be passed as the only parameter of this callback function.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the timer object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. c:function:: boolean setRepeat(numeric millisecond)

   If repeat is non-zero, the callback fires first after the milliseconds set and then repeatedly after repeat milliseconds.

.. c:function:: boolean setTimeout(numeric millisecond)

   If timeout is zero, the timer is fired as soon as possible. If the timer is non-zero, the callback will be fired after the milliseconds set.

.. c:function:: boolean start()

   Start the timer

.. c:function:: boolean stop()

   Stops the timer

Example
^^^^^^^

.. code-block:: lua

   local M = {};

   function M.timer(timer)
     local data = timer.getUserdata();
     print(data['msg']);

     timer.stop();
   end

   function M.run()
     local timer = pilight.async.timer();
     local data = timer.getUserdata();

     data['status'] = "Hello World!";

     timer.setCallback("timer");
     timer.setTimeout(1000);
     timer.setRepeat(1000);
     timer.start();

     return 1;
   end

   return M;

Thread
------

A thread can be used to trigger a callbacks concurrently.

API
^^^

.. c:function:: userdata pilight.async.thread()

   Creates a new thread object

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the thread object.

.. c:function:: boolean setCallback(string callback)

   The name of the callback being triggered by the thread. The thread object will be passed as the only parameter of this callback function.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the thread object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. c:function:: boolean trigger()

   Trigger the thread callback

Example
^^^^^^^

.. code-block:: lua

   local M = {};

   function M.thread(thread)
     local data = thread.getUserdata();

     print(data['status']);
   end

   function M.run()
     local thread = pilight.async.thread();
     local data = thread.getUserdata();

     thread.setCallback("thread");

     data['status'] = "Hello World!";

     thread.trigger();

     return 1;
   end

   return M;

Event
-----

.. versionadded:: 8.1.5

The event library implements an async consumer listener pattern

API
^^^

.. c:function:: userdata pilight.async.event()

   Creates a new event object

.. c:function:: userdata register(int callback)

   Register the async object to a specific event

.. c:function:: userdata unregister(int callback)

   Unregister the async object from a specific event

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the thread object.

.. c:function:: boolean setCallback(string callback)

   The name of the callback being trigger when the event occured

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the thread object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. c:function:: boolean trigger(userdata table)

   Trigger an event with data from lua

.. c:function:: boolean gc()

   Garbage collect the event object when no callback is set

Example listening
^^^^^^^^^^^^^^^^^

.. code-block:: lua

   local M = {};

   function M.send(event, reason, data)
     --
     -- Check, double check
     --
     if reason ~= pilight.reason.SEND_CODE then
        return;
     end;

     print(data['pulses']); -- The SEND_CODE metatable contains specific keys like 'pulses'
   end

   function M.run()
     local event = pilight.async.event();
     event.register(pilight.reason.SEND_CODE);
     event.setCallback("event");

     return 1;
   end

   return M;

Example triggering
^^^^^^^^^^^^^^^^^^

.. code-block:: lua

   local M = {};

   function M.run()
     local event = pilight.async.event();
     event.register(pilight.reason.RECEIVED_PULSETRAIN);

     local data = {};
     data['length'] = 0;

     event.trigger(data)
     event.gc();

     return 1;
   end

   return M;