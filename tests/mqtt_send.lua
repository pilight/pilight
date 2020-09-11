--
-- Used inside lua_network_mqtt.c
-- - test_lua_network_mqtt_server_client
--
function dump_(o, i)
	if type(o) == 'table' then
		local s = '{'
		for k,v in pairs(o) do
			if type(k) ~= 'number' then k = '"'..k..'"' end
			s = s .. '['..k..']=' .. dump_(v, i+1) .. ','
		end
		return s .. '}'
	else
		return tostring(o)
	end
end

function dump(o)
	return dump_(o, 1);
end

local M = {}

function M.timer(timer)
	local data = timer.getUserdata();
	local mqtt = pilight.network.mqtt(data['mqtt']);

	data['repeat'] = data['repeat'] + 1;

	if data['repeat'] > 2 then
		timer.stop();
	end
end

function M.callback(mqtt, data)
	print('send', dump(data));

	if data['type'] == MQTT_CONNACK then
		local timer = pilight.async.timer();
		timer.setCallback("timer");
		timer.setTimeout(100);
		timer.setRepeat(100);
		timer.setUserdata({['mqtt']=mqtt(),['repeat']=0});
		timer.start();

		mqtt.publish("tele/sonoff/POWER", "on", {MQTT_QOS2});
	end
end

function M.run()
	local mqtt = pilight.network.mqtt();
	local data = mqtt.getUserdata();

	mqtt.setCallback("callback");
	mqtt.connect("127.0.0.1", 11883, "pilight-send");

	return 1;
end

function M.info()
	return {
		name = "mqtt_send",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;