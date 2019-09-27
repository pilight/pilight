Network
=======

Various functions to do network communication

- `Mail`_
- `HTTP`_

.. versionadded:: nightly

- `COAP`_
- `MDNS`_
- `MQTT`_

Mail
----

Allows sending mails

API
^^^

.. c:function:: userdata pilight.network.mail()

   Creates a new mail object

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the mail object.

.. c:function:: string getFrom()

   Returns the sender.

.. c:function:: string getHost()

   Returns the mail server host.

.. c:function:: string getMessage()

   Returns the mail message.

.. c:function:: string getPassword()

   Returns the mail server password.

.. c:function:: number getPort()

   Returns the mail server port.

.. c:function:: number getSSL()

   Returns the mail server ssl.

.. c:function:: boolean getStatus()

   After the mail was sent this function can be used in the callback to check if the mail was sent successfully to the server.

.. c:function:: string getTo()

   Returns the recipient.

.. c:function:: string getUser()

   Returns the mail server user.

.. c:function:: boolean setCallback(string callback)

   The name of the callback being triggered by the mail library. The mail object will be passed as the only parameter of this callback function.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the mail object. The userdata table cannot be of another type as returned from the getData functions.

.. c:function:: boolean setFrom()

   Sets the sender.

.. c:function:: boolean setHost()

   Sets the mail server host.

.. c:function:: boolean setMessage()

   Sets the mail message.

.. c:function:: boolean setPassword()

   Sets the mail server password.

.. c:function:: boolean setPort()

   Sets the mail server port.

.. c:function:: boolean setSSL()

   Sets the mail server ssl.

.. c:function:: boolean setTo()

   Sets the recipient.

.. c:function:: boolean setUser()

   Sets the mail server user.

.. c:function:: boolean send()

   Send the mail

Example
^^^^^^^

.. code-block:: lua

   local M = {}

   function M.callback(mail)
     print(mail.getStatus());
   end

   function M.run()
     local mailobj = pilight.network.mail();

     mailobj.setSubject("foo");
     mailobj.setFrom("order@pilight.org");
     mailobj.setTo("info@pilight.nl");
     mailobj.setMessage("bar");
     mailobj.setHost("127.0.0.1");
     mailobj.setPort(25);
     mailobj.setUser("pilight");
     mailobj.setPassword("test");
     mailobj.setSSL(0);
     mailobj.setCallback("callback");
     mailobj.send();

     return 1;
   end

   return M;

HTTP
----

Do HTTP GET or POST requests

API
^^^

.. c:function:: userdata pilight.network.http()

   Creates a new http object

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the http object.

.. c:function:: string getMimetype()

   Returns the mimetype of the received data. This means it does not returns the mimetype set by the setMimetype() function.

.. c:function:: string getData()

   Returns the data received from the url. This means it does not returns the data set by the setData() function.

.. c:function:: string getUrl()

   Returns the url.

.. c:function:: number getCode()

   After the HTTP request was sent this function can be used in the callback to check if the return HTTP code.

.. c:function:: number getSize()

   After the HTTP request was sent this function can be used in the callback to get the size of the received content.

.. c:function:: boolean setCallback(string callback)

   The name of the callback being triggered by the http library. The http object will be passed as the only parameter of this callback function.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the http object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. c:function:: boolean setUrl(string url)

   Sets the request URL. Examples:

     | http://127.0.0.1/
     | https://127.0.0.1/
     | http://127.0.0.1:8080/
     | http://127.0.0.1/index.php?page=foo
     | https://username:password@www.pilight.org:8080/index.php

.. c:function:: boolean setMimetype(string mimetype)

   Sets the mimetype of the data being posted.

.. c:function:: boolean setData(string data)

   Sets the data to be posted.

.. c:function:: boolean get()

   Send a GET request to the URL.

.. c:function:: boolean post()

   Send a POST request to the URL with the data set.

Example
^^^^^^^

.. code-block:: lua

   local M = {}

   function M.callback(http)
     print(http.getCode());
     print(http.getMimetype());
     print(http.getSize());
     print(http.getData());
   end

   function M.run()
     local httpobj = pilight.network.http();

     httpobj.setUrl("http://127.0.0.1:10080/")
     httpobj.setCallback("callback");
     httpobj.get();

     return 1;
   end

   return M;

COAP
----

Send and/or receive COAP messages

API
^^^

.. c:function:: userdata pilight.network.coap()

   Creates a new coap object

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the coap object.

.. c:function:: boolean listen(userdata table)

   Listens to coap messages

.. c:function:: boolean setCallback(string callback)

   The name of the callback being triggered by the coap library. The parameters of the callback function are the coap object, received data object, ip and port of the sender.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the http object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. c:function:: boolean send(userdata table)

   Sends a coap message

.. note:: COAP data specifications

   The data the lua COAP interface parses has to be set in a low-level way. That means you have to construct a valid coap object yourself. The COAP responses are represented in the same low-level way. The COAP interface follows the same specification for it's data as the protocol description.

   The allowed COAP data fields are

   - **numeric** ``ver``
   - **numeric** ``t``
   - **numeric** ``token``
   - **string** ``payload``
   - **numeric** ``code``
   - **numeric** ``msgid``
   - **array** ``options``
      - **numeric** ``num``
      - **string** ``val``

Example
^^^^^^^

.. code-block:: lua

   local M = {}

   function M.discover(coap, data, ip, port)
     return;
   end

   function M.run()
      local coap = pilight.network.coap();

      local send = {};
      send['ver'] = 1;
      send['t'] = 1;
      send['code'] = 0001;
      send['msgid'] = 0001;
      send['options'] = {};
      send['options'][0] = {};
      send['options'][0]['val'] = 'cit';
      send['options'][0]['num'] = 11;
      send['options'][1] = {};
      send['options'][1]['val'] = 'd';
      send['options'][1]['num'] = 11;

      coap.setCallback("discover");
      coap.send(send);
      coap.listen();

     return 1;
   end

   return M;

MDNS
----

Send and/or receive MDNS messages

API
^^^

.. c:function:: userdata pilight.network.mdns()

   Creates a new mdns object

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the mdns object.

.. c:function:: boolean listen(userdata table)

   Listens to mdns messages

.. c:function:: boolean setCallback(string callback)

   The name of the callback being triggered by the mdns library. The parameters of the callback function are the mdns object, received data object, ip and port of the sender.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the http object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. c:function:: boolean send(userdata table)

   Sends a mdns message

.. note:: MDNS data specifications

   The data the lua MDNS interface parses has to be set in a low-level way. That means you have to construct a valid mdns object yourself. The MDNS responses are represented in the same low-level way. The MDNS interface follows the same specification for it's data as the protocol description.

   The allowed MDNS data fields are

   - **numeric** ``id``
   - **numeric** ``qr``
   - **numeric** ``opcode``
   - **numeric** ``aa``
   - **numeric** ``tc``
   - **numeric** ``rd``
   - **numeric** ``ra``
   - **numeric** ``z``
   - **numeric** ``ad``
   - **numeric** ``cd``
   - **numeric** ``rcode``
   - **numeric** ``rr_auth``
   - **numeric** ``queries``
      - **string** ``name``
      - **numeric** ``qu``
      - **numeric** ``type``
      - **numeric** ``class``
   - **numeric** ``answers``
   - **numeric** ``records``
      - **string** ``type``
      - **numeric** ``name``
      - **numeric** ``length``
      - **numeric** ``ttl``
      - **numeric** ``class``
      - **numeric** ``flush``
      - **numeric** ``domain``
      - **numeric** ``weight``
      - **numeric** ``priority``
      - **numeric** ``port``
      - **numeric** ``ip``
      - **array** ``options``
         - **string** ``values``

   The ``answers`` and ``records`` fields share the same fields. The combination of fields that are allowed depends on the
   on the answer or record type. The ``options`` array contains numeric keys with values.

Example
^^^^^^^

.. code-block:: lua

   local M = {}

   function M.discover(mdns, data, ip, port)
     return;
   end

   function M.run()
      local mdns = pilight.network.mdns();

      local send = {};
      send['id'] = tonumber("AABB", 16);
      send['qr'] = 0;
      send['opcode'] = 2;
      send['aa'] = 0;
      send['tc'] = 1;
      send['rd'] = 0;
      send['ra'] = 1;
      send['z'] = 0;
      send['ad'] = 1;
      send['cd'] = 0;
      send['rcode'] = 1;
      send['rr_auth'] = 0;
      send['queries'] = {};
      send['queries'][1] = {};
      send['queries'][1]['name'] = "testname.local.foo";
      send['queries'][1]['qu'] = 0;
      send['queries'][1]['type'] = 33;
      send['queries'][1]['class'] = 1;
      send['queries'][2] = {};
      send['queries'][2]['name'] = "testname1.local1.foo";
      send['queries'][2]['qu'] = 0;
      send['queries'][2]['type'] = 33;
      send['queries'][2]['class'] = 1;
      send['queries'][3] = {};
      send['queries'][3]['name'] = "testname.local1.foo";
      send['queries'][3]['qu'] = 0;
      send['queries'][3]['type'] = 33;
      send['queries'][3]['class'] = 1;

      mdns.setCallback("discover");
      mdns.send(send);
      mdns.listen();

     return 1;
   end

   return M;

MQTT
----

Send and/or receive MQTT messages

API
^^^

.. c:function:: userdata pilight.network.mqtt([userdata mqtt instance])

   Creates a new mqtt object or restore the saved instance when passing the instance parameter.

.. versionadded:: nightly

.. c:function:: userdata pilight.network.mqtt()()

   Returns the mqtt instance as lightuserdata so it can be stored in a pilight metatable.

.. c:function:: nil connect([string ip, number port][, string id][, string willtopic, string willmessage])

   Connect to a mqtt broken. This function either takes 2, 3, or 5 arguments.

   - The ``ip`` and ``port``
   - The ``ip``, ``port``, and ``id``
   - The ``ip``, ``port``, ``id``, ``willtopic``, and ``willmessage``

.. c:function:: userdata getUserdata()

   Returns a persistent userdata table for the lifetime of the mqtt object.

.. c:function:: nil publish(string topic, string message[, table options])

   Publish to a certain message to topic. The optional options table gives the possibility to set the duplicate message, retained message, and/or QoS options like this: ``{MQTT_DUB, MQTT_RETAIN, MQTT_QOS1}``

.. c:function:: nil pubrec(number msgid)

   Send a pubrec message.

.. c:function:: nil pubrel(number msgid)

   Send a pubrel message.

.. c:function:: nil pubcomp(number msgid)

   Send a pubcomp message.

.. c:function:: nil ping()

   Send a ping message.

.. c:function:: nil subscribe(string topic[, table options])

   Subscribes to a topic with a certain QoS. The QoS parameter can be set using the optional options table ``{MQTT_QOS1}`` or ``{MQTT_QOS2}``.

.. c:function:: boolean setUserdata(userdata table)

   Set a new persistent userdata table for the lifetime of the http object. The userdata table cannot be of another type as returned from the getUserdata functions.

.. note:: MQTT packets

   The MQTT packets are either ``MQTT_CONNECT``, ``MQTT_CONNACK``, ``MQTT_PUBLISH``, ``MQTT_PUBACK``, ``MQTT_PUBREC``, ``MQTT_PUBREL``, ``MQTT_PUBCOMP``, ``MQTT_SUBSCRIBE``, ``MQTT_UNSUBSCRIBE``, ``MQTT_UNSUBACK``,  ``MQTT_PINGREQ``, ``MQTT_PINGRESP``, ``MQTT_DISCONNECT``, ``MQTT_CONNECTED``

Example
^^^^^^^

.. code-block:: lua

   local M = {}

   function M.timer(timer)
      local data = timer.getUserdata();
      local mqtt = pilight.network.mqtt(data['mqtt']);

      mqtt.ping();
   end

   function M.callback(mqtt, data)
      if data['type'] == MQTT_CONNACK then
         local timer = pilight.async.timer();
         timer.setCallback("timer");
         timer.setTimeout(3000);
         timer.setRepeat(3000);
         timer.setUserdata({['mqtt']=mqtt()});
         timer.start();

         mqtt.subscribe("#", {MQTT_QOS2});
      end
      if data['type'] == MQTT_PUBACK then
      end
      if data['type'] == MQTT_PUBLISH then
         mqtt.pubrec(data['msgid']);
      end
      if data['type'] == MQTT_PUBREL then
         mqtt.pubcomp(data['msgid']);
      end
      if data['type'] == MQTT_PINGRESP then
      end
   end

   function M.run()
     mqtt.setCallback("callback");
     mqtt.connect("127.0.0.1", 1883);

     return 1;
   end

   return M;