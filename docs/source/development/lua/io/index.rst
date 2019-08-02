IO
==

.. versionadded:: 8.1.3

Various functions to do io

- `File`_
- `Dir`_

.. versionadded:: 8.1.5

- `Serial`_
- `SPI`_

File
----

Allows interaction with files

API
^^^

.. c:function:: userdata pilight.io.file(string file)

   Creates a new file object

.. c:function:: boolean exists()

   Returns true if the file exists and false if not

.. c:function:: iterator open(string mode)

   Opens the file with a specific mode

      +----------+------------------------------------------------+
      | **Mode** | **Description**                                |
      +----------+------------------------------------------------+
      | r        | Opens a file for reading. The file must exist. |
      +----------+------------------------------------------------+
      | w        | | Creates an empty file for writing.           |
      |          | | If a file with the same name already exists, |
      |          | | its content is erased and the file is        |
      |          | | considered as a new empty file.              |
      +----------+------------------------------------------------+
      | a        | | Appends to a file. Writing operations,       |
      |          | | append data at the end of the file. The file |
      |          | | is created if it does not exist.             |
      +----------+------------------------------------------------+
      | r+       | | Opens a file to update both reading          |
      |          | | and writing. The file must exist.            |
      +----------+------------------------------------------------+
      | w+       | | Creates an empty file for both reading       |
      |          | | and writing.                                 |
      +----------+------------------------------------------------+
      | a+       | | Opens a file for reading and appending.      |
      +----------+------------------------------------------------+

.. c:function:: iterator read(number bytes)

   Reads maximum number of bytes from the opened file at a time

.. c:function:: iterator readline()

   Returns an iterator to read the lines of the file

   .. code-block:: lua

      for line in file.readline() do
         print(line);
      end

.. c:function:: iterator write(string data)

   Writes date to the opened file

.. c:function:: iterator seek(number location [, string whence])

   Sets the position of the file to the given offset, relative to the optional whence

      +------------+--------------------------------------+
      | **Whence** | **Description**                      |
      +------------+--------------------------------------+
      | set        | Beginning of file (default)          |
      +------------+--------------------------------------+
      | cur        | Current position of the file pointer |
      +------------+--------------------------------------+
      | end        | End of file                          |
      +------------+--------------------------------------+

.. c:function:: boolean close()

   Closes the file object

Example
^^^^^^^

.. code-block:: lua

   function M.run()
      local file = pilight.io.file("/var/log/syslog");
      file.open("r");
      local content = '';
      for line in file.readline() do
         content = content .. line;
      end
      file.close();

     return 1;
   end

   return M;

Dir
---

Allows interaction with directories

API
^^^

.. c:function:: userdata pilight.io.dir(string directory)

   Creates a new directory object

.. c:function:: boolean exists()

   Returns true if the directory exists and false if not

.. c:function:: boolean close()

   Closes the directory object

Example
^^^^^^^

.. code-block:: lua

   function M.run()
      local dir = pilight.io.dir("/tmp");
      if dir.exists() == false then
         dir.close();
         error("/tmp does not exists")

      dir.close();

     return 1;
   end

   return M;

Serial
------

Allows interaction with serial devices

API
^^^

.. c:function:: userdata pilight.io.serial(string port)

   Creates a new serial object or return a previously created object for the same port

.. c:function:: boolean setBaudrate()

   Sets or changes the baudrate of the serial connection
   Supported baudrates are:

      +-----------------------------------------------------------+
      | **Supported baudrates**                                   |
      +-----------------------------------------------------------+
      | | 50, 75, 110, 134, 150, 200, 600, 1200, 1800, 2400, 4800 |
      | | 9600, 19200, 38400, 57600, 115200, 230400               |
      +-----------------------------------------------------------+

.. c:function:: boolean setParity()

   Sets or changes the parity of the serial connection

      +------------+-------------------------+
      | **Letter** | **Function**            |
      +------------+-------------------------+
      | n, s       | No parity               |
      +------------+-------------------------+
      | o          | Disable parity checking |
      +------------+-------------------------+
      | e          | Enable parity checking  |
      +------------+-------------------------+

.. c:function:: boolean open()

   Open the serial device

.. c:function:: boolean close()

   Close the serial device

.. c:function:: boolean write(string line)

   Write the line to the serial device

.. c:function:: boolean read()

   Tell the serial device we are (still) reading

.. c:function:: boolean setCallback(string callback)

   The name of the callback being triggered when io occured. This callback will be called when data was read, written or when an error occured.

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the serial object.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the serial object. The userdata table cannot be of another type as returned from the getUserdata functions.

Example
^^^^^^^

.. code-block:: lua

   function M.callback(rw, serial, line)
     if rw == 'write' then
       print(line); -- success or fail
     elseif rw == 'read' then
       print(line);
       serial.read();
     elseif rw == 'disconnect' then
       serial.close();
     end
   end

   function M.run()
     local serial = pilight.io.serial("/dev/ttyUSB0");
     serial.setBaudrate(57600);
     serial.setParity('n');
     serial.setCallback("callback");
     if serial.open() == false then
       error("could not connect to device /dev/ttyUSB0");
     end
     serial.write("foo");
     serial.read();
   end

   return M;

SPI
---

Allows interaction with Serial Peripheral Interfaces

API
^^^

.. c:function:: userdata pilight.io.spi(number channel[, number frequency])

   Creates a new SPI object or return a previously created object for the same channel.

.. c:function:: number rw(table value)

   Sets or gets the value of a certain register. The table required should have the following format:

   .. code-block:: lua

     { adr = adr, val = val }

   - read

     The ``adr`` key should contain the register addres to be read, the ``val`` key is ignored. A number is returned when valid data was read.

   - write

     The ``adr`` key should contain the register addres to be written, the ``val`` should contain the value to be set. No data is returned.

Example
^^^^^^^

.. code-block:: lua

   function M.run()
     local spi = pilight.io.spi(0, 2500000);
     spi.rw({ adr = 0x01, val = 0x02 });

     print(spi.rw({ adr = 0xAB, val = 0x00 }));
   end

   return M;
