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
	local nr1 = -1;
	local nr2 = -1;
	local nr3 = -1;
	local nr4 = -1;
	local nr5 = -1;
	local nr6 = -1;

	if parameters['DEVICE'] == nil then
		error("dim action is missing a \"DEVICE\" statement");
	end

	if parameters['TO'] == nil then
		error("dim action is missing a \"TO ...\" statement");
	end

	if #parameters['TO']['value'] ~= 1 or parameters['TO']['value'][2] ~= nil then
		error("dim action \"TO\" only takes one argument");
	end

	if type(tonumber(parameters['TO']['value'][1])) ~= 'number' then
		error("dim action \"TO\" must requires a numeric value got a \"" .. type(parameters['TO']['value'][1]) .. "\"");
	end

	nr1 = parameters['DEVICE']['order'];
	nr2 = parameters['TO']['order'];

	if parameters['FOR'] ~= nil then
		if #parameters['FOR']['value'] ~= 1 or parameters['FOR']['value'][2] ~= nil or parameters['FOR']['value'][1] == nil then
			error("dim action \"FOR\" only takes one argument");
		else
			local array = pilight.common.explode(parameters['FOR']['value'][1], " ");
			if #array ~= 2 then
				error("dim action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
			if units[array[2]] ~= true then
				error("dim action \"" .. array[2] .. "\" is not a valid unit");
			end
			if tonumber(array[1]) <= 0 then
				error("dim action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
		end
		nr3 = parameters['FOR']['order'];

		if nr3 < nr2 then
			error("dim actions are formatted as \"dim DEVICE ... TO ... FOR ...\"");
		end
	end

	if parameters['AFTER'] ~= nil then
		if #parameters['AFTER']['value'] ~= 1 or parameters['AFTER']['value'][2] ~= nil or parameters['AFTER']['value'][1] == nil then
			error("dim action \"AFTER\" only takes one argument");
		else
			local array = pilight.common.explode(parameters['AFTER']['value'][1], " ");
			if #array ~= 2 then
				error("dim action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
			if units[array[2]] ~= true then
				error("dim action \"" .. array[2] .. "\" is not a valid unit");
			end
			if tonumber(array[1]) <= 0 then
				error("dim action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
		end
		nr4 = parameters['AFTER']['order'];

		if nr4 < nr2 then
			error("dim actions are formatted as \"dim DEVICE ... TO ... AFTER ...\"");
		end
	end

	if parameters['IN'] ~= nil then
		if #parameters['IN']['value'] ~= 1 or parameters['IN']['value'][2] ~= nil or parameters['IN']['value'][1] == nil then
			error("dim action \"IN\" only takes one argument");
		else
			local array = pilight.common.explode(parameters['IN']['value'][1], " ");
			if #array ~= 2 then
				error("dim action \"IN\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
			if units[array[2]] ~= true then
				error("dim action \"" .. array[2] .. "\" is not a valid unit");
			end
			if tonumber(array[1]) <= 0 then
				error("dim action \"IN\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
		end
		nr6 = parameters['IN']['order'];

		if nr6 < nr2 then
			error("dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
		end
	end

	if parameters['FROM'] ~= nil then
		if #parameters['FROM']['value'] ~= 1 or parameters['FROM']['value'][2] ~= nil then
			error("dim action \"FROM\" only takes one argument");
		end

		if type(tonumber(parameters['FROM']['value'][1])) ~= 'number' then
			error("dim action \"FROM\" must requires a numeric value got a \"" .. type(parameters['FROM']['value'][1]) .. "\"");
		end

		nr5 = parameters['FROM']['order'];

		if nr5 < nr2 then
			error("dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
		end

		if nr6 == -1 then
			error("dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
		end
	else
		if nr6 ~= -1 then
			error("dim actions are formatted as \"dim DEVICE ... TO ... FROM ... IN ...\"");
		end
	end

	if nr1 ~= 1 or nr2 ~= 2 then
		error("dim actions are formatted as \"dim DEVICE ... TO ...\"");
	end

	local config = pilight.config();
	local nrdev = #parameters['DEVICE']['value'];
	for i = 1, nrdev, 1 do
		local dev = config.getDevice(parameters['DEVICE']['value'][i]);
		if dev == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" does not exist");
		end

		if dev.hasDimlevel == nil or dev.setDimlevel == nil or dev.hasDimlevel(tonumber(parameters['TO']['value'][1])) == false then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be set to dimlevel \"" .. parameters['TO']['value'][1] .. "\"");
		end
		if parameters['FROM'] ~= nil then
			if dev.hasDimlevel == nil or dev.setDimlevel == nil or dev.hasDimlevel(tonumber(parameters['FROM']['value'][1])) == false then
				error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be set to dimlevel \"" .. parameters['FROM']['value'][1] .. "\"");
			end
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
		error("skipping overridden action dim for device " .. devname);
		return;
	end

	if devobj.setState("on") == false then
		error("device \"" .. devname .. "\" could not be set to state \"on\"")
	end

	if devobj.setDimlevel(data['old_dimlevel']) == false then
		error("device \"" .. devname .. "\" could not be set to dimlevel \"" .. data['old_dimlevel'] .. "\"")
	end

	devobj.send();

end

function M.execute_for(data)
	if data == nil then
		return;
	end

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

function M.timer_in(timer)
	local data = timer.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if devobj.setState("on") == false then
		error("device \"" .. devname .. "\" could not be set to state \"on\"")
	end

	if devobj.setDimlevel(data['from_dimlevel']) == false then
		error("device \"" .. devname .. "\" could not be set to dimlevel \"" .. data['from_dimlevel'] .. "\"")
	end

	devobj.send();

	if data['direction'] == false then
		data['from_dimlevel'] = data['from_dimlevel'] + 1;
		if (data['from_dimlevel'] - 1) == data['new_dimlevel'] then
			if data['time_for'] ~= 0 and data['type_for'] ~= nil then
				M.execute_for(data);
			end
			timer.stop();
			return;
		end
	else
		data['from_dimlevel'] = data['from_dimlevel'] - 1;
		if (data['from_dimlevel'] + 1) == data['new_dimlevel'] then
			if data['time_for'] ~= 0 and data['type_for'] ~= nil then
				M.execute_for(data);
			end
			timer.stop();
			return;
		end
	end
end

function M.execute_in(data)
	if data == nil then
		return;
	end

	local async = pilight.async.timer();
	if(data['type_in'] == 'SECOND') then
		async.setRepeat(data['time_in']*1000);
		async.setTimeout(data['time_in']*1000);
	elseif(data['type_in'] == 'MINUTE') then
		async.setRepeat(data['time_in']*1000*60);
		async.setTimeout(data['time_in']*1000*60);
	elseif(data['type_in'] == 'HOUR') then
		async.setRepeat(data['time_in']*1000*60*60);
		async.setTimeout(data['time_in']*1000*60*60);
	elseif(data['type_in'] == 'DAY') then
		async.setRepeat(data['time_in']*1000*60*60);
		async.setTimeout(data['time_in']*1000*60*60);
	else
		async.setRepeat(data['time_in']);
		async.setTimeout(data['time_in']);
	end

	async.setUserdata(data());
	async.setCallback("timer_in");
	async.start();
end

function M.thread(thread)
	local data = thread.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action dim for device " .. devname);
	end

	if devobj.setState("on") == false then
		error("device \"" .. devname .. "\" could not be set to state \"on\"")
	end

	if devobj.setDimlevel(data['new_dimlevel']) == false then
		error("device \"" .. devname .. "\" could not be set to dimlevel \"" .. data['new_dimlevel'] .. "\"")
	end

	devobj.send();

	if data['time_for'] ~= 0 and data['type_for'] ~= nil then
		M.execute_for(data);
	end
end

function M.timer_after(timer)
	local data = timer.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action dim for device " .. devname);
	end

	if devobj.setState("on") == false then
		error("device \"" .. devname .. "\" could not be set to state \"on\"")
	end

	if data['time_in'] > 0 and data['type_in'] ~= nil then
		M.execute_in(data);
	else
		if devobj.setDimlevel(data['new_dimlevel']) == false then
			error("device \"" .. devname .. "\" could not be set to dimlevel \"" .. data['new_dimlevel'] .. "\"")
		end

		devobj.send();
		if data['time_for'] > 0 and data['type_for'] ~= nil then
			M.execute_for(data);
		end
	end
end

function M.run(parameters)
	local nrdev = #parameters['DEVICE']['value'];

	for i = 1, nrdev, 1 do
		local devname = parameters['DEVICE']['value'][i];
		local config = pilight.config();
		local devobj = config.getDevice(devname);
		local old_dimlevel = nil;
		local new_dimlevel = parameters['TO']['value'][1];
		local from_dimlevel = nil;
		local after = nil;
		local in_ = nil;
		local for_ = nil;
		local async = nil;

		if parameters['FROM'] ~= nil then
			if #parameters['FROM']['value'] == 1 then
				from_dimlevel = parameters['FROM']['value'][1];
			end
		end

		if parameters['IN'] ~= nil then
			in_ = pilight.common.explode(parameters['IN']['value'][1], " ");
		end
		if parameters['AFTER'] ~= nil then
			after = pilight.common.explode(parameters['AFTER']['value'][1], " ");
		end
		if parameters['FOR'] ~= nil then
			for_ = pilight.common.explode(parameters['FOR']['value'][1], " ");
		end

		if devobj.hasSetting("dimlevel") == true then
			if devobj.getDimlevel ~= nil then
				old_dimlevel = devobj.getDimlevel();
			end
		end

		if in_ ~= nil and #in_ == 2 then
			async = pilight.async.timer();
		elseif after ~= nil and #after == 2 then
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
		data['time_in'] = 0;
		data['type_in'] = nil;
		data['direction'] = nil;
		data['steps'] = 1;
		data['old_dimlevel'] = tonumber(old_dimlevel);
		data['new_dimlevel'] = tonumber(new_dimlevel);
		data['from_dimlevel'] = tonumber(from_dimlevel);
		data['action_id'] = devobj.setActionId();

		if from_dimlevel ~= nil then
			data['direction'] = tonumber(from_dimlevel) > tonumber(new_dimlevel);
		end

		if in_ ~= nil and #in_ == 2 then
			local steps = math.abs(data['from_dimlevel']-data['new_dimlevel']);
			data['time_in'] = tonumber(in_[1])/steps;
			data['type_in'] = in_[2];
		end

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
		elseif data['time_in'] > 0 and data['type_in'] ~= nil then
			M.execute_in(data);
		else
			async.setCallback("thread");
			async.trigger();
		end
	end

	return 1;
end

function M.parameters()
	return "DEVICE", "TO", "FROM", "FOR", "AFTER", "IN";
end

function M.info()
	return {
		name = "dim",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;