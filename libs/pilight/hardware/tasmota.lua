--
-- Copyright (C) CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

--
-- Module type:
-- 1: Sonoff Basic
--

local dump = require "dump";
local json = require "json";
local lookup = require "lookup";

local M = {}

function M.validate()
	return;
end

function M.reset(timer)
	local data = timer.getUserdata();
	local mqtt = pilight.network.mqtt(data['mqtt']);
	local tmp = mqtt.getUserdata();

	for _ in pairs(tmp) do
		if(_ == data['id']) then
			M.createMessage(tmp, _);
		end
	end
end

function M.timer(timer)
	local data = timer.getUserdata();
	local mqtt = pilight.network.mqtt(data['mqtt']);
	local tmp = mqtt.getUserdata();

	--
	-- Request switch status when it's still
	-- unknown to pilight
	--
	for _ in pairs(tmp) do
		if tmp[_]['hasstatus'] ~= nil then
			if tmp[_]['hasstatus'] == false then
				mqtt.publish("cmnd/" .. _ .. "/status", "0");
			end
		end
		if os.time()-tmp[_]['lastseen'] > 120 then
			mqtt.publish("cmnd/" .. _ .. "/status", "STATUS0");
		end
	end
end

function M.createMessage(data, id)
	local broadcast = pilight.table();
	if data[id]['type'] ~= nil then
		broadcast['origin'] = 'receiver';
		broadcast['message'] = {};
		if data[id]['type'] == 1 then
			broadcast['protocol'] = 'tasmota_switch';
			if data[id]['status'] ~= nil then
				broadcast['message']['state'] = data[id]['status'];
			end
			if data[id]['connected'] ~= nil then
				broadcast['message']['connected'] = data[id]['connected'];
			end
		end
		broadcast['message']['id'] = id;
	end

	if broadcast['protocol'] ~= nil and
		 broadcast['message'] ~= nil and
		 broadcast['message']['id'] ~= nil then
		 if broadcast['protocol'] == 'tasmota_switch' and
				broadcast['message']['state'] ~= nil then

			local event = pilight.async.event();
			event.register(pilight.reason.RECEIVED_API);
			event.trigger(broadcast());
		end
	end
end

function M.callback(mqtt, data)
	local tmp = mqtt.getUserdata();

	if data['type'] == MQTT_CONNACK then
		local timer = pilight.async.timer();
		timer.setCallback("timer");
		timer.setTimeout(3000);
		timer.setRepeat(3000);
		timer.setUserdata({['mqtt']=mqtt()});
		timer.start();

		mqtt.subscribe("tele/+/+");
		mqtt.subscribe("cmnd/+/+");
		mqtt.subscribe("stat/+/+");
		mqtt.subscribe("pilight/mqtt/+");
	end

	if data['type'] == MQTT_PUBLISH then
		local substr = pilight.common.explode(data['topic'], "/");
		if #substr == 3 then
			if substr[1] == 'tele' then
				if substr[3] == 'LWT' then
					if data['message'] == 'Online' then
						if tmp[substr[2]] == nil then
							tmp[substr[2]] = {};
							tmp[substr[2]]['hasstatus'] = false;
							tmp[substr[2]]['lastseen'] = os.time();
						end
						tmp[substr[2]]['connected'] = 0;
					end
					if data['message'] == 'Offline' then
						tmp[substr[2]]['connected'] = 1;
						M.createMessage(tmp, substr[2]);
					end
				end
				tmp[substr[2]]['lastseen'] = os.time();
			end
			if substr[1] == 'stat' then
				if substr[3] == 'POWER' then
					if tmp[substr[2]] ~= nil then
						tmp[substr[2]]['status'] = string.lower(data['message']);
						tmp[substr[2]]['timer'] = nil;
					end
					M.createMessage(tmp, substr[2]);
				end
				if substr[3] == 'STATUS' then
					local jobject = json.parse(data['message']);
					tmp[substr[2]]['type'] = lookup(jobject, 'Status', 'Module') or nil;
					tmp[substr[2]]['name'] = lookup(jobject, 'Status', 'FriendlyName') or nil;
				end
				if substr[3] == 'STATUS5' then
					local jobject = json.parse(data['message']);
					tmp[substr[2]]['ip'] = lookup(jobject, 'StatusNET', 'IPAddress') or nil;
					tmp[substr[2]]['mac'] = lookup(jobject, 'StatusNET', 'Mac') or nil;
					tmp[substr[2]]['hasstatus'] = true;
				end
				if substr[3] == 'STATUS11' then
					local jobject = json.parse(data['message']);
					tmp[substr[2]]['status'] = lookup(jobject, 'StatusSTS', 'POWER') or nil;
					if tmp[substr[2]]['status'] ~= nil then
						tmp[substr[2]]['status'] = string.lower(tmp[substr[2]]['status']);
						tmp[substr[2]]['timer'] = nil;
					end
					tmp[substr[2]]['hasstatus'] = true;
					M.createMessage(tmp, substr[2]);
				end

				tmp[substr[2]]['lastseen'] = os.time();
			end
			if substr[1] == 'pilight' and substr[2] == 'mqtt' then
				if data['message'] == 'disconnected' and tmp[substr[3]] ~= nil then
					tmp[substr[3]]['connected'] = 1;
					M.createMessage(tmp, substr[3]);
				end
			end
		end
	end
	if data['type'] == MQTT_SUBACK then
	end
end

function M.send(obj, reason, data)
	if reason ~= pilight.reason.SEND_CODE then
		return;
	end

	if data['hwtype'] == nil or data['hwtype'] ~= pilight.hardware.TASMOTA then
		return;
	end

	local data1 = obj.getUserdata();
	local mqtt = pilight.network.mqtt(data1['mqtt']);
	local devs = mqtt.getUserdata();

	if data['id'] == nil then
		return;
	end

	if data['connected'] ~= nil then
		local broadcast = pilight.table();
		broadcast['origin'] = 'receiver';
		broadcast['message'] = {};
		broadcast['protocol'] = data['protocol'];
		broadcast['message']['id'] = data['id'];
		broadcast['message']['connected'] = data['connected'];

		local event = pilight.async.event();
		event.register(pilight.reason.RECEIVED_API);
		event.trigger(broadcast());
		return;
	end

	if devs[data['id']] == nil then
		pilight.log(LOG_ERR, 'Tasmota device ' .. data['id'] .. ' couldn\'t be discovered');
		return
	end

	if data['protocol'] == 'tasmota_switch' then
		mqtt.publish("cmnd/" .. data['id'] .. "/POWER", data['state']);

		local tmp = devs[data['id']]['status'];
		devs[data['id']]['status'] = string.lower(data['state']);
		M.createMessage(devs, data['id']);
		devs[data['id']]['status'] = tmp;

		--
		-- If we don't get a confirmation within
		-- 3000ms, restore to the current state
		--
		local timer = pilight.async.timer();
		timer.setCallback("reset");
		timer.setTimeout(3000);
		timer.setRepeat(0);
		timer.setUserdata({['mqtt']=mqtt(),['id']=data['id']});
		timer.start();
	end
end

function M.run(a)
	local mqtt = pilight.network.mqtt();

	local config = pilight.config();
	local data = config.getData();
	local port = lookup(data, 'settings', 'mqtt-port') or 1883;

	mqtt.setCallback("callback");
	mqtt.connect("127.0.0.1", port);

	local event = pilight.async.event();
	event.setUserdata({['mqtt']=mqtt()});
	event.register(pilight.reason.SEND_CODE);
	event.setCallback("send");
end

function M.implements()
	return pilight.hardware.TASMOTA;
end

function M.info()
	return {
		name = "tasmota",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;
