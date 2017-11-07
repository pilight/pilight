.. |yes| image:: ../../../images/yes.png
.. |no| image:: ../../../images/no.png

.. role:: underline
   :class: underline

OpenWeatherMap
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

*None*

.. rubric:: Sender Arguments

*None*

.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "outside": {
         "protocol": [ "openweathermap" ],
         "id": [{
           "location": "amsterdam",
           "country": "nl"
         }],
         "humidity": 76.00,
         "temperature": 7.00,
         "sunrise": 7.00,
         "sunset": 16.15,
         "sun": "set",
         "update": 1
        }
     },
     "gui": {
       "outside": {
         "name": "Temperature",
         "group": [ "Outside" ],
         "media": [ "all" ]
       }
     }
   }

+------------------+----------------------------------+
| **Option**       | **Value**                        |
+------------------+----------------------------------+
| location         | *valid openweathermap location*  |
+------------------+----------------------------------+
| country          | *valid country abbreviation*     |
+------------------+----------------------------------+
| temperature      | -100.00 - 100.00                 |
+------------------+----------------------------------+
| humitidy         | 0.00 - 100.00                    |
+------------------+----------------------------------+
| sunrise          | 00.00 - 23.59                    |
+------------------+----------------------------------+
| sunset           | 00.00 - 23.59                    |
+------------------+----------------------------------+
| sun              | rise / set                       |
+------------------+----------------------------------+
| update           | 0 - 1                            |
+------------------+----------------------------------+

.. rubric:: Optional Settings

:underline:`Device Settings`

+--------------------+-------------+------------+---------------------------------------------+
| **Setting**        | **Default** | **Format** | **Description**                             |
+--------------------+-------------+------------+---------------------------------------------+
| humidity-offset    | 0           | number     | Offset for correcting humidity value        |
+--------------------+-------------+------------+---------------------------------------------+
| temperature-offset | 0           | number     | Offset for correcting temperature value     |
+--------------------+-------------+------------+---------------------------------------------+
| poll-interval      | 86400       | seconds    | The poll interval of the API                |
+--------------------+-------------+------------+---------------------------------------------+

:underline:`GUI Settings`

+----------------------+-------------+------------+----------------------------------------------------------------------+
| **Setting**          | **Default** | **Format** | **Description**                                                      |
+----------------------+-------------+------------+----------------------------------------------------------------------+
| temperature-decimals | 2           | number     | Number of decimal places to show for temperature                     |
+----------------------+-------------+------------+----------------------------------------------------------------------+
| humidity-decimals    | 2           | number     | Number of decimal places to show for humidity                        |
+----------------------+-------------+------------+----------------------------------------------------------------------+
| sunriseset-decimals  | 2           | number     | Number of decimal places to show for sunrise/sunset time             |
+----------------------+-------------+------------+----------------------------------------------------------------------+
| show-humidity        | 1           | 0 or 1     | Whether to display the humidity value                                |
+----------------------+-------------+------------+----------------------------------------------------------------------+
| show-temperature     | 1           | 0 or 1     | Whether to display the temperature value                             |
+----------------------+-------------+------------+----------------------------------------------------------------------+
| show-sunriseset      | 1           | 0 or 1     | Whether to display the sunrise/sunset time                           |
+----------------------+-------------+------------+----------------------------------------------------------------------+
| show-update          | 1           | 0 or 1     | Whether to display the update button                                 |
+----------------------+-------------+------------+----------------------------------------------------------------------+


.. rubric:: Comment

#.  Please note that the openweathermap poll-interval cannot be less than 10 minutes (600 seconds) to respect open weather map traffic and policy
