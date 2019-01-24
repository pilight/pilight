.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

KERUI D026 Contact
==================

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
| KERUI            | D026         |
+------------------+--------------+

.. rubric:: Sender Arguments

*None*

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "contact": {
         "protocol": [ "kerui_D026" ],
         "id": [{
           "unitcode": 0
         }],
         "state": "closed"
        }
     }
   }

+------------------+--------------------------+
| **Option**       | **Value**                |
+------------------+--------------------------+
| id               | 0 - 524288               |
+------------------+--------------------------+
| state            | opened / closed / tamper |
+------------------+--------------------------+
