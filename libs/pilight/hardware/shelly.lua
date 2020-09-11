--
-- Copyright (C) CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

function M.send(obj, reason, data)
	if reason ~= pilight.reason.SEND_CODE then
		return;
	end

	if data['hwtype'] == nil or data['hwtype'] ~= pilight.hardware.SHELLY then
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
		pilight.log(LOG_ERR, 'Shelly device ' .. data['id'] .. ' couldn\'t be discovered');
		return
	end

	if data['protocol'] == 'shelly1pm' or
	   data['protocol'] == 'shelly1' or
		 data['protocol'] == 'shellyplug' or
		 data['protocol'] == 'shellyplug-s' then
		mqtt.publish("shellies/" .. devs[data['id']]['type'] .. "-" .. data['id'] .. "/relay/0/command", data['state']);

		local tmp = devs[data['id']]['state'];
		devs[data['id']]['state'] = string.lower(data['state']);
		M.createMessage(devs, data['id']);
		devs[data['id']]['state'] = tmp;

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

function M.createMessage(data, id)
	local broadcast = pilight.table();
	if data[id]['type'] ~= nil then
		broadcast['origin'] = 'receiver';
		broadcast['message'] = {};
		if data[id]['type'] == 'shelly1pm' or
		   data[id]['type'] == 'shelly1' or
			 data[id]['type'] == 'shellyplug' or
			 data[id]['type'] == 'shellyplug-s' then
			broadcast['protocol'] = data[id]['type'];
			broadcast['message']['state'] = lookup(data[id], 'relay', 0, 'state') or nil;
			broadcast['message']['power'] = lookup(data[id], 'relay', 0, 'power') or nil;
			broadcast['message']['energy'] = lookup(data[id], 'relay', 0, 'energy') or nil;
			broadcast['message']['overtemperature'] = lookup(data[id], 'overtemperature') or nil;
			broadcast['message']['temperature'] = lookup(data[id], 'temperature') or nil;
			broadcast['message']['connected'] = lookup(data[id], 'connected') or nil;
		end
		broadcast['message']['id'] = id;
	end

	if broadcast['protocol'] ~= nil and
		 broadcast['message'] ~= nil and
		 broadcast['message']['id'] ~= nil then
		 if (broadcast['protocol'] == 'shelly1pm' or
		     broadcast['protocol'] == 'shelly1' or
		     broadcast['protocol'] == 'shellyplug' or
		     broadcast['protocol'] == 'shellyplug-s')
		and
		     broadcast['message']['state'] ~= nil then
			local tmp = json.stringify(broadcast);
			if data[id]['lastmsg'] == nil or data[id]['lastmsg'] ~= tmp then
				local event = pilight.async.event();
				event.register(pilight.reason.RECEIVED_API);
				event.trigger(broadcast());
				data[id]['lastmsg'] = tmp;
			end
		end
	end
end

function M.callback(mqtt, data)
	local tmp = mqtt.getUserdata();

	if data['type'] == MQTT_CONNACK then
		mqtt.subscribe("shellies/+/#");
		mqtt.subscribe("pilight/mqtt/+");
	end
	if data['type'] == MQTT_PUBLISH then
		local substr = pilight.common.explode(data['topic'], "/");
		if #substr >= 3 then
			if substr[1] == 'shellies' then
				local foo = pilight.common.explode(substr[2], "-");
				local id = nil;
				local type_ = nil;
				if #foo >= 2 then
					id = foo[#foo];
					type_ = foo[1];
					for i = 2, #foo-1, 1 do
						type_ = type_ .. '-' .. foo[i];
					end
					if tmp[id] == nil then
						tmp[id] = {};
					end
					tmp[id]['type'] = type_;
					tmp[id]['connected'] = 0;
					if substr[3] == 'temperature' then
						tmp[id]['temperature'] = tonumber(data['message']);
					end
					if substr[3] == 'online' then
						if data['message'] == 'true' then
							tmp[id]['connected'] = 0;
						elseif data['message'] == 'false' then
							tmp[id]['connected'] = 1;
						end
					end
					if substr[3] == 'overtemperature' then
						tmp[id]['overtemperature'] = tonumber(data['message']);
					end
					if substr[3] == 'relay' then
						if tmp[id]['relay'] == nil then
							tmp[id]['relay'] = {};
						end
						if #substr >= 4 then
							if tmp[id]['relay'][tonumber(substr[4])] == nil then
								tmp[id]['relay'][tonumber(substr[4])] = {};
							end
							if #substr == 4 then
								tmp[id]['relay'][tonumber(substr[4])]['state'] = data['message'];
								tmp[id]['timer'] = nil;
							end
							if #substr == 5 then
								if substr[5] == 'power' then
									tmp[id]['relay'][tonumber(substr[4])]['power'] = tonumber(data['message']);
								end
								if substr[5] == 'energy' then
									tmp[id]['relay'][tonumber(substr[4])]['energy'] = tonumber(data['message']);
								end
							end
						end
					end
					M.createMessage(tmp, id);
				end
				tmp[id]['lastseen'] = os.time();
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
	return pilight.hardware.SHELLY;
end

function M.info()
	return {
		name = "shelly",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;
