Config
======

The config library allows for interaction between the pilight configuration and configured pilight devices.

- `Devices`_
   - `Datetime`_
   - `Dimmer`_
   - `Label`_
   - `Switch`_
   - `Screen`_
- `Settings`_

.. warning::

   Not all devices are currently supported in the config library. More devices will be added in the future.

.. versionadded:: 8.1.3

A new version object must be initialized before the different configuration subsections can be used:

.. code-block:: lua

	local config = pilight.config();
	local devobj = config.getDevice("switch");
	local standalone = config.getSetting("standalone");

Devices
-------

.. versionchanged:: 8.1.3

.. c:function:: userdata [pilight.config()].getDevice(string device)

   The device parameter should correspond to a valid configured device. If the device isn't configured a ``nil`` is returned.

.. versionadded:: 8.1.2

.. c:function:: userdata pilight.config.getDevice(string device)

   The device parameter should correspond to a valid configured device. If the device isn't configured a ``nil`` is returned.

.. c:function:: boolean getActionId()

   Gets the unique device identifier.

.. c:function:: table getId()

   Returns the device id multidimensional table.

   .. code-block:: lua

      local config = pilight.config();
      local dev = config.device("dimmer");
      local id = dev.getId();
      print(id[1]['id']); -- 1
      print(id[1]['unit']); -- 0
      print(id[2]['id']); -- 6
      print(id[2]['unit']); -- 2

.. c:function:: userdata getName()

   Returns the name of the device

.. c:function:: table getType()

   Returns the device types of the configured device in an array.

.. c:function:: boolean hasSetting(string setting)

   Checks if the device has a specific setting. If this is true, a corresponding getter and setter are present. E.g. ``hasSetting('dimlevel')`` corresponds to ``setDimlevel(1)`` and ``getDimlevel()``

.. c:function:: boolean setActionId()

   Sets a unique identifier for this specific device. This identifier is used to check if an action execution should be aborted or not.

Datetime
^^^^^^^^

.. c:function:: number getDay()

   Returns the day of the datetime device.

.. c:function:: number getMonth()

   Returns the month of the datetime device.

.. c:function:: number getYear()

   Returns the year of the datetime device.

.. c:function:: number getHour()

   Returns the hour of the datetime device.

.. c:function:: number getMinute()

   Returns the hour of the datetime device.

.. c:function:: number getSecond()

   Returns the seconds of the datetime device.

.. c:function:: number getWeekday()

   Returns the weekday of the datetime device.

.. c:function:: number getDST()

   Returns the daylight savings time of the datetime device.

.. c:function:: number getTable()

   Returns a datetime table with year, month, day, hour, minute, second keys with their corresponding values.

Dimmer
^^^^^^

.. c:function:: number getDimlevel()

   Returns the dimlevel of the dimmer.

.. c:function:: string getState()

   Returns the state of the dimmer.

.. c:function:: boolean hasState(string state)

   Check if this dimmer can be set to a specific state.

.. c:function:: boolean hasDimlevel(number dimlevel)

   Check if this dimmer can be set to a specific dimlevel.

.. c:function:: boolean setState(string state)

   Set the dimmer to a specific device.

.. c:function:: boolean setDimlevel(number dimlevel)

   Set the dimmer to a specific dimlevel.

.. c:function:: string send()

   Sends the new settings to the dimmer.

Label
^^^^^

.. c:function:: string getLabel()

   Returns the label of the label.

.. c:function:: string getColor()

   Returns the color of the label.

.. c:function:: boolean setLabel(string label)

   Set the label to a specific label.

.. c:function:: boolean setLabel(string label)

   Set the label label to a specific color.

.. c:function:: string send()

   Sends the new settings to the label.

Switch
^^^^^^

.. c:function:: string getState()

   Returns the state of the switch.

.. c:function:: boolean hasState(string state)

   Check if this switch can be set to a specific state.

.. c:function:: boolean setState(string state)

   Set the switch to a specific state.

.. c:function:: string send()

   Sends the new settings to the switch.

Screen
^^^^^^

.. c:function:: string getState()

   Returns the state of the screen.

.. c:function:: boolean hasState(string state)

   Check if this screen can be set to a specific state.

.. c:function:: boolean setState(string state)

   Set the screen to a specific state.

.. c:function:: string send()

   Sends the new settings to the screen.

Settings
--------

.. versionchanged:: 8.1.3

.. c:function:: number | string [pilight.config()].getSetting(string setting)

   Returns the value of a specific setting in the pilight configuration. If a setting was not configured, a ``nil`` is returned.

.. versionadded:: 8.1.2

.. c:function:: number | string [pilight.config.setting(string setting)

   Returns the value of a specific setting in the pilight configuration. If a setting was not configured, a ``nil`` is returned.
