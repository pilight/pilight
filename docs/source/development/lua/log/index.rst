Log
===

.. versionadded:: 8.1.5

Log messages based on severeness and level of informativeness.

If the log should be posted to stdout and/or if the log messages
should be retained in a logfile is all configured from the main
daemon, therefor these options cannot be changed from inside the
pilight lua interface

.. c:function:: table pilight.log(number loglevel, string message)

   The loglevel can be either one of these constants:

   - LOG_EMERG
   - LOG_ALERT
   - LOG_CRIT
   - LOG_ERR
   - LOG_WARNING
   - LOG_NOTICE
   - LOG_INFO
   - LOG_DEBUG

   These levels are the same as the C syslog loglevels and they follow the same logic.