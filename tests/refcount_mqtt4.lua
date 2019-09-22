--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_mqtt4
--
local M = {};

function M.mqtt(mqtt, data)
	print("mqtt");
end

function M.timer(timer)
	print("timer");
	local data = timer.getUserdata();
	local mqtt = pilight.network.mqtt(data['mqtt']);
	mqtt.setCallback("mqtt");
	mqtt.connect("127.0.0.1", 11883);

	data['mqtt'] = nil;
	timer.stop();
end

function M.run()
	local timer = pilight.async.timer();
	local mqtt = pilight.network.mqtt();
	print("timer", timer());
	print("mqtt", mqtt());

	timer.setUserdata({['mqtt'] = mqtt()});

	timer.setCallback("timer");
	timer.setTimeout(100);
	timer.setRepeat(100);
	timer.start();
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