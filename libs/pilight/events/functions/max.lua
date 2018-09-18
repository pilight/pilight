-- Copyright (C) 2013 - 2016 CurlyMo

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

local M = {}

function M.run(a, b, c)
	if (a == nil or b == nil) or c ~= nil then
		error("MAX requires two arguments");
	end
	if tonumber(a) == nil then
		error(string.format("MAX argument #1 expected number, \"%s\" given", type(a)));
	end
	if tonumber(b) == nil then
		error(string.format("MAX argument #2 expected number, \"%s\" given", type(b)));
	end

	return math.max(tonumber(a), tonumber(b));
end

function M.info()
	return {
		name = "MAX",
		version = "2.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;