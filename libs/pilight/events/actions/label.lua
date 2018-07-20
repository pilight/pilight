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
	local nr1 = 0;
	local nr2 = 0;
	local nr3 = 0;
	local nr4 = 0;
	local nr5 = 0;

	if parameters['DEVICE'] == nil then
		error("label action is missing a \"DEVICE\" statement");
	end

	if parameters['TO'] == nil then
		error("label action is missing a \"TO ...\" statement");
	end

	if #parameters['TO']['value'] ~= 1 or parameters['TO']['value'][2] ~= nil then
		error("label action \"TO\" only takes one argument");
	end

	nr1 = parameters['DEVICE']['order'];
	nr2 = parameters['TO']['order'];

	if parameters['COLOR'] ~= nil then
		nr5 = parameters['COLOR']['order'];
	end

	if parameters['FOR'] ~= nil then
		if #parameters['FOR']['value'] ~= 1 or parameters['FOR']['value'][2] ~= nil or parameters['FOR']['value'][1] == nil then
			error("label action \"FOR\" only takes one argument");
		else
			local array = pilight.common.explode(parameters['FOR']['value'][1], " ");
			if #array ~= 2 then
				error("label action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
			if units[array[2]] ~= true then
				error("label action \"" .. array[2] .. "\" is not a valid unit");
			end
			if tonumber(array[1]) <= 0 then
				error("label action \"FOR\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
		end
		nr3 = parameters['FOR']['order'];

		if nr3 < nr2 then
			error("label actions are formatted as \"label DEVICE ... TO ... FOR ...\"");
		end
		if nr5 > 0 and nr3 < nr5 then
			error("label actions are formatted as \"label DEVICE ... TO ... COLOR ... FOR ...\"");
		end
	end

	if parameters['AFTER'] ~= nil then
		if #parameters['AFTER']['value'] ~= 1 or parameters['AFTER']['value'][2] ~= nil or parameters['AFTER']['value'][1] == nil then
			error("label action \"AFTER\" only takes one argument");
		else
			local array = pilight.common.explode(parameters['AFTER']['value'][1], " ");
			if #array ~= 2 then
				error("label action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
			if units[array[2]] ~= true then
				error("label action \"" .. array[2] .. "\" is not a valid unit");
			end
			if tonumber(array[1]) <= 0 then
				error("label action \"AFTER\" requires a positive number and a unit e.g. \"1 MINUTE\"");
			end
		end
		nr4 = parameters['AFTER']['order'];

		if nr4 < nr2 then
			error("label actions are formatted as \"label DEVICE ... TO ... AFTER ...\"");
		end
		if nr5 > 0 and nr4 < nr5 then
			error("label actions are formatted as \"label DEVICE ... TO ... COLOR ... AFTER ...\"");
		end
	end

	if nr1 ~= 1 or nr2 ~= 2 then
		error("label actions are formatted as \"label DEVICE ... TO ...\"");
	end

	if nr5 > 0 and nr5 ~= 3 then
		error("label actions are formatted as \"label DEVICE ... TO ... COLOR ...\"");
	end

	local nrdev = #parameters['DEVICE']['value'];
	for i = 1, nrdev, 1 do
		local dev = pilight.config.device(parameters['DEVICE']['value'][i]);
		if dev == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" does not exist");
		end
		if dev.getLabel == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be labeled \"" .. parameters['TO']['value'][1] .. "\"");
		end
		if dev.getColor == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be colored \"" .. parameters['COLOR']['value'][1] .. "\"");
		end
	end

	return 1;
end

function M.timer_for(timer)
	local data = timer.getUserdata();
	local devname = data['device'];
	local devobj = pilight.config.device(devname);

	if devobj.setColor(data['old_color']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_color'] .. "\"")
	end

	if devobj.setLabel(data['old_label']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_label'] .. "\"")
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
	local devobj = pilight.config.device(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action label for device " .. devname);
	end

	if data['new_color'] ~= nil then
		if devobj.setColor(data['new_color']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_color'] .. "\"")
		end
	end

	if data['new_label'] ~= nil then
		if devobj.setLabel(data['new_label']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_label'] .. "\"")
		end
	end

	devobj.send();

	if data['time_for'] ~= 0 and data['type_for'] ~= nil then
		execute_for(data);
	end
end

function M.timer_after(timer)
	local data = timer.getUserdata();
	local devname = data['device'];
	local devobj = pilight.config.device(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action label for device " .. devname);
	end

	if data['new_color'] ~= nil then
		if devobj.setColor(data['new_color']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_color'] .. "\"")
		end
	end

	if data['new_label'] ~= nil then
		if devobj.setLabel(data['new_label']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_label'] .. "\"")
		end
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
		local devobj = pilight.config.device(devname);
		local old_color = nil;
		local new_color = nil;
		local old_label = nil;
		local new_label = nil;
		local after = nil;
		local for_ = nil;
		local async = nil;

		if parameters['COLOR'] ~= nil then
			if #parameters['COLOR']['value'] == 1 then
				new_color = parameters['COLOR']['value'][1];
			end
		end

		if parameters['TO'] ~= nil then
			if #parameters['TO']['value'] == 1 then
				new_label = parameters['TO']['value'][1];
			end
		end

		if parameters['AFTER'] ~= nil then
			after = pilight.common.explode(parameters['AFTER']['value'][1], " ");
		end
		if parameters['FOR'] ~= nil then
			for_ = pilight.common.explode(parameters['FOR']['value'][1], " ");
		end

		if devobj.hasSetting("label") == true then
			if devobj.getLabel ~= nil then
				old_label = devobj.getLabel();
			end
		end

		if devobj.hasSetting("color") == true then
			if devobj.getColor ~= nil then
				old_color = devobj.getColor();
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
		data['old_label'] = old_label;
		data['new_label'] = new_label;
		data['old_color'] = old_color;
		data['new_color'] = new_color;
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
	return "DEVICE", "TO", "AFTER", "FOR", "COLOR";
end

function M.info()
	return {
		name = "label",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;