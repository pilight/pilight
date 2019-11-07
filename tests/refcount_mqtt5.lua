--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_mqtt5
--
local M = {};

function M.mqtt(mqtt, data)
	print("mqtt");
end

function M.send(event)
	print("event");
end

function M.run()
	local event = pilight.async.event();
	local mqtt = pilight.network.mqtt();
	print("mqtt", mqtt());

	mqtt.setCallback("mqtt");
	mqtt.connect("127.0.0.1", 11883);

	local event = pilight.async.event();
	event.setUserdata({['mqtt']=mqtt()});
	event.register(pilight.reason.SEND_CODE);
	event.setCallback("send");
end

function M.info()
	return {
		name = "refcount",
		version = "1.0",
		reqversion = "1.0",
		reqcommit = "0"
	}
end

return M;