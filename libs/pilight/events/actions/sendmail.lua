--
-- Copyright (C) 2018 CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local M = {}

function M.check(parameters)
	if parameters['SUBJECT'] == nil then
		error("sendmail action is missing a \"SUBJECT\" statement");
	end

	if parameters['MESSAGE'] == nil then
		error("sendmail action is missing a \"MESSAGE\" statement");
	end

	if parameters['TO'] == nil then
		error("sendmail action is missing a \"TO\" statement");
	end

	if #parameters['SUBJECT']['value'] ~= 1 or parameters['SUBJECT']['value'][2] ~= nil then
		error("sendmail action \"SUBJECT\" only takes one argument");
	end

	if #parameters['MESSAGE']['value'] ~= 1 or parameters['MESSAGE']['value'][2] ~= nil then
		error("sendmail action \"MESSAGE\" only takes one argument");
	end

	if #parameters['TO']['value'] ~= 1 or parameters['TO']['value'][2] ~= nil then
		error("sendmail action \"TO\" only takes one argument");
	end

	if parameters['MESSAGE']['value'][1] == "." then
		error("sendmail action \"MESSAGE\" cannot be a single dot");
	end

	local address = parameters['TO']['value'][1];
	if string.match(address, "^[%w.]+@[%w]+[.][%w]+[%w]?[%w]?$") ~= address then
		error("sendmail action \"TO\" must contain an e-mail address");
	end

	if pilight.config.setting("smtp-host") == nil then
		error("sendmail: setting \"smtp-host\" is missing in config");
	end

	if pilight.config.setting("smtp-port") == nil then
		error("sendmail: setting \"smtp-port\" is missing in config");
	end

	if pilight.config.setting("smtp-user") == nil then
		error("sendmail: setting \"smtp-user\" is missing in config");
	end

	if pilight.config.setting("smtp-password") == nil then
		error("sendmail: setting \"smtp-password\" is missing in config");
	end

	local address = pilight.config.setting("smtp-sender");
	if address == nil then
		error("sendmail: setting \"smtp-sender\" is missing in config");
	end

	if string.match(address, "^[%w.]+@[%w]+[.][%w]+[%w]?[%w]?$") ~= address then
		error("sendmail action \"FROM\" must contain an e-mail address");
	end

	return 1;
end

function M.callback(mail)
	if mail.getStatus() == true then
		error("successfully sent mail with subject \"" .. mail.getSubject() .. "\" to " .. mail.getTo());
	else
		error("failed sent mail with subject \"" .. mail.getSubject() .. "\" to " .. mail.getTo());
	end
end

function M.run(parameters)
	local from = pilight.config.setting("smtp-sender");
	local to = parameters['TO']['value'][1];
	local subject = parameters['SUBJECT']['value'][1];
	local message = parameters['MESSAGE']['value'][1];
	local host = pilight.config.setting("smtp-host");
	local port = pilight.config.setting("smtp-port");
	local user = pilight.config.setting("smtp-user");
	local password = pilight.config.setting("smtp-password");
	local ssl = pilight.config.setting("smtp-ssl");

	if ssl == nil then
		ssl = 0;
	end

	local mailobj = pilight.network.mail();
	mailobj.setSubject(subject);
	mailobj.setFrom(from);
	mailobj.setTo(to);
	mailobj.setMessage(message);
	mailobj.setHost(host);
	mailobj.setPort(tonumber(port));
	mailobj.setUser(user);
	mailobj.setPassword(password);
	mailobj.setSSL(tonumber(ssl));
	mailobj.setCallback("callback");
	mailobj.send();

	return 1;
end

function M.parameters()
	return "SUBJECT", "MESSAGE", "TO";
end

function M.info()
	return {
		name = "sendmail",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;