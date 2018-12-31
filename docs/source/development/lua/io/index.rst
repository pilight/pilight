IO
==

.. versionadded:: 8.1.3

Various functions to do filesystem io

- `File`_
- `Dir`_

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
