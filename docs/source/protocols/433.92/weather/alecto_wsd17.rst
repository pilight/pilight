.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
:class: underline

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
| Alecto WSD17     | alecto_wsd17 |
+------------------+--------------+

.. rubric:: Sender Arguments

*None*

.. rubric:: Config

.. code-block:: json
   :linenos:

       {
         "devices": {
           "weather": {
             "protocol": [ "alecto_wsd17" ],
             "id": [{
               "id": 100
             }],
             "temperature": 23.00
            }
         },
         "gui": {
           "weather": {
             "name": "Weather Station",
             "group": [ "Outside" ],
             "media": [ "all" ]
           }
         }
       }

    +------------------+-----------------+
    | **Option**       | **Value**       |
+------------------+-----------------+
| id               | 0 - 4095        |
+------------------+-----------------+
| temperature      | -50 - 70        |
+------------------+-----------------+

.. rubric:: Optional Settings

:underline:`Device Settings`

+--------------------+-------------+------------+-----------------------------------------+
| **Setting**        | **Default** | **Format** | **Description**                         |
+--------------------+-------------+------------+-----------------------------------------+
| temperature-offset | 0           | number     | Offset for correcting temperature value |
+--------------------+-------------+------------+-----------------------------------------+

:underline:`GUI Settings`

+----------------------+-------------+------------+-----------------------------------------------------------+
| **Setting**          | **Default** | **Format** | **Description**                                           |
+----------------------+-------------+------------+-----------------------------------------------------------+
| temperature-decimals | 2           | number     | Number of decimal places to show for temperature          |
+----------------------+-------------+------------+-----------------------------------------------------------+
| show-temperature     | 1           | 0 or 1     | Whether to display the temperature value                  |
+----------------------+-------------+------------+-----------------------------------------------------------+

.. rubric:: Comment

This protocol was created for pilight with the help of this document: http://www.tfd.hu/tfdhu/files/wsprotocol/auriol_protocol_v20.pdf

Humidity and battery are not supported yet
