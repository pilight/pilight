-- Copyright (C) 2013 - 2016 CurlyMo

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

local json = require "json";

local M = {}

function M.read(f)
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
	local registry = jobject['registry'];

	data['registry'] = registry;

	return 1;
end

function M.set(key, val)
	if key == nil then
		return;
	end

	local config = pilight.config();
	local data = config.getData();

	local tmp = data['registry'];
	local keys = pilight.common.explode(key, ".");
	local key = nil;

	if #keys == 1 then
		key = keys[1];
	else
		for i = 1, #keys-1, 1
		do
			tmp[keys[i]] = {};
			tmp = tmp[keys[i]];
			key = keys[i+1];
		end
	end

	tmp[key] = val;

	return 0;
end

function M.get(key)
	if key == nil then
		return;
	end

	local config = pilight.config();
	local data = config.getData();

	local tmp = data['registry'];
	local keys = pilight.common.explode(key, ".");
	local key = nil;

	if #keys == 1 then
		key = keys[1];
	else
		for i = 1, #keys-1, 1
		do
			tmp = tmp[keys[i]];
			key = keys[i+1];
		end
	end

	return tmp[key];
end

function M.write()
	local config = pilight.config();
	local data = config.getData();

	if data ~= nil and data['registry'] ~= nil then
		return json.stringify(data['registry']);
	else
		return nil;
	end
end

function M.info()
	return {
		name = "registry",
		version = "1.0",
		reqversion = "5.0",
		reqcommit = "87"
	}
end

return M;
