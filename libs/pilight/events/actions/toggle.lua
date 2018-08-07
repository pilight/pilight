--
-- Copyright (C) 2018 CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local M = {}

function M.check(parameters)
	if parameters['DEVICE'] == nil then
		error("toggle action is missing a \"DEVICE\" statement");
	end

	if parameters['BETWEEN'] == nil then
		error("toggle action is missing a \"BETWEEN ...\" statement");
	end

	if parameters['DEVICE']['order'] ~= 1 or parameters['BETWEEN']['order'] ~= 2 then
		error("toggle actions are formatted as \"toggle DEVICE ... BETWEEN ...\"");
	end

	if #parameters['BETWEEN']['value'] ~= 2 or parameters['BETWEEN']['value'][3] ~= nil then
		error("toggle action \"BETWEEN\" takes two arguments");
	end

	local nrdev = #parameters['DEVICE']['value'];
	local config = pilight.config();
	for i = 1, nrdev, 1 do
		local dev = config.getDevice(parameters['DEVICE']['value'][i]);
		if dev == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" does not exist");
		end
		local nrstate = #parameters['BETWEEN']['value'];
		for x = 1, nrstate, 1 do
			if dev.hasState == nil or dev.setState == nil or dev.hasState(parameters['BETWEEN']['value'][x]) == false then
				error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be set to state \"" .. parameters['BETWEEN']['value'][x] .. "\"");
			end
		end
	end

	return 1;
end

function M.thread(thread)
	local data = thread.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if devobj.setState(data['new_state']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_state'] .. "\"")
	end

	devobj.send();
end

function M.run(parameters)
	local nrdev = #parameters['DEVICE']['value'];

	for i = 1, nrdev, 1 do
		local devname = parameters['DEVICE']['value'][i];
		local config = pilight.config();
		local devobj = config.getDevice(devname);
		local old_state = nil;
		local new_state = nil;
		local async = pilight.async.thread();
		if devobj.hasSetting("state") == true then
			if devobj.getState ~= nil then
				old_state = devobj.getState();
			end
		end

		local data = async.getUserdata();

		if parameters['BETWEEN']['value'][1] == old_state then
			new_state = parameters['BETWEEN']['value'][2]
		else
			new_state = parameters['BETWEEN']['value'][1]
		end

		data['device'] = devname;
		data['old_state'] = old_state;
		data['new_state'] = new_state;

		async.setCallback("thread");
		async.trigger();
	end

	return 1;
end

function M.parameters()
	return "DEVICE", "BETWEEN";
end

function M.info()
	return {
		name = "toggle",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;