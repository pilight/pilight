Changelog
=========

.. versionadded:: 8.1.5

.. rubric:: Changes

- Lua pilight.defaults are renamed to pilight.default
- Removed 433lirc hardware module

.. rubric:: Improvment

- Add warning when other protocol options besides the id are defined as an array
- Added Kerui an TFA2017 protocol
- Allow userdata in eventpool callbacks
- Added C getters and setters for lua metatable
- Added pilight event lua c to lua interface
- Ported hardware modules to lua
- Ported config hardware parsing to lua
- Added pilight io spi and serial interface to lua
- Added pilight log to lua
- Allow enforcing the old state in a switch action
- Pass lua metatable by reference
- Add support for the coap protocol
- C handler for lua metatables
- pilight lua metatables are now threadsafe
- Added IWDS07 contact protocol

.. rubric:: Fixes

- Fixed segfault in duplicate lua metatable key
- Fixed segfault due to long device strings #413
- Fixed garbage collection in event module loaders
- Fixed bug using boolean value in lua metatable
- Fixed lua stack not fully cleared in certain cases
- Fixed bug in which action execution wasn't logged
- Fixed threadpool work creation outside main thread
- Fixed buffer overflow in lua sync lib
- Fixed unitialized values

.. versionadded:: 8.1.4

.. rubric:: Improvements

- Remove libunwind as a dependency
- Add pointer location to memory debugger
- Make sure lua is not initialized multiple times
- Differentiate between plain lua and lua_c libraries
- Ported config registry to lua
- Add CLI parameter for changing the lua-root library folder
- Add CLI paremeter for changing the storage-root library folder
- Better detect if multiple pilight-daemons are running
- Continue generating uuid when initial attempt failed

.. rubric:: Fixes

- Fixed version parameter in pilight-daemon
- Fixed lua wiringx aarch64 detection
- Fixed pilight-debug timing parameters
- Fixed pilight-control not respecting server/port parameters
- Fixed case sensitive event action parsing
- Fixed pilight-receive server/port CLI arguments
- Fixed uninitialized value in lua io file lib
- Fixed memory leak in lua config lib
- Properly gc config memory on shutdown
- Fixed memory leak in ssl library
- Fixed memory leak in event module loaders
- Allow compilation without webserver(_https)
- Fixed possible deadlock when clearing lua state
- Prevent buffer overflow in 433nano hardware module
- Fixed possible segfault in plua_gc_unreg

.. versionadded:: 8.1.3

.. rubric:: Improvements

- Added aarch64 / arm64 packages
- Added lua file and dir io
- Improved lua config library
- Allow nested metatables in lua modules
- Allow boolean values in lua metatables
- Added pilight defaults to lua modules
- Added from parameter to switch action
- Allow multi-character short options
- Added storage-root to clients
- Added eventing MAX and MIN function

.. rubric:: Fixes

- Fix arctech dimmer in which dimlevel 0 would turn the dimmer off
- Fix eventing library in which a rule was triggered by a device affected in an action
- Do not partially write config when partially read
- Fixed a segfault in pilight-debug and pilight-raw

.. versionadded:: 8.1.2

.. rubric:: Improvements

- HTTP library correctly parses HTTP headers without Content-Length
- Allow userdata to be passed to a mail callback
- Ported all event actions to Lua
- Massively improved the pilight Lua library. Refer to the Lua Development pages in this manual for more information.

.. rubric:: Bugfixes

- Allow multiple dots in rules for e.g. IP addresses.
- Fixed bug in which didn't close opened files.
- Few fixes in mail library.
- Fixed pushbullet SSL handshake error.
- Improved Alecto WX500

.. versionadded:: 8.1.1

.. rubric:: Improvements

- HTTP library callback when client stops responding
- Disable loopback by default (can be enabled with the ``loopback`` setting).

.. rubric:: Bugfixes

- New eventparser again evaluates devices used in functions
- 433nano works again

.. versionadded:: 8.1.0

.. rubric:: Improvements

- Ported events operators and functions to LUA
- Complete rewritten event parser with:
   - (nested) IF/ELSE/THEN support
   - case insensitive
   - operator precedence compliant
   - better error messages
   - string concatenation
   - calculations inside actions
   - etc.

.. rubric:: bugfixes

- webGUI: Take `stats-enable` into account
- webGUI: Warp long labels into multiple lines
- webGUI: Keep values together on small screens
- webGUI: Fix dimmer element being too small

.. versionadded:: 8.0.10

.. rubric:: Bugfixes

- Ignore dst in DATE_FORMAT function to keep input datetime unaffected by dst.

.. versionadded:: 8.0.9

.. rubric:: Bugfixes

- Properly reset time variable in DATE_FORMAT function to correctly handle dst.

.. versionadded:: 8.0.8

.. rubric:: Bugfixes

- --debuglevel=2 replaced --debuglevel=1. Now both are allowed.

.. versionadded:: 8.0.7

.. rubric:: Change functionality

- Openweathermap now requires a personal API key. Until now, a free pilight organization API key was used, but that exceeded the maximum allowed requests per minute.

.. rubric:: Bugfixes

- Actually allow --debuglevel=2 to see additional mail logging

.. versionadded:: 8.0.6

.. rubric:: Bugfixes

- Readded min / max dimlevel in generic_dimmer, but validation is still disabled

.. versionadded:: 8.0.5

.. rubric:: New functionality

- IPv6 support for all in library clients
- Disable min / max dimlevel in generic_dimmer

.. rubric:: Internal core changed

- Callback in mail library when host is not reachable
- Shutdown on more signals

.. rubric:: Bugfixes

- Fixed pilight removal with apt
- Calling REST API with socket API
- Prevent using ``.`` as message body
- Segfault due to wrong memory freeing

.. versionadded:: 8.0.4

.. rubric:: New functionality

- webGUI long labels are wrapped 

.. rubric:: Internal core changed

- Olsen timezone database is now used for timezone parsing
- 433.92Mhz now respect the UUID setting
- improved datetime, openweathermap, weatherunderground, and sunriseset library

.. rubric:: Bugfixes

- config not being saved at shutdown

.. versionadded:: 8.0.3

.. rubric:: Bugfixes

- webgui labels are word-wrapped into multiple lines on small screens
- webgui takes ``stats-enable`` into account by hiding CPU stats
- memory usage statistics has been removed because they were unreliable
- arctech_dimmer signals sent by pilight are now correctly received by pilight as well

.. rubric:: webserver, mail and http library

At this moment the https, mail, and webserver module and the full ``pilight-sha256`` program has been backported from rewrite. The asynchronous I/O library libuv has been added as well as the new SSL and eventpool module. The openweathermap and weather underground protocols have been adapted to use this new code as well as the pushbullet and pushover event actions.

- pilight now supports a HTTPS webserver which can be configured in the settings:

   .. code-block:: json

      { "webserver-https-port": 5002 }

- pilight also stopped detecting if the mailserver you have configures requires an SSL connection. To tell pilight about the SSL requirement of a mail server a new setting has been added. Servers that switch from a plain connection to SSL require a 0 value here:

   .. code-block:: json

      { "smtp-ssl": 1 }

.. versionadded:: 8.0.2

.. rubric:: Bugfixes

- wiringX log was scrambled

.. versionadded:: 8.0.1

.. rubric:: Bugfixes

- gpio_switch protocol that stops working after a while
- pilight not starting at boottime

.. versionadded:: 8.0

.. note::

   Not all changes from development where ported to stable. Especially the rules can break when upgraded from the latest development to the latest stable. Porting the new eventing code is in the planning real soon, but until then, just stick with development.

.. rubric:: Breaking changes

- The PHP parsing functionality has been removed.

.. rubric:: New functionality

- Proper REST API through the webserver. See `Development -> API <https://manual.pilight.org/development/api.html#webserver>`_.

- Protocol names cannot be used anymore as configuration device names.

- Supporting flashing the Arduino Uno.

- Added TFA 30.X weather stations. FIXME
- Added Quigg GT-9000 protocol. See FIXME.
- Added Secudo / FlammEx smoke sensor. FIXME.
- Added Eurodomest protocol. `Protocols -> 433.92Mhz -> Switches -> Eurodomest <https://manual.pilight.org/protocols/433.92/switch/eurodomest.html>`_
- Added TCM 218943 protocol `Protocols -> 433.92Mhz -> Weather -> TCM <https://manual.pilight.org/protocols/433.92/weather/protocols/433.92/weather/tcm.html>`_

- Allow event triggers based on received actions. See `Eventing -> Syntax <https://manual.pilight.org/eventing/syntax.html#devices>`_.
- Added the ISNOT operator. See `Eventing -> Operators <https://manual.pilight.org/eventing/operators.html>`_

- Added webGUI support for illuminance sensor.

- Allow filtering ``pilight-receive`` protocols. See `Programs -> pilight-receive <https://manual.pilight.org/programs/receive.html>`_
- Split pilight daemon debug and foreground functionality in two parameters. See `Programs -> pilight-daemon <https://manual.pilight.org/programs/daemon.html>`_

- Removed internal wiringX integration and changes to shared library linking.
- Validate duplicate 433gpio GPIO for both sender and receiver value.

- Default pilight paths have changes.
- The tzdata.json file has been deprecated and moved internally into pilight.

.. rubric:: Bugfixes

- XBMC and LIRC protocol. Properly try to reconnect when connection is lost.
- X10 Switch. The same unit was sent for units 8 and 9.
- Beamisch Switch. Resend the beamisch switch 10 times again.
- Arctech Switch. More precise pulse lengths.
- EV1527 Switch. Allow a bigger ID range.
- Conrad RSL switch. Add learn parameter for device learning.
- RSL366 Switch. Better protocol validation for less false positives.
- Arctech Dusk. Fixed swapped states.
- Clarus Switch. Prevent crashes on too long ID parameter.
- Alecto WX500 Weather Station. Fixed negative temperatures.
- Alecto WS1700 Weather Station. Better protocol implementation according to specsheet.
- Arctech Old Switch. Better protocol validation for less false positives.
- Quigg GT-1000 Switch. Added support more group codes.
- Elro 800 Switch. Support for more systemcodes.
- Teknihall Weather Station. Fixed negatives temperature values.
- Dim action. IN timeout could take too long.
- ``smtp-user`` setting. Any character is now allowed.
- ``smtp-email`` setting. Better email address validation.
- pilight startup. Start pilight after network at boottime.

.. rubric:: Internal core changes

- Frequency properties are made hardware module independent.
- Added file_get_contents function.
- Updated webGUI jQuery and moment library .
- Better internal pushbullet and pushbullet action argument parsing.
- Better binary to decimals and vice versa parsing.
- Support for AArch64 compilation.
- Send version after request values API call.
- Differentiate JSON types using bitmasks.
- Better mail library status checking.
- Added python3 client example.

- Various typo fixes.

- Fixed shared and static library linking.
- Fixed unused protocol repeat parameter.
- Fixed various uninitialized fields or incomplete buffer initializations.
- Fixed memory leaks in Dim, Label, and Switch action.
- Fixed inconsistent min and max dimlevel parsing in generic dimmer.
- Fixed webGUI dimmer display bugs.
- Fixed possible deadlock in datetime library.
- Fixed various buffer overflows in protocols.
- Fixed lm75, lm76, and bmp180 i2c-patch parsing.
- Fixed ntp time library bugs.
