Development
===========

There are several ways you can connect to or extend pilight. The eventing modules (operators, functions, and actions) are created in Lua based on a pilight <> Lua interface. Developing new modules only requires Lua programming skills and the knowledge of the pilight Lua interface.

.. note:: Work in progress

   The Lua interface is under heavy development. Therefor, not all pilight functionality is already available in Lua. If you need some specific function just do a request and we'll prioritize it.

Interacting with pilight with other software can be done using the socket and/or REST API.

.. toctree::
   :maxdepth: 0

   lua/index
   socket/index
   rest/index
   debugging
