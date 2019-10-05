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

function M.timer(timer)
	local data = timer.getUserdata();
	local mqtt = pilight.network.mqtt(data['mqtt']);
	local tmp = mqtt.getUserdata();

	mqtt.ping();
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

		mqtt.subscribe("#");
	end
	if data['type'] == MQTT_PUBLISH then
		local broadcast = pilight.table();
		broadcast['origin'] = 'receiver';
		broadcast['message'] = {};
		broadcast['message']['topic'] = data['topic'];
		broadcast['message']['payload'] = data['message'];
		broadcast['protocol'] = 'mqtt';

		local event = pilight.async.event();
		event.register(pilight.reason.RECEIVED_API);
		event.trigger(broadcast());
	end
	if data['type'] == MQTT_SUBACK then
	end
end

function M.run(a)
	local mqtt = pilight.network.mqtt();

	local config = pilight.config();
	local data = config.getData();
	local port = lookup(data, 'settings', 'mqtt-port') or 1883;

	local mqtt = pilight.network.mqtt();

	mqtt.setCallback("callback");
	mqtt.connect("127.0.0.1", port);
end

function M.implements()
	return nil;
end

function M.info()
	return {
		name = "mqtt",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;
