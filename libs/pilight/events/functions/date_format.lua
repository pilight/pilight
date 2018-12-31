-- Copyright (C) 2013 - 2016 CurlyMo

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

local M = {}

function M.run(a, b, c, d)
	local tm = nil;
	local e = nil;

	if d ~= nil then
		error("DATE_FORMAT requires no more than three arguments");
	elseif a ~= nil and b ~= nil and c ~= nil then
		e = c;
		tm = pilight.datetime.strptime(a, b);
	elseif a ~= nil and b ~= nil then
		e = b;
		local config = pilight.config();
		local dev = config.getDevice(a);
		if dev == nil then
			error(string.format("DATE_FORMAT device \"%s\" is not a datetime protocol", a));
		else
			local match = 0;
			local types = dev.getType();
			for i = 1, #types, 1 do
				if types[i] == 8 then
					match = 1;
					break;
				end
			end
			if match == 0 then
				error(string.format("DATE_FORMAT device \"%s\" is not a datetime protocol", a));
			end
			local datetime = dev.getTable();
			tm = pilight.datetime.strptime(
				datetime['year'] .. '-' ..
				datetime['month'] .. '-' ..
				datetime['day'] .. ' ' ..
				datetime['hour'] .. ':' ..
				datetime['minute'] .. ':' ..
				datetime['second'],
				"%Y-%m-%d %H:%M:%S");
		end
	else
		error("DATE_FORMAT requires at least two arguments");
	end
	local t = os.time(tm);
	return os.date(e, t);
end


function M.info()
	return {
		name = "DATE_FORMAT",
		version = "1.0",
		reqversion = "6.0",
		reqcommit = "94"
	}
end

return M;