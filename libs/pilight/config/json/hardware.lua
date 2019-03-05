-- Copyright (C) 2013 - 2016 CurlyMo

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

local json = require "json";
local dump = require "dump";

local M = {}

function firstToUpper(str)
    return (str:gsub("^%l", string.upper))
end

function M.read(f)
	if f == nil then
		return 0;
	end

	local file = pilight.io.file(f);

	file.open("r");
	local content = '';
	for line in file.readline() do
		content = content .. line;
	end
	file.close();

	local config = pilight.config();
	local data = config.getData();
	local jobject = json.parse(content);
	local hardware = jobject['hardware'];

	data['hardware'] = hardware;

	if hardware ~= nil then
		for k, v in pairs(hardware) do
			local obj = config.getHardware(k);
			if obj == nil then
				return 0;
			end
			if obj.validate() == false then
				return 0;
			end
		end;
	end;

	return 1;
end

function M.write()
	return nil;
end

function M.info()
	return {
		name = "hardware",
		version = "1.0",
		reqversion = "5.0",
		reqcommit = "87"
	}
end

return M;
