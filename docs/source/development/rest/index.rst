REST API
========

The webserver has some special pages:

- The config page will present the latest configuration JSON object.
- The values page will present the latest values of all device.

   The values and config page will by default only show the configuration that applies to a web GUI type. To retrieve the configuration relevant for other GUI types, use the media argument:

   .. code-block:: console

      http://x.x.x.x:5001/config?media=all

.. versionadded:: 4.0 Send codes through webserver

- The send page can be used to control devices. To use this function, call the send page with a URL-encoded "send" or "registry" JSON object like this:

   .. code-block:: console

      send%3F%7B%0A%09%22action%22%3A%20%22control%22%2C%0A%09%22code%22%3A%20%7B%0A%09%09%22device%22%3A%20%22mainlight%22%2C%0A%09%09%22state%22%3A%20%22on%22%2C%0A%09%09%22values%22%3A%20%7B%0A%09%09%09%22dimlevel%22%3A%20%2210%22%0A%09%09%7D%0A%09%7D%0A%7D

   This will send the object as described earlier.

.. versionchanged:: 8.0 Implemented REST API instead of posting arbitrary JSON codes

- The send page has the same functionality as the ``pilight-send`` program and uses the same arguments:

   .. code-block:: guess

      http://x.x.x.x:5001/send?protocol=kaku_switch&on=1&id=1&unit=1

   As can be seen, the URL arguments are the same as for the ``kaku_switch`` protocol:

   .. code-block:: console

      pi@pilight:~# pilight-send -p kaku_switch -H
      ...
      [kaku_switch]
        -t --on                        send an on signal
        -f --off                       send an off signal
        -u --unit=unit                 control a device with this unit code
        -i --id=id                     control a device with this id
        -a --all                       send command to all devices with this id
        -l --learn                     send multiple streams so switch can learn

   All protocols can be controlled using their respective arguments using the webserver send page as described in this example.

- The control page has the same functionality as the ``pilight-control`` program and uses the same arguments:

   .. code-block:: guess

      http://x.x.x.x/control?device=nasStatus&values[label]=computer%20is%20disconnected&values[color]=red

   In this case a generic_label device is being controlled