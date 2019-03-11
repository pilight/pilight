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

function M.timer(timer)
	local config = pilight.config();
	local data = config.getData();
	local port = data['hardware']['433nano']['comport'];

	local serial = pilight.io.serial(port);
	if serial.open() == false then
		error("could not connect to device \"" .. port .. "\"");
	end
	serial.read();
	timer.stop();
end

function M.send(obj, reason, data)
	if reason ~= pilight.reason.SEND_CODE then
		return;
	end

	local config = pilight.config();
	local data1 = config.getData();

	local code = "c:";
	local match = -1;
	local pulses = {};

	for i in pairs(data['pulses']) do
		match = -1;
		local x = 0;
		for x in pairs(pulses) do
			if pulses[x] == data['pulses'][i] then
				match = x;
				break;
			end
		end
		if match == -1 then
			pulses[#pulses + 1] = data['pulses'][i];
			code = code .. tostring(#pulses - 1);
		else
			code = code .. tostring(match - 1);
		end
	end
	code = code .. ";p:";
	for x in pairs(pulses) do
		code = code .. tostring(pulses[x])
		if x < #pulses then
			code = code .. ","
		end
	end
	code = code .. ";r:" .. data['txrpt'] .. "@";

	local port = data1['hardware']['433nano']['comport'];

	local serial = pilight.io.serial(port);
	serial.write(code);
end

function M.callback(rw, serial, line)
	if line == nil then
		return;
	end

	if rw == 'read' then
		if #line > 0 or line == '\n' then
			local data = serial.getUserdata();

			if line == '\n' and data['write'] == 0 then
				data['write'] = 1;
			end

			if data['write'] == 1 then
				data['write'] = 2;
				local config = pilight.config();
				local data1 = config.getData();
				local minrawlen = lookup(data1, 'registry', 'hardware', 'RF433', 'minrawlen') or 0;
				local maxrawlen = lookup(data1, 'registry', 'hardware', 'RF433', 'maxrawlen') or 0;
				local mingaplen = lookup(data1, 'registry', 'hardware', 'RF433', 'mingaplen') or 0;
				local maxgaplen = lookup(data1, 'registry', 'hardware', 'RF433', 'maxgaplen') or 0;
				serial.write("s:" .. minrawlen .. "," .. maxrawlen .. "," .. 5200 .. "," .. maxgaplen .. "@");
			end

			if data['content'] == nil then
				data['content'] = "";
			end

			data['content'] = data['content'] .. line;

			local a = #data['content'];
			local l = a;

			while true do
				for i = 1, l, 1 do
					if string.sub(data['content'], i, i) == '@' then
						l = i;
						break;
					end
				end

				if l == a then
					break;
				end

				local c = string.sub(data['content'], l, l);
				if c == '@' then
					content = string.sub(data['content'], 1, l)

					data['content'] = string.sub(data['content'], l+1, a);
					a = #data['content'];

					local stream = {};
					local pulses = {};
					for i = 1, l, 1 do
						c = string.sub(content, i, i);
						if c == 'c' then
							if string.sub(content, i+1, i+1) == ':' then
								i = i + 1;
								for x = i, l, 1 do
									c = string.sub(content, x+1, x+1);
									if c == ';' then
										i = x + 2;
										break;
									end
									stream[#stream+1] = c;
								end
							else
								serial.read();
								return;
							end
						end
						if c == 'p' then
							if string.sub(content, i+1, i+1) == ':' then
								i = i + 1;
								for x = i, l, 1 do
									c = string.sub(content, x+1, x+1);
									if c == ',' or c == '@' then
										c = string.sub(content, i+1, x);
										i = x + 1;
										pulses[#pulses+1] = tonumber(c);
										if c == '@' then
											i = x + 1;
											break;
										end
									end
								end
							else
								serial.read();
								return;
							end
						end
						if c == 'v' then
							-- pilight usb nano initialized
						end
					end
					l = a;

					if data['length'] == nil then
						data['length'] = 0;
					end
					if data['pulses'] == nil then
						data['pulses'] = {};
					end

					if #stream > 0 then
						local b = 1;
						for i = 1, #stream, 1 do
							data['pulses'][b] = pulses[1];
							b = b + 1;
							data['pulses'][b] = pulses[stream[i] + 1];
							b = b + 1;
						end

						data['length'] = b-1;
						local tmp = data['content'];
						data['content'] = nil;
						data['write'] = nil;

						local event = pilight.async.event();
						event.register(pilight.reason.RECEIVED_PULSETRAIN);
						event.trigger(data());

						data['pulses'] = {};
						data['content'] = tmp;
						data['write'] = 2;
					end
				end
			end
		end
		serial.read();
	elseif rw == 'disconnect' then
		local timer = pilight.async.timer();
		timer.setCallback("timer");
		timer.setTimeout(1000);
		timer.setRepeat(1000);
		timer.start();
	end
end

function M.validate()
	local config = pilight.config();
	local data = config.getData();
	local obj = nil;

	for x in pairs(data['hardware']['433nano']) do
		if x ~= 'comport' then
			error(x .. "is an unknown parameter")
		end
	end

	local port = data['hardware']['433nano']['comport'];

	file = pilight.io.file(port);

	if file.exists() == false then
		error(port .. " does not exist");
	end

	if file.open("a+") == false then
		error(port .. " cannot be opened for reading and/or writing");
	end
	file.close();
end

function M.run()
	local config = pilight.config();
	local data = config.getData();
	local obj = nil;
	local port = data['hardware']['433nano']['comport'];

	local serial = pilight.io.serial(port);
	serial.setBaudrate(57600);
	serial.setParity('n');
	serial.setCallback("callback");
	if serial.open() == false then
		error("could not connect to device \"" .. port .. "\"");
	end
	serial.read();

	local data = serial.getUserdata();
	data['write'] = 0;
	data['hardware'] = '433nano';

	local event = pilight.async.event();
	event.register(pilight.reason.SEND_CODE);
	event.setCallback("send");

	return 1;
end

function M.implements()
	return pilight.hardware.RF433;
end

function M.info()
	return {
		name = "433nano",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;