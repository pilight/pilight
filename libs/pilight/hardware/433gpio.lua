--
-- Copyright (C) CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local dump = require "dump";
local lookup = require "lookup";

local M = {}

function M.send(obj, reason, data)
	if reason ~= pilight.reason.SEND_CODE then
		return;
	end

	local config = pilight.config();
	local data1 = config.getData();
	local platform = config.getSetting("gpio-platform");
	local sender = data1['hardware']['433gpio']['sender'];

	local wx = wiringX.setup(platform);

	local count = 0;
	for _ in pairs(data['pulses']) do
		count = count + 1
	end
	if sender >= 0 then
		for i = 1, data['txrpt'], 1 do
			wx.digitalWrite(sender, 1, data['pulses']());
		end
		--
		-- Make sure we don't leave the GPIO dangling
		-- in HIGH position.
		--
		if (count % 2) == 0 then
			wx.digitalWrite(sender, 0);
		end
	end
end

function M.callback(obj, nr, pulses)
	if obj == nil then
		return;
	end
	local config = pilight.config();
	local data = config.getData();

	local pulse = 0;
	local length = 0;

	local minrawlen = lookup(data, 'registry', 'hardware', 'RF433', 'minrawlen') or 0;
	local maxrawlen = lookup(data, 'registry', 'hardware', 'RF433', 'maxrawlen') or 0;
	local mingaplen = lookup(data, 'registry', 'hardware', 'RF433', 'mingaplen') or 0;
	local maxgaplen = lookup(data, 'registry', 'hardware', 'RF433', 'maxgaplen') or 0;

	data = obj.getUserdata();
	data['hardware'] = '433gpio';

	if data['length'] == nil then
		data['length'] = 0;
		length = 0;
	end
	if data['pulses'] == nil then
		data['pulses'] = {};
	end
	for i = 1, nr - 1, 1 do
		pulse = pulses[i];
		data['length'] = data['length'] + 1;
		length = data['length'];
		data['pulses'][length] = pulse;
		if length > 512 then
			data['pulses'] = {};
			data['length'] = 0;
			length = 0;
		end
		if pulse > mingaplen then
			if length >= minrawlen and
				length <= maxrawlen and
				((length+1 >= nr and minrawlen == 0) or (minrawlen > 0)) then
				local event = pilight.async.event();
				event.register(pilight.reason.RECEIVED_OOK);
				event.trigger(data());

				data['pulses'] = {};
				data['length'] = 0;
				length = 0;
			end
			if length+1 >= nr then
				data['pulses'] = {};
				data['length'] = 0;
				length = 0;
			end
		end
	end
end

function M.validate()
	local config = pilight.config();
	local platform = config.getSetting("gpio-platform");
	local data = config.getData();
	local obj = nil;

	for x in pairs(data['hardware']['433gpio']) do
		if x ~= 'sender' and x ~= 'receiver' then
			error(x .. "is an unknown parameter")
		end
	end

	local receiver = data['hardware']['433gpio']['receiver'];
	local sender = data['hardware']['433gpio']['sender'];

	if receiver == nil then
		error("receiver parameter missing");
	end

	if receiver == nil then
		error("sender parameter missing");
	end

	if platform == nil or platform == 'none' then
		error("no gpio-platform configured");
	end

	obj = wiringX.setup(platform);

	if obj == nil then
		error(platform .. " is an invalid gpio-platform");
	end
	if receiver == nil then
		error("no receiver parameter set");
	end
	if sender == nil then
		error("no sender parameter set");
	end
	if type(sender) ~= 'number' then
		error("the sender parameter must be a number, but a " .. type(sender) .. " was given");
	end
	if type(receiver) ~= 'number' then
		error("the receiver parameter must be a number, but a " .. type(receiver) .. " was given");
	end
	if sender < -1 then
		error("the sender parameter cannot be " .. tostring(sender));
	end
	if receiver < -1 then
		error("the receiver parameter cannot be " .. tostring(receiver));
	end
	if receiver == sender then
		error("sender and receiver cannot be the same GPIO");
	end
	if sender > -1 then
		if obj.hasGPIO(sender) == false then
			error(sender .. " is an invalid sender GPIO");
		end
		if obj.pinMode(sender, wiringX.PINMODE_OUTPUT) == false then
			error("GPIO #" .. sender .. " cannot be set to output mode");
		end
	end
	if receiver > -1 then
		if obj.hasGPIO(receiver) == false then
			error(receiver .. " is an invalid receiver GPIO");
		end
		if obj.pinMode(receiver, wiringX.PINMODE_INPUT) == false then
			error("GPIO #" .. receiver .. " cannot be set to input mode");
		end
		if obj.ISR(receiver, wiringX.ISR_MODE_BOTH, "callback", 250) == false then
			error("GPIO #" .. receiver .. " cannot be configured as interrupt");
		end
	end
end

function M.run()
	local config = pilight.config();
	local platform = config.getSetting("gpio-platform");
	local data = config.getData();
	local obj = nil;

	local receiver = data['hardware']['433gpio']['receiver'];
	local sender = data['hardware']['433gpio']['sender'];

	obj = wiringX.setup(platform);
	obj.pinMode(sender, wiringX.PINMODE_OUTPUT);
	obj.pinMode(receiver, wiringX.PINMODE_INPUT);
	obj.ISR(receiver, wiringX.ISR_MODE_BOTH, "callback", 250);

	local event = pilight.async.event();
	event.register(pilight.reason.SEND_CODE);
	event.setCallback("send");

	return 1;
end

function M.implements()
	return pilight.hardware.RF433
end

function M.info()
	return {
		name = "433gpio",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;