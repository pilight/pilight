Lua Modules
===========

.. toctree::
   :maxdepth: 1

   defaults/index
   table/index
   async/index
   cast/index
   common/index
   config/index
   datetime/index
   network/index
   io/index

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