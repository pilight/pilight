Reason
======

.. versionadded:: 8.1.5

The pilight reasons that can be used in the pilight lua event interface

- **pilight.reason.RECEIVED_PULSETRAIN**
  **pilight.reason.RECEIVED_OOK**

   Tells pilight that a new pulsetrain of OOK pulses are received.

   .. c:var:: int length
      
      The number of pulses counted from 1

   .. c:var:: array pulses

      A numeric array with numeric pulses counted from 1

   .. c:var:: string hardware

      The hardware module that received the pulses

- **pilight.reason.SEND_CODE**

   Tells the hardware module that pilight wants to sent pulses

   .. c:var:: int rawlen
      
      The number of pulses counted from 1

   .. c:var:: int txrpt

      How many times should be repeat the sent action

   .. c:var:: string protocol

      The protocol being sent

   .. c:var:: string hwtype

      The hardware type the protocol uses

   .. c:var:: string hwtype

      The UUID of the device that should sent
