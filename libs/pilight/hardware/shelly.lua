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

	if devs[data['id']] == nil then
		pilight.log(LOG_ERR, 'Shelly device ' .. data['id'] .. ' couldn\'t be discovered');
		return
	end

	if data['protocol'] == 'shelly1pm' or data['protocol'] == 'shelly1' then
		mqtt.publish("shellies/" .. devs[data['id']]['type'] .. "-" .. data['id'] .. "/relay/0/command", data['state']);
	end
end

function M.timer(timer)
	local data = timer.getUserdata();
	local mqtt = pilight.network.mqtt(data['mqtt']);
	local tmp = mqtt.getUserdata();

	mqtt.ping();
end

function M.createMessage(data, id)
	local broadcast = pilight.table();
	if data[id]['type'] ~= nil then
		broadcast['origin'] = 'receiver';
		broadcast['message'] = {};
		if data[id]['type'] == 'shelly1pm' or data[id]['type'] == 'shelly1' then
			broadcast['protocol'] = data[id]['type'];
			broadcast['message']['state'] = lookup(data[id], 'relay', 0, 'state') or nil;
			broadcast['message']['power'] = lookup(data[id], 'relay', 0, 'power') or nil;
			broadcast['message']['energy'] = lookup(data[id], 'relay', 0, 'energy') or nil;
			broadcast['message']['overtemperature'] = lookup(data[id], 'overtemperature') or nil;
			broadcast['message']['temperature'] = lookup(data[id], 'temperature') or nil;
		end
		broadcast['message']['id'] = id;
	end

	if broadcast['protocol'] ~= nil and
		 broadcast['message'] ~= nil and
		 broadcast['message']['id'] ~= nil then
		 if (broadcast['protocol'] == 'shelly1pm' or
		     broadcast['protocol'] == 'shelly1')
				and
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

		mqtt.subscribe("shellies/+/#");
	end
	if data['type'] == MQTT_PUBLISH then
		local substr = pilight.common.explode(data['topic'], "/");
		if #substr >= 3 then
			local id = pilight.common.explode(substr[2], "-");
			if #id == 2 then
				if tmp[id[2]] == nil then
					tmp[id[2]] = {};
				end
				tmp[id[2]]['type'] = id[1];
				if substr[3] == 'temperature' then
					tmp[id[2]]['temperature'] = tonumber(data['message']);
				end
				if substr[3] == 'overtemperature' then
					tmp[id[2]]['overtemperature'] = tonumber(data['message']);
				end
				if substr[3] == 'relay' then
					if tmp[id[2]]['relay'] == nil then
						tmp[id[2]]['relay'] = {};
					end
					if #substr >= 4 then
						if tmp[id[2]]['relay'][tonumber(substr[4])] == nil then
							tmp[id[2]]['relay'][tonumber(substr[4])] = {};
						end
						if #substr == 4 then
							tmp[id[2]]['relay'][tonumber(substr[4])]['state'] = data['message'];
						end
						if #substr == 5 then
							if substr[5] == 'power' then
								tmp[id[2]]['relay'][tonumber(substr[4])]['power'] = tonumber(data['message']);
							end
							if substr[5] == 'energy' then
								tmp[id[2]]['relay'][tonumber(substr[4])]['energy'] = tonumber(data['message']);
							end
						end
					end
				end
				M.createMessage(tmp, id[2]);
			end
			tmp[id[2]]['lastseen'] = os.time();
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
