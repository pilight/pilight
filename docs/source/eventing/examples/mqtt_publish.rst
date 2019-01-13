.. |yes| image:: ../../images/yes.png
.. |no| image:: ../../images/no.png

.. role:: underline
   :class: underline

.. versionadded:: nightly

Publish mqtt message  on contact trigger
==================================

.. rubric:: Description

Publish an mqtt message based on a contact device trigger.

.. rubric:: Devices

.. code-block:: json
   :linenos:

   {

     "frontdoor-contact": {
       "protocol": [ "kaku_contact" ],
       "id": [{
         "id": 123456,
         "unit": 0
       }],
       "state": "opened"
     }
   }

.. rubric:: Rule

.. code-block:: json
   :linenos:

   {
     "frontdoor-contact-publish": {
       "rule": "IF frontdoor-contact.state == opened or frontdoor-contact.state == closed THEN mqtt_publish HOST localhost PORT 1883 USER foo PASSWORD bar TOPIC pilight/states/frontdoor MESSAGE frontdoor-contact.state",
       "active": 1
     }
   }
