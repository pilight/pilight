Lua Modules
===========

.. toctree::
   :maxdepth: 1

   default/index
   reason/index
   table/index
   async/index
   cast/index
   common/index
   config/index
   datetime/index
   network/index
   io/index
   log/index
   wiringx/index

States
------

pilight used 4 identical Lua states that execute all modules. The number of states correspond to the size of the threadpool used by pilight. Lua code can therefor be run multi-threaded. It is important to notice that each lua state is completely reset after the it was triggered by pilight. So it is impossible to save persistent data in the global scope. The pilight library does have functionality to persist data between lua module calls, which is explained in API docs for each function.

Modules
-------

pilight Lua modules are all written in a fairly similar matter. The module should always be built as a Lua table and all modules should have a info function returning information specific for that module. Installing a module is as simple as copying it in the right folder or by letting pilight search in your own modules folder. These paths can be found in the settings page of this manual. A restart of pilight is required for the new module to be loaded.

.. code-block:: lua

   local M = {};

   function M.run()
   end

   function M.info() {
     return {
       name = "EXAMPLE", -- The name of the module
       version = "2.0", -- The version of the module
       reqversion = "7.0", -- The required pilight version in order to run this module
       reqcommit = "94" -- The required pilight commit of the specific version to run this module
     }
   }

   return M;

Operator
^^^^^^^^

Operators require two additional functions: ``associativity`` and ``precedence``. Both function should return the respective values for the operator.

The run function is called with two parameters. The left side value and the right side value of when the operator was called.

The run function is required to return a valid value.

Example code:

.. code-block:: lua

   local M = {}

   function M.run(a, b)
     return pilight.cast.tonumber(a) + pilight.cast.tonumber(b);
   end

   function M.associativity()
     return 60;
   end

   function M.precedence()
     return 1;
   end

   function M.info()
     return {
       name = "+",
       version = "1.0",
       reqversion = "5.0",
       reqcommit = "87"
     }
   end

   return M;

Function
^^^^^^^^

The run function is called with a variable number of parameters. It is up to the module to validate the number and value of the parameters.

The run function is required to return a valid value.

Example code:

.. code-block:: lua

   local M = {}

   function M.run(a, b, c)
     if (a == nil or b == nil) or c ~= nil then
       error("RANDOM requires two arguments");
     end
     if tonumber(a) == nil then
       error(string.format("RANDOM argument #1 expected number, \"%s\" given", type(a)));
     end
     if tonumber(b) == nil then
       error(string.format("RANDOM argument #2 expected number, \"%s\" given", type(b)));
     end

     return pilight.common.random(tonumber(a), tonumber(b));
   end

   function M.info()
     return {
       name = "RANDOM",
       version = "2.1",
       reqversion = "7.0",
       reqcommit = "94"
     }
   end

   return M;

Action
^^^^^^

Actions require two additional functions: ``check`` and ``parameters``.

The check function is used to validate the parameters passed to the module.

The parameters tells pilight which paramaters the module accepts.

The check and run function is called with a multidimensional table with corresponds to the values passed to the action.

The run function is required to return a boolean value.

The following rule

.. code-block:: console

   IF 1 == 1 THEN toggle DEVICE television BETWEEN on AND off

Passes the following parameters table

.. code-block:: javascript

  {
    "DEVICE": {
       "order": 1,
       "values": [ "television" ]
     },
     "BETWEEN": {
       "order": 2,
       "values": [ "on", "off" ]
     }
   }

The following rule

.. code-block:: console

   IF 1 == 1 THEN dim DEVICE mainlight TO 15 FROM 0 IN '45 MINUTE' FOR '15 MINUTE' AFTER '5 MINUTE'

Passes the following parameters table

.. code-block:: javascript

  {
    "DEVICE": {
       "order": 1,
       "values": [ "mainlight" ]
     },
     "TO": {
       "order": 2,
       "values": [ 15 ]
     },
     "FROM": {
       "order": 3,
       "values": [ 0 ]
     },
     "IN": {
       "order": 4,
       "values": [ "45 MINUTE" ]
     },
     "FOR": {
       "order": 5,
       "values": [ "15 MINUTE" ]
     },
     "AFTER": {
       "order": 6,
       "values": [ "5 MINUTE" ]
     }
   }

Example code:

.. code-block:: lua

   local M = {}

   function M.check(parameters)
     if parameters['DEVICE'] == nil then
       error("toggle action is missing a \"DEVICE\" statement");
     end

     if parameters['BETWEEN'] == nil then
       error("toggle action is missing a \"BETWEEN ...\" statement");
     end

     if parameters['DEVICE']['order'] ~= 1 or parameters['BETWEEN']['order'] ~= 2 then
       error("toggle actions are formatted as \"toggle DEVICE ... BETWEEN ...\"");
     end

     if #parameters['BETWEEN']['value'] ~= 2 or parameters['BETWEEN']['value'][3] ~= nil then
       error("toggle action \"BETWEEN\" takes two arguments");
     end

     local nrdev = #parameters['DEVICE']['value'];

     for i = 1, nrdev, 1 do
       local dev = pilight.config.device(parameters['DEVICE']['value'][i]);
       if dev == nil then
         error("device \"" .. parameters['DEVICE']['value'][i] .. "\" does not exist");
       end

       local nrstate = #parameters['BETWEEN']['value'];
       for x = 1, nrstate, 1 do
         if dev.hasState == nil or dev.setState == nil or dev.hasState(parameters['BETWEEN']['value'][x]) == false then
           error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be set to state \"" .. parameters['BETWEEN']['value'][x] .. "\"");
         end
       end
     end

     return 1;
   end

   function M.thread(thread)
     local data = thread.getUserdata();
     local devname = data['device'];
     local devobj = pilight.config.device(devname);

     if devobj.setState(data['new_state']) == false then
       error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_state'] .. "\"")
     end

     devobj.send();
   end

   function M.run(parameters)
     local nrdev = #parameters['DEVICE']['value'];

     for i = 1, nrdev, 1 do
       local devname = parameters['DEVICE']['value'][i];
       local devobj = pilight.config.device(devname);
       local old_state = nil;
       local new_state = nil;
       local async = pilight.async.thread();
       if devobj.hasSetting("state") == true then
         if devobj.getState ~= nil then
           old_state = devobj.getState();
         end
       end

       local data = async.getUserdata();

       if parameters['BETWEEN']['value'][1] == old_state then
         new_state = parameters['BETWEEN']['value'][2]
       else
         new_state = parameters['BETWEEN']['value'][1]
       end

       data['device'] = devname;
       data['old_state'] = old_state;
       data['new_state'] = new_state;

       async.setCallback("thread");
       async.trigger();
     end

     return 1;
   end

   function M.parameters()
     return "DEVICE", "BETWEEN";
   end

   function M.info()
     return {
       name = "toggle",
       version = "4.1",
       reqversion = "7.0",
       reqcommit = "94"
     }
   end

   return M;

Hardware
^^^^^^^^

The hardware modules require two additional functions: ``implements`` and ``validate``.

The implements function tells pilight what hardware interface this module implements: pilight.hardware.RF433 or pilight.hardware.RF868.

The validate functions validates the parameters in the pilight config.

The run function is required to return a boolean value.

The communication between the hardware module and the pilight core is genrally done using the event library as can be seen in the code of the 433gpio hardware module.

Example code:

.. code-block:: lua

   local dump = require "dump";
   local lookup = require "lookup";

   local M = {}

   function M.send(obj, reason, data)
     if reason ~= pilight.reason.SEND_CODE then
       return;
     end

     local config = pilight.config();
     local data1 = config.getData();
     local platform = config.getSetting("gpio-platform");
     local sender = data1['hardware']['433gpio']['sender'];

     local wx = wiringX.setup(platform);

     local count = 0;
     for _ in pairs(data['pulses']) do
       count = count + 1
     end
     if sender >= 0 then
       for i = 1, data['txrpt'], 1 do
         wx.digitalWrite(sender, 1, data['pulses']());
       end
       --
       -- Make sure we don't leave the GPIO dangling
       -- in HIGH position.
       --
       if (count % 2) == 0 then
         wx.digitalWrite(sender, 0);
       end
     end
   end

   function M.callback(obj, nr, pulses)
     if obj == nil then
       return;
     end
     local config = pilight.config();
     local data = config.getData();

     local pulse = 0;
     local length = 0;

     local minrawlen = lookup(data, 'registry', 'hardware', 'RF433', 'minrawlen') or 0;
     local maxrawlen = lookup(data, 'registry', 'hardware', 'RF433', 'maxrawlen') or 0;
     local mingaplen = lookup(data, 'registry', 'hardware', 'RF433', 'mingaplen') or 0;
     local maxgaplen = lookup(data, 'registry', 'hardware', 'RF433', 'maxgaplen') or 0;

     data = obj.getUserdata();
     data['hardware'] = '433gpio';

     if data['length'] == nil then
       data['length'] = 0;
       length = 0;
     end
     if data['pulses'] == nil then
       data['pulses'] = {};
     end
     for i = 1, nr - 1, 1 do
       pulse = pulses[i];
       data['length'] = data['length'] + 1;
       length = data['length'];
       data['pulses'][length] = pulse;
       if length > 512 then
         data['pulses'] = {};
         data['length'] = 0;
         length = 0;
       end
       if pulse > mingaplen then
         if length >= minrawlen and
         length <= maxrawlen and
         ((length+1 >= nr and minrawlen == 0) or (minrawlen > 0)) then
           local event = pilight.async.event();
           event.register(pilight.reason.RECEIVED_OOK);
           event.trigger(data());

           data['pulses'] = {};
           data['length'] = 0;
           length = 0;
         end
         if length+1 >= nr then
           data['pulses'] = {};
           data['length'] = 0;
           length = 0;
         end
       end
     end
   end

   function M.validate()
     local config = pilight.config();
     local platform = config.getSetting("gpio-platform");
     local data = config.getData();
     local obj = nil;

     for x in pairs(data['hardware']['433gpio']) do
       if x ~= 'sender' and x ~= 'receiver' then
         error(x .. "is an unknown parameter")
       end
     end

     local receiver = data['hardware']['433gpio']['receiver'];
     local sender = data['hardware']['433gpio']['sender'];

     if receiver == nil then
       error("receiver parameter missing");
     end

     if receiver == nil then
       error("sender parameter missing");
     end

     if platform == nil or platform == 'none' then
       error("no gpio-platform configured");
     end

     obj = wiringX.setup(platform);

     if obj == nil then
       error(platform .. " is an invalid gpio-platform");
     end
     if receiver == nil then
       error("no receiver parameter set");
     end
     if sender == nil then
       error("no sender parameter set");
     end
     if type(sender) ~= 'number' then
       error("the sender parameter must be a number, but a " .. type(sender) .. " was given");
     end
     if type(receiver) ~= 'number' then
       error("the receiver parameter must be a number, but a " .. type(receiver) .. " was given");
     end
     if sender < -1 then
       error("the sender parameter cannot be " .. tostring(sender));
     end
     if receiver < -1 then
       error("the receiver parameter cannot be " .. tostring(receiver));
     end
     if receiver == sender then
       error("sender and receiver cannot be the same GPIO");
     end
     if sender > -1 then
       if obj.hasGPIO(sender) == false then
         error(sender .. " is an invalid sender GPIO");
       end
       if obj.pinMode(sender, wiringX.PINMODE_OUTPUT) == false then
         error("GPIO #" .. sender .. " cannot be set to output mode");
       end
     end
     if receiver > -1 then
       if obj.hasGPIO(receiver) == false then
         error(receiver .. " is an invalid receiver GPIO");
       end
       if obj.pinMode(receiver, wiringX.PINMODE_INPUT) == false then
         error("GPIO #" .. receiver .. " cannot be set to input mode");
       end
       if obj.ISR(receiver, wiringX.ISR_MODE_BOTH, "callback", 250) == false then
         error("GPIO #" .. receiver .. " cannot be configured as interrupt");
       end
     end
   end

   function M.run()
     local config = pilight.config();
     local platform = config.getSetting("gpio-platform");
     local data = config.getData();
     local obj = nil;

     local receiver = data['hardware']['433gpio']['receiver'];
     local sender = data['hardware']['433gpio']['sender'];

     obj = wiringX.setup(platform);
     obj.pinMode(sender, wiringX.PINMODE_OUTPUT);
     obj.pinMode(receiver, wiringX.PINMODE_INPUT);
     obj.ISR(receiver, wiringX.ISR_MODE_BOTH, "callback", 250);

     local event = pilight.async.event();
     event.register(pilight.reason.SEND_CODE);
     event.setCallback("send");

     return 1;
   end

   function M.implements()
     return pilight.hardware.RF433
   end

   function M.info()
     return {
       name = "433gpio",
       version = "4.1",
       reqversion = "7.0",
       reqcommit = "94"
     }
   end

   return M;