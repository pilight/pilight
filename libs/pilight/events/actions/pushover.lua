--
-- Copyright (C) 2018 CurlyMo & Niek
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local M = {}

function M.check(parameters)
	if parameters['TITLE'] == nil then
		error("pushover action is missing a \"TITLE\" statement");
	else
		if (#parameters['TITLE']['value'] ~= 1 or parameters['TITLE']['value'][2] ~= nil) then
			error("pushover action \"TITLE\" only takes one argument");
		end
	end

	if parameters['MESSAGE'] == nil then
		error("pushover action is missing a \"MESSAGE\" statement");
	else
		if (#parameters['MESSAGE']['value'] ~= 1 or parameters['MESSAGE']['value'][2] ~= nil) then
			error("pushover action \"MESSAGE\" only takes one argument");
		end
	end

	if parameters['TOKEN'] == nil then
		error("pushover action is missing a \"TOKEN\" statement");
	else
		if (#parameters['TOKEN']['value'] ~= 1 or parameters['TOKEN']['value'][2] ~= nil) then
			error("pushover action \"TOKEN\" only takes one argument");
		end
	end

	if parameters['USER'] == nil then
		error("pushover action is missing a \"USER\" statement");
	else
		if (#parameters['USER']['value'] ~= 1 or parameters['USER']['value'][2] ~= nil) then
			error("pushover action \"USER\" only takes one argument");
		end
	end

	return 1;
end

function M.callback(http)
	if http.getCode() == 200 then
		error("pushover action succeeded with message \" " .. http.getData() .. "\"");
	else
		error("pushover action failed (" .. http.getCode() .. ") with message \" " .. http.getData() .. "\"");
	end
end

function M.run(parameters)
	local httpobj = pilight.network.http();
	httpobj.setCallback("callback");
	httpobj.setUrl("https://api.pushover.net/1/messages.json");
	httpobj.setMimetype("application/x-www-form-urlencoded");
	httpobj.setData("token=" .. parameters['TOKEN']['value'][1] .. "&user=" .. parameters['USER']['value'][1] .. "&title=" .. parameters['TITLE']['value'][1] .. "&message=" .. parameters['MESSAGE']['value'][1]);
	httpobj.post();

	return 1;
end

function M.parameters()
	return "TITLE", "MESSAGE", "USER", "TOKEN";
end

function M.info()
	return {
		name = "pushover",
		version = "1.0",
		reqversion = "8.1.2",
		reqcommit = "23"
	}
end

return M;
