.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

iwds07 Contact
==============

+------------------+-------------+
| **Feature**      | **Support** |
+------------------+-------------+
| Sending          | |no|        |
+------------------+-------------+
| Receiving        | |yes|       |
+------------------+-------------+
| Config           | |yes|       |
+------------------+-------------+

.. rubric:: Supported Brands

+------------------+--------------+
| **Brand**        | **Protocol** |
+------------------+--------------+
| Golden Security  | iwds07       |
+------------------+--------------+

.. rubric:: Sender Arguments

*None*

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "contact": {
         "protocol": [ "iwds07" ],
         "id": [{
           "unitcode": 0
         }],
         "state": "closed",
         "battery": 1
        }
     }
   }

+------------------+--------------------------+
| **Option**       | **Value**                |
+------------------+--------------------------+
| id               | 0 - 524288               |
+------------------+--------------------------+
| state            | opened / closed          |
+------------------+--------------------------+
| battery          | 0 / 1                    |
+------------------+--------------------------+
