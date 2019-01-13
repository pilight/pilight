--
-- Copyright (C) 2019 CurlyMo & Niek
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local M = {}

function M.check(parameters)
	
	if parameters['TOPIC'] == nil then
		error("mqtt action is missing a \"TOPIC\" statement");
	end

	if #parameters['TOPIC']['value'] ~= 1 or parameters['TOPIC']['value'][2] ~= nil then
		error("mqtt action \"TOPIC\" only takes one argument");
	end
	
	if parameters['MESSAGE'] == nil then
		error("mqtt action is missing a \"MESSAGE\" statement");
	end

	if #parameters['MESSAGE']['value'] ~= 1 or parameters['MESSAGE']['value'][2] ~= nil then
		error("mqtt action \"MESSAGE\" only takes one argument");
	end
	
	if parameters['HOST'] ~= nil and (#parameters['HOST']['value'] ~= 1 or parameters['HOST']['value'][2] ~= nil) then
		error("mqtt action \"HOST\" only takes one argument");
	end

	if parameters['USER'] ~= nil and (#parameters['USER']['value'] ~= 1 or parameters['USER']['value'][2] ~= nil) then
		error("mqtt action \"USER\" only takes one argument");
	end

	if parameters['PASSWORD'] ~= nil and (#parameters['PASSWORD']['value'] ~= 1 or parameters['PASSWORD']['value'][2] ~= nil) then
		error("mqtt action \"USER\" only takes one argument");
	end
	
	if parameters['PORT'] ~= nil then
		if #parameters['PORT']['value'] ~= 1 or parameters['PORT']['value'][2] ~= nil then
			error("mqtt action \"PORT\" only takes one argument");
		else if tonumber(parameters['PORT']['value'][1]) == nil then
			error("mqtt action \"PORT\" must be a number");
		end
	end
		
	end	if parameters['QOS'] ~= nil then
		if #parameters['QOS']['value'] ~= 1 or parameters['QOS']['value'][2] ~= nil then
			error("mqtt action \"QOS\" only takes one argument");
		else if tonumber(parameters['QOS']['value'][1]) < 0 or tonumber(parameters['QOS']['value'][1]) > 1 then
			error("mqtt action: \"QOS\"" .. tostring(parameters['QOS']['value'][1]) .. " is not supported");
			end
		end
	end
		
	return 1;
end

function M.callback(mqtt_publish)
	if mqtt_publish.getStatus() ~= 0 then
		error("mqtt_publish action: failed to publish message \"" .. mqtt_publish.getText() .. "\" for topic \"" .. mqtt_publish.getTopic() .. " with error \"" .. mqtt_publish.getMessage .. "\""); 
	end
end

function M.run(parameters)
	local topic = parameters['TOPIC']['value'][1];
	local message = parameters['MESSAGE']['value'][1];
	local host = "";
	if parameters['HOST'] ~= nil then
		host = parameters['HOST']['value'][1];
	end
	
	local user = "";
	if parameters['USER'] ~= nil then
		user = parameters['USER']['value'][1];
	end

	local password = "";
	if parameters['PASSWORD'] ~= nil then
		password = parameters['PASSWORD']['value'][1];
	end

	local qos = "0";
	local port = "0";
	if parameters['QOS'] ~= nil then
		qos = parameters['QOS']['value'][1];
	end
	if parameters['PORT'] ~= nil then
		port = parameters['PORT']['value'][1];
	end
	
--create pseudo random clientid
	clientid = "action" .. tostring(pilight.common.random(1000000, 9999999));
	
	local mqttobj = pilight.network.mqtt_publish();
	mqttobj.setTopic(topic);
	mqttobj.setQos(tonumber(qos));
	mqttobj.setHost(host);
	mqttobj.setPort(tonumber(port));
	mqttobj.setKeepalive(2); -- let broker disconnect after 2 seconds
	mqttobj.setMessage(message);
	mqttobj.setClientid(clientid);
	mqttobj.setUser(user);
	mqttobj.setPassword(password);
	mqttobj.setCallback("callback");
	
	mqttobj.send();

	return 1;
end

function M.parameters()
	return "TOPIC", "MESSAGE", "QOS", "HOST", "PORT", "USER", "PASSWORD";
end

function M.info()
	return {
		name = "mqtt_publish",
		version = "1.0",
		reqversion = "8.0",
		reqcommit = ""
	}
end

return M;