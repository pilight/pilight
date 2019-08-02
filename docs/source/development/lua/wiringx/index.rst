WiringX
=======

.. versionadded:: 8.1.5

.. c:function:: userdata wiringX.setup(string platform)

   Creates a new wiringx object

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the thread object.

.. c:function:: boolean ISR(int gpio, int mode, string callback[, int interval])

   Configures a certain GPIO to read interrupts with mode **wiringX.ISR_MODE_RISING**, **wiringX.ISR_MODE_FALLING**, or **wiringX.ISR_MODE_BOTH**. The callback will be trigger each 250 milliseconds. All pulses received in the meanwhile will be passed as an array to the callback function. When necessary, this interval can be changed with the interval parameter.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the thread object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. c:function:: boolean hasGPIO(int gpio)

   Tells pilight if the platform supports this specific GPIO

.. c:function:: boolean pinMode(int mode)

   Set the pinMode of a certain GPIO to **wiringX.PINMODE_OUTPUT** or **wiringX.PINMODE_INPUT**

	 wx.digitalWrite(sender, 1, data['pulses']());

.. c:function:: boolean digitalWrite(int gpio, int mode[, metatable pulses])

   Sets the GPIO to a certain mode. Either low (0) or high (1).

   The pulses parameter is optional. Due to the lag of the lua interface, digitalWrite can sends pulses buffered. Therefor, when the GPIO needs to be toggled superfast, this lag is not an option. As an alternative you can pass an array with milliseconds (counted from 1). The GPIO will be toggled between high and low delayed by these milliseconds. The initial mode of the toggling will be the mode passed as the second parameter.

Example triggering
^^^^^^^^^^^^^^^^^^

.. code-block:: lua

   local M = {};

   function M.callback(wiringx, nr, pulses)
      print(nr);
      print(pulses);
   end

   function M.run()
     local wiringx = wiringX.setup(platform);
     wiringx.pinMode(0, wiringX.PINMODE_OUTPUT);
     wiringx.pinMode(1, wiringX.PINMODE_INPUT);
     wiringx.ISR(1, wiringX.ISR_MODE_BOTH, "callback", 250);

     local pulses = {};
     pulses[1] = 250;
     pulses[2] = 500;
     pulses[3] = 250;
     pulses[4] = 1000;
			wiringx.digitalWrite(1, 1, pulses);
     return 1;
   end

   return M;