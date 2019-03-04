--
-- Copyright (C) 2018 CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local M = {}

local units = {
	MILLISECOND = true,
	SECOND = true,
	MINUTE = true,
	HOUR = true,
	DAY = true
};

function M.check(parameters)
	if parameters['DEVICE'] == nil then
		error("switch action is missing a \"DEVICE\" statement");
	end

	if parameters['TO'] == nil then
		error("switch action is missing a \"TO ...\" statement");
	end

	if parameters['DEVICE']['order'] ~= 1 or parameters['TO']['order'] ~= 2 then
		error("switch actions are formatted as \"switch DEVICE ... TO ...\"");
	end

	if #parameters['TO']['value'] ~= 1 or parameters['TO']['value'][2] ~= nil then
		error("switch action \"TO\" only takes one argument");
	end

	if parameters['FROM'] ~= nil and (#parameters['FROM']['value'] ~= 1 or parameters['FROM']['value'][2] ~= nil) then
		error("switch action \"FROM\" only takes one argument");
	end

	if parameters['FOR'] ~= nil then
		if #parameters['FOR']['value'] ~= 1 or parameters['FOR']['value'][2] ~= nil or parameters['FOR']['value'][1] == nil then
			error("switch action \"FOR\" only takes one argument");
		else
			local array = pilight.common.explode(parameters['FOR']['value'][1], " ");
			if #array ~= 2 then
				error("switch action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
			if units[array[2]] ~= true then
				error("switch action \"" .. array[2] .. "\" is not a valid unit");
			end
			if tonumber(array[1]) <= 0 then
				error("switch action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
		end
	end

	if parameters['AFTER'] ~= nil then
		if #parameters['AFTER']['value'] ~= 1 or parameters['AFTER']['value'][2] ~= nil or parameters['AFTER']['value'][1] == nil then
			error("switch action \"AFTER\" only takes one argument");
		else
			local array = pilight.common.explode(parameters['AFTER']['value'][1], " ");
			if #array ~= 2 then
				error("switch action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
			if units[array[2]] ~= true then
				error("switch action \"" .. array[2] .. "\" is not a valid unit");
			end
			if tonumber(array[1]) <= 0 then
				error("switch action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
		end
	end

	if (parameters['FROM'] ~= nil and parameters['FOR'] == nil) then
		error("switch action \"FROM\" can only be combined with the \"FOR\" parameter");
	end

	local config = pilight.config();
	local nrdev = #parameters['DEVICE']['value'];
	for i = 1, nrdev, 1 do
		local dev = config.getDevice(parameters['DEVICE']['value'][i]);
		if dev == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" does not exist");
		end
		if dev.hasState ~= nil and dev.setState ~= nil then
			if dev.hasState(parameters['TO']['value'][1]) == false then
				error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be set to state \"" .. parameters['TO']['value'][1] .. "\"");
			end
			if parameters['FROM'] ~= nil and dev.hasState(parameters['FROM']['value'][1]) == false then
				error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be set to state \"" .. parameters['FROM']['value'][1] .. "\"");
			end
		else
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be set to state \"" .. parameters['TO']['value'][1] .. "\"");
		end
	end

	return 1;
end

function M.timer_for(timer)
	local data = timer.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action switch for device " .. devname);
	end

	if devobj.setState(data['old_state']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['old_state'] .. "\"")
	end

	devobj.send();

end

function execute_for(data)
	local async = pilight.async.timer();

	if(data['type_for'] == 'SECOND') then
		async.setTimeout(data['time_for']*1000);
	elseif(data['type_for'] == 'MINUTE') then
		async.setTimeout(data['time_for']*1000*60);
	elseif(data['type_for'] == 'HOUR') then
		async.setTimeout(data['time_for']*1000*60*60);
	elseif(data['type_for'] == 'DAY') then
		async.setTimeout(data['time_for']*1000*60*60);
	else
		async.setTimeout(data['time_for']);
	end

	async.setUserdata(data());
	async.setRepeat(0);
	async.setCallback("timer_for");
	async.start();
end

function M.thread(thread)
	local data = thread.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action switch for device " .. devname);
	end

	if devobj.setState(data['new_state']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_state'] .. "\"")
	end

	devobj.send();

	if data['time_for'] ~= 0 and data['type_for'] ~= nil then
		execute_for(data);
	end
end

function M.timer_after(timer)
	local data = timer.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action switch for device " .. devname);
	end

	if devobj.setState(data['new_state']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_state'] .. "\"")
	end

	devobj.send();

	if data['time_for'] > 0 and data['type_for'] ~= nil then
		execute_for(data);
	end
end

function M.run(parameters)
	local nrdev = #parameters['DEVICE']['value'];

	for i = 1, nrdev, 1 do
		local devname = parameters['DEVICE']['value'][i];
		local config = pilight.config();
		local devobj = config.getDevice(devname);
		local old_state = nil;
		local new_state = parameters['TO']['value'][1];
		local after = nil;
		local for_ = nil;
		local async = nil;

		if parameters['AFTER'] ~= nil then
			after = pilight.common.explode(parameters['AFTER']['value'][1], " ");
		end
		if parameters['FOR'] ~= nil then
			for_ = pilight.common.explode(parameters['FOR']['value'][1], " ");
		end

		if parameters['FROM'] ~= nil then
			old_state = parameters['FROM']['value'][1];
		else
			if devobj.hasSetting("state") == true then
				if devobj.getState ~= nil then
					old_state = devobj.getState();
				end
			end
		end

		if after ~= nil and #after == 2 then
			async = pilight.async.timer();
		else
			async = pilight.async.thread();
		end

		local data = async.getUserdata();

		data['device'] = devname;
		data['time_after'] = 0;
		data['type_after'] = nil;
		data['time_for'] = 0;
		data['type_for'] = nil;
		data['steps'] = 1;
		data['old_state'] = old_state;
		data['new_state'] = new_state;
		data['action_id'] = devobj.setActionId();

		if for_ ~= nil and #for_ == 2 then
			data['time_for'] = tonumber(for_[1]);
			data['type_for'] = for_[2];
		end

		if after ~= nil and #after == 2 then
			data['time_after'] = tonumber(after[1]);
			data['type_after'] = after[2];
		end

		if data['time_after'] > 0 and data['type_after'] ~= nil then
			if(data['type_after'] == 'SECOND') then
				async.setTimeout(data['time_after']*1000);
			elseif(data['type_after'] == 'MINUTE') then
				async.setTimeout(data['time_after']*1000*60);
			elseif(data['type_after'] == 'HOUR') then
				async.setTimeout(data['time_after']*1000*60*60);
			elseif(data['type_after'] == 'DAY') then
				async.setTimeout(data['time_after']*1000*60*60);
			else
				async.setTimeout(data['time_after']);
			end

			async.setRepeat(0);
			async.setCallback("timer_after");
			async.start();
		else
			async.setCallback("thread");
			async.trigger();
		end
	end

	return 1;
end

function M.parameters()
	return "DEVICE", "TO", "FOR", "AFTER", "FROM";
end

function M.info()
	return {
		name = "switch",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;