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
		error("pushbullet action is missing a \"TITLE\" statement");
	else
		if (#parameters['TITLE']['value'] ~= 1 or parameters['TITLE']['value'][2] ~= nil) then
			error("pushbullet action \"TITLE\" only takes one argument");
		end
	end

	if parameters['BODY'] == nil then
		error("pushbullet action is missing a \"BODY\" statement");
	else
		if (#parameters['BODY']['value'] ~= 1 or parameters['BODY']['value'][2] ~= nil) then
			error("pushbullet action \"BODY\" only takes one argument");
		end
	end

	if parameters['TOKEN'] == nil then
		error("pushbullet action is missing a \"TOKEN\" statement");
	else
		if (#parameters['TOKEN']['value'] ~= 1 or parameters['TOKEN']['value'][2] ~= nil) then
			error("pushbullet action \"TOKEN\" only takes one argument");
		end
	end

	return 1;
end

function M.callback(http)
	if http.getCode() == 200 then
		error("pushbullet action succeeded with message \"" .. http.getData() .. "\"");
	else
		error("pushbullet action failed (" .. http.getCode() .. ") with message \" " .. http.getData() .. "\"");
	end
end

function M.run(parameters)
	local httpobj = pilight.network.http();
	local url = "https://" .. parameters['TOKEN']['value'][1] .. "@api.pushbullet.com/v2/pushes";

	--
	-- Allow overriding the url for unittesting purposes
	--
	if parameters['URL']['value'][1] ~= nil then
		url = parameters['URL']['value'][1];
	end

	httpobj.setCallback("callback");
	httpobj.setUrl(url);
	httpobj.setMimetype("application/json");
	httpobj.setData("{\"type\": \"note\", \"title\": \"" .. parameters['TITLE']['value'][1] .."\", \"body\": \"" .. parameters['BODY']['value'][1] .. "\"}");
	httpobj.post();

	return 1;
end

function M.parameters()
	return "TITLE", "BODY", "TOKEN";
end

function M.info()
	return {
		name = "pushbullet",
		version = "1.0",
		reqversion = "7.0",
		reqcommit = "380"
	}
end

return M;
