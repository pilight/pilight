.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

Label
=====

+------------------+-------------+
| **Feature**      | **Support** |
+------------------+-------------+
| Sending          | |yes|       |
+------------------+-------------+
| Receiving        | |no|        |
+------------------+-------------+
| Config           | |yes|       |
+------------------+-------------+

.. rubric:: Supported Brands

*None*

.. rubric:: Sender Arguments

.. code-block:: console

   -i --id=id               control a device with this id
   -l --label=label         show a specific label
   -c --color=color         give the label a specific color
   
 .. versionadded:: nightly
 
 .. code-block:: console
 
   -b --blink=on|off        enable or disable blinking of the label
   -g --bgcolor=color       give the label a specific background color


.. rubric:: Config

.. code-block:: json
   :linenos:

   {
     "devices": {
       "label": {
         "protocol": [ "generic_label" ],
         "id": [{
           "id": 100
         }],
         "label": "test1234",
         "color": "red",
       }
     },
     "gui": {
       "label": {
         "name": "Label",
         "group": [ "Living" ],
         "media": [ "all" ]
       }
     }
   }

.. versionchanged:: nightly

.. code-block:: json
   :linenos:

   {
     "devices": {
       "label": {
         "protocol": [ "generic_label" ],
         "id": [{
           "id": 100
         }],
         "label": "test1234",
         "color": "red",
         "bgcolor": "white",
         "blink": "off"
       }
     },

+------------------+----------------------+
| **Option**       | **Value**            |
+------------------+----------------------+
| id               | 0 - 99999            |
+------------------+----------------------+
| label            | *any value*          |
+------------------+----------------------+
| color            | *any color*          |
+------------------+----------------------+

.. note::

   Please notice that the label color is not validated in any way. The color information is just forwarded to the GUIs as is. So it could be that some GUIs do not support certain color naming. Using hex colors is therefore the safest, e.g. #000000.

.. versionadded:: nightly

+------------------+----------------------+
| **Option**       | **Value**            |
+------------------+----------------------+
| bgcolor          | *any color*          |
+------------------+----------------------+
| blink            | on / off             |
+------------------+----------------------+

.. note::

   Color, bgcolor and blink are all optional, but must be configured if custom setting of color, background color or blinking are desired.
   By default color will be black (#000000)), bgcolor transparent and blink off.
   Just like for the color option, using hex colors for the background color is the safest.
