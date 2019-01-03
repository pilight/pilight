--
-- Copyright (C) 2019 CurlyMo & Niek
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
	local nr6 = 0;
	local nr7 = 0;

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
		if #parameters['COLOR']['value'] ~= 1 or parameters['COLOR']['value'][2] ~= nil then
			error("label action \"COLOR\" only takes one argument");
		end
		nr5 = parameters['COLOR']['order'];
	end

	if parameters['BGCOLOR'] ~= nil then
		if #parameters['BGCOLOR']['value'] ~= 1 or parameters['BGCOLOR']['value'][2] ~= nil then
			error("label action \"BGCOLOR\" only takes one argument");
		end
		nr6 = parameters['BGCOLOR']['order'];
	end

	if parameters['BLINK'] ~= nil then
		if #parameters['BLINK']['value'] ~= 1 or parameters['BLINK']['value'][2] ~= nil then
			error("label action \"BLINK\" only takes one argument");
		end
		if parameters['BLINK']['value'][1] ~= "on" and parameters['BLINK']['value'][1] ~= "off" then
			error("label action \"BLINK\" must be \"on\" or \"off\", \"" .. parameters['BLINK']['value'][1] .. "\" given" );		
		end
		nr7   = parameters['BLINK']['order'];
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
			return -1;
		end
		if nr5 > 0 and nr3 < nr5 then
			error("label actions are formatted as \"label DEVICE ... TO ... COLOR ... FOR ...\"");
			return -1;
		end
		if nr6 > 0 and nr3 < nr6 then
			error("label actions are formatted as \"label DEVICE ... TO ... BGCOLOR ... FOR ...\"");
			return -1;
		end
		if nr7 > 0 and nr3 < nr7 then
			error("label actions are formatted as \"label DEVICE ... TO ... BLINK ... FOR ...\"");
			return -1;
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
		if nr6 > 0 and nr4 < nr6 then
			error("label actions are formatted as \"label DEVICE ... TO ... BGCOLOR ... AFTER ...\"");
		end
		if nr7 > 0 and nr4 < nr7 then
			error("label actions are formatted as \"label DEVICE ... TO ... BLINK ... AFTER ...\"");
		end
	end

	if nr1 ~= 1 or nr2 ~= 2 then
		error("label actions are formatted as \"label DEVICE ... TO ...\"");
	end

	if nr5 > 0 and (nr5 < 3 or nr5 > 5) then
		error("label actions are formatted as \"label DEVICE ... TO ... COLOR ...\"");
	end

	if nr6> 0 and (nr6 < 3 or nr6 > 5) then
		error("label actions are formatted as \"label DEVICE ... TO ... BGCOLOR ...\"");
	end

	if nr7 > 0 and (nr7 < 3 or nr7 > 5) then
		error("label actions are formatted as \"label DEVICE ... TO ... BLINK ...\"");
	end

	local nrdev = #parameters['DEVICE']['value'];
	local config = pilight.config();

	for i = 1, nrdev, 1 do

		local dev = config.getDevice(parameters['DEVICE']['value'][i]);
		if dev == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" does not exist");
		end
		if dev.getLabel == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be labeled \"" .. parameters['TO']['value'][1] .. "\"");
		end
		if dev.getColor == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't be colored \"" .. parameters['COLOR']['value'][1] .. "\"");
		end
		if dev.getBgcolor == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't have background color \"" .. parameters['BGCOLOR']['value'][1] .. "\"");
		end
		if dev.getBlink == nil then
			error("device \"" .. parameters['DEVICE']['value'][i] .. "\" can't have blinking \"" .. parameters['BLINK']['value'][1] .. "\"");
		end
	end

	return 1;
end

function M.timer_for(timer)
	local data = timer.getUserdata();
	local devname = data['device'];
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if devobj.setColor(data['old_color']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_color'] .. "\"")
	end

	if devobj.setLabel(data['old_label']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_label'] .. "\"")
	end

	if devobj.setLabel(data['old_bgcolor']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_bgcolor'] .. "\"")
	end

	if devobj.setLabel(data['old_blink']) == false then
		error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_blink'] .. "\"")
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
		error("skipping overridden action label for device " .. devname);
		return;
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

	if data['new_bgcolor'] ~= nil then
		if devobj.setBgcolor(data['new_bgcolor']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_bgcolor'] .. "\"")
		end
	end

	if data['new_blink'] ~= nil then
		if devobj.setBlink(data['new_blink']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_blink'] .. "\"")
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
	local config = pilight.config();
	local devobj = config.getDevice(devname);

	if(devobj.getActionId() ~= data['action_id']) then
		error("skipping overridden action label for device " .. devname);
		return;
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

	if data['new_bgcolor'] ~= nil then
		if devobj.setBgcolor(data['new_bgcolor']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_bgcolor'] .. "\"")
		end
	end

	if data['new_blink'] ~= nil then
		if devobj.setBlink(data['new_blink']) == false then
			error("device \"" .. devname .. "\" could not be set to state \"" .. data['new_blink'] .. "\"")
		end
	end
	
	devobj.send();

	if data['time_for'] > 0 and data['type_for'] ~= nil then
		execute_for(data);
	end
end

function M.run(parameters)
	local nrdev = #parameters['DEVICE']['value'];
	local config = pilight.config();

	for i = 1, nrdev, 1 do
		local devname = parameters['DEVICE']['value'][i];
		local devobj = config.getDevice(devname);
		local old_color = nil;
		local new_color = nil;
		local old_label = nil;
		local new_label = nil;
		local old_bgcolor = nil;
		local new_bgcolor = nil;
		local old_blink = nil;
		local new_blink = nil;
		local after = nil;
		local for_ = nil;
		local async = nil;

		if parameters['COLOR'] ~= nil then
			if #parameters['COLOR']['value'] == 1 then
				new_color = parameters['COLOR']['value'][1];
			end
		end

		if parameters['BGCOLOR'] ~= nil then
			if #parameters['BGCOLOR']['value'] == 1 then
				new_bgcolor = parameters['BGCOLOR']['value'][1];
			end
		end

		if parameters['BLINK'] ~= nil then
			if #parameters['BLINK']['value'] == 1 then
				new_blink = parameters['BLINK']['value'][1];
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
		if devobj.hasSetting("bgcolor") == true then
			if devobj.getBgcolor ~= nil then
				old_bgcolor = devobj.getBgcolor();
			end
		end

		if devobj.hasSetting("blink") == true then
			if devobj.getBlink ~= nil then
				old_blink = devobj.getBlink();
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
		data['old_bgcolor'] = old_bgcolor;
		data['new_bgcolor'] = new_bgcolor;
		data['old_blink'] = old_blink;
		data['new_blink'] = new_blink;
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
	return "DEVICE", "TO", "COLOR", "BGCOLOR", "BLINK", "AFTER", "FOR"
end

function M.info()
	return {
		name = "label",
		version = "5.0",
		reqversion = "8.1.3",
		reqcommit = "0"
	}
end

return M;