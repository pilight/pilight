--
-- Copyright (C) CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local dump = require "dump";

local M = {}

function M.validate()
	return 1;
end

function M.run()
	return 1;
end

function M.implements()
	return pilight.hardware.RFNONE;
end

function M.info()
	return {
		name = "none",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;