-- Copyright (C) 2013 - 2016 CurlyMo

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

local json = require "json";

local M = {}

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

	local k = nil;
	local s = nil;
	local valid = false;
	local config = pilight.config();
	local data = config.getData();
	local port = -1;
	local http_port = pilight.default.WEBSERVER_HTTP_PORT;
	local https_port = pilight.default.WEBSERVER_HTTPS_PORT;
	local jobject = json.parse(content);
	local settings = jobject['settings'];

	if data == nil then
		error("config data table not properly initialized");
	end

	if settings == nil then
		error('no config settings were given');
	end

	local keys = {
		'port', 'loopback',

		'name', 'adhoc-master', 'adhoc-mode', 'standalone',

		'storage-root', 'protocol-root', 'hardware-root',
		'actions-root', 'functions-root', 'operators-root',

		'smtp-port', 'smtp-user', 'smtp-password', 'smtp-host',
		'smtp-sender', 'smtp-ssl',

		'gpio-platform',

		'firmware-gpio-sck', 'firmware-gpio-mosi', 'firmware-gpio-miso',
		'firmware-gpio-reset',

		'webserver-authentication', 'webserver-http-port', 'webserver-https-port',
		'webserver-enable', 'webserver-cache', 'watchdog-enable', 'webgui-websockets',
		'webserver-root',

		'pid-file', 'pem-file', 'log-file',

		'log-level',

		'arp-timeout', 'arp-interval',

		'ntp-servers',

		'stats-enable',

		'whitelist'
	};

	for k, v in pairs(settings) do
		valid = false;
		for k1, v1 in pairs(keys) do
			if k == v1 then
				valid = true;
			end
		end

		if valid == false then
			error('"' .. k .. '" is not a valid config setting');
		end
	end

	--
	-- These settings should point to
	-- a valid path.
	--
	local keys = {
		'storage-root', 'protocol-root', 'hardware-root',
		'actions-root', 'functions-root', 'operators-root',
		'webserver-root', 'log-file', 'pid-file', 'pem-file' }
	for k, v in pairs(keys) do
		if settings[v] ~= nil then
			s = settings[v];
			if s ~= nil then
				local dir = pilight.io.dir(s:match("(.*[\\/])"));
				if dir.exists() == false then
					dir.close();
					error('config setting "' .. v .. '" must contain a valid path');
				end
				dir.close();
			end
		end
	end

	--
	-- These settings should be a valid positive number
	--
	keys = { 'port', 'arp-timeout', 'arp-interval', 'smtp-port' }
	for k, v in pairs(keys) do
		if settings[v] ~= nil then
			s = settings[v];
			if type(tonumber(s)) ~= 'number' or tonumber(s) < 0 then
				error('config setting "' .. v .. '" must contain a number larger than 0');
			end
		end
	end

	--
	-- These settings should be a valid string
	--
	keys = { 'smtp-user', 'smtp-password', 'smtp-host', 'name', 'adhoc-master' }
	for k, v in pairs(keys) do
		if settings[v] ~= nil then
			s = settings[v];
			if type(s) ~= 'string' or #settings[v] <= 0 then
				error('config setting "' .. v .. '" must contain a valid string');
			end
		end
	end

	keys = { 'name', 'adhoc-master' }
	for k, v in pairs(keys) do
		if settings[v] ~= nil and #settings[v] > 16 then
			error('config setting "' .. v .. '" can not contain more than 16 characters');
		end
	end

	v = 'adhoc-mode';
	if settings[v] ~= nil then
		s = settings[v];
		if type(s) ~= 'string' or (s ~= 'server' and s ~= 'client') then
			error('config setting "' .. v .. ' must be either "server" or "client"');
		end
	end

	--
	-- These settings should be either 0 or 1
	--
	keys = {
		'standalone', 'watchdog-enable', 'stats-enable', 'loopback',
		'webserver-enable', 'webserver-cache', 'webgui-websockets', 'smtp-ssl' }
	for k, v in pairs(keys) do
		if settings[v] ~= nil then
			s = settings[v];
			if type(tonumber(s)) ~= 'number' or (tonumber(s) ~= 0 and tonumber(s) ~= 1) then
				error('config setting "' .. v .. '" must be either 0 or 1');
			end
		end
	end

	v = 'log-level';
	if settings[v] ~= nil then
		s = settings[v];
		if type(tonumber(s)) ~= 'number' or tonumber(s) < 0 or tonumber(s) > 6 then
			error('config setting "' .. v .. '" must be from 0 till 6');
		end
	end

	v = 'webserver-authentication';
	if settings[v] ~= nil then
		if type(settings[v]) ~= 'table' or settings[v].__len() ~= 2 then
			error('config setting "' .. v .. '" must be in the format of [ "username", "password" ]');
		end
		if type(settings[v][1]) ~= 'string' or type(settings[v][2]) ~= 'string' then
			error('config setting "' .. v .. '" must be in the format of [ "username", "password" ]');
		end
	end

	--
	-- This should receive a url validitor regex
	--
	-- Including smtp-host
	--
	v = 'ntp-servers';
	if settings[v] ~= nil then
		if type(settings[v]) ~= 'table' or settings[v].__len() == 0 then
			error('config setting "' .. v .. '" must be in the format of [ \"0.eu.pool.ntp.org\", ... ]');
		end
		if type(settings[v]) == 'table' then
			for k, x in pairs(settings[v]) do
				if type(x) ~= 'string' or type(x) ~= 'string' then
					error('config setting "' .. v .. '" must be in the format of [ \"0.eu.pool.ntp.org\", ... ]');
				end
			end
		else
			error('config setting "' .. v .. '" must be in the format of [ \"0.eu.pool.ntp.org\", ... ]');
		end
	end

	v = 'smtp-host'
	if settings[v] ~= nil then
		s = settings[v];
		if type(s) ~= 'string' then
			error('config setting "' .. v .. '" must contain a valid string');
		end
	end

	--
	-- These settings should contain a valid ip address
	--
	v = 'whitelist'
	if settings[v] ~= nil then
		s = settings[v]
		if type(s) == 'string' then
			for k, x in pairs(pilight.common.explode(s, ',')) do
				x = x:gsub("%s+", "");
				if type(x) == 'string' then
					local ip4 = {x:match("([%d*]+)%.([%d*]+)%.([%d*]+)%.([%d*]+)")};
					local ip6 = x:match("^([a-fA-F0-9:]+)$");
					if ip4 ~= nil and #ip4 == 4 then
						for _, y in pairs(ip4) do
							if y ~= '*' and (tonumber(y) > 255 or tonumber(y) < 0) then
								error('config setting "' .. v .. '" must contain a valid ip address');
							end
						end
					elseif ip6 ~= nil and #ip6 > 0 then
						local nc, dc = 0, false;
						for chunk, colons in ip6:gmatch("([^:]*)(:*)") do
							if nc > (dc and 7 or 8) then
								error('config setting "' .. v .. '" must contain a valid ip address');
							end
							if #chunk > 0 and tonumber(chunk, 16) > 65535 then
								error('config setting "' .. v .. '" must contain a valid ip address');
							end
							if #colons > 0 then
								if #colons > 2 then
									error('config setting "' .. v .. '" must contain a valid ip address');
								end
								if #colons == 2 and dc == true then
									error('config setting "' .. v .. '" must contain a valid ip address');
								end
								if #colons == 2 and dc == false then dc = true end
							end
							nc = nc + 1
						end
					else
						error('config setting "' .. v .. '" must contain a valid ip address');
					end
				else
					error('config setting "' .. v .. '" must contain a valid ip address');
				end
			end
		else
			error('config setting "' .. v .. '" must contain a valid ip address');
		end
	end

	--
	-- These settings should contain a valid mail adres
	--
	v = 'smtp-sender';
	if settings[v] ~= nil then
		if type(settings[v]) ~= 'string' then
			error('config setting "' .. v .. '" must contain a valid e-mail address');
		elseif string.match(settings[v], "^[%w.]+@[%w]+[.][%w]+[%w]?[%w]?$") ~= settings[v] then
			error('config setting "' .. v .. '" must contain a valid e-mail address');
		end
	end

	if settings['port'] ~= nil then
		port = settings['port'];
	end

	if settings['webserver-http-port'] ~= nil then
		http_port = settings['webserver-http-port'];
	end

	if settings['webserver-https-port'] ~= nil then
		https_port = settings['webserver-https-port'];
	end

	if http_port == port then
		error("config setting \"webserver-http-port\" and \"port\" cannot be the same");
	end

	if https_port == port then
		error("config setting \"webserver-https-port\" and \"port\" cannot be the same");
	end

	if https_port == http_port then
		error("config setting \"webserver-http-port\" and \"webserver-https-port\" cannot be the same");
	end

	--
	-- Check for sendmail required settings
	--
	keys = { 'smtp-host', 'smtp-port', 'smtp-user', 'smtp-password', 'smtp-sender' }
	local nrkeys = 0;
	for k, v in pairs(keys) do
		if settings[v] ~= nil then
			s = settings[v];
			if type(s) == 'string' and #s > 0 then
				nrkeys = nrkeys + 1;
			elseif type(s) == 'number' then
				nrkeys = nrkeys + 1;
			end
		end
	end

	if nrkeys > 0 and nrkeys ~= 5 then
		error('config setting "' .. v .. '" must be set in combination with the smtp-host, smtp-port, smtp-user, smtp-password and smtp-sender settings');
	end

	local wx = nil;
	v = 'gpio-platform';
	if settings[v] ~= nil then
		s = settings[v];
		if type(v) ~= 'string' then
		end
		if s ~= "none" then
			wx = wiringX.setup(s);
			if type(wx) ~= 'table' then
				error('config setting "' .. v .. '" must contain a supported gpio platform');
			end
		end
	end

	--
	-- These settings should hold a valid gpio
	--
	keys = { 'firmware-gpio-reset', 'firmware-gpio-sck', 'firmware-gpio-mosi', 'firmware-gpio-miso' }
	for k, v in pairs(keys) do
		if settings[v] ~= nil then
			if settings['gpio-platform'] == nil then
				error('config setting "' .. v .. '" must contain a supported gpio platform before the firmware gpio can be changed');
			end
			s = settings[v];
			if type(s) ~= 'number' or s < 0 then
				error('config setting "' .. v .. '" must contain a number larger than 0');
			end
			if wx.hasGPIO(s) == false then
				error('config setting "' .. v .. '" must contain a valid GPIO number');
			end
		end
	end

	for k, v in pairs(keys) do
		s = settings[v];
		for k1, v1 in pairs(keys) do
			if s ~= nil then
				s1 = settings[v1];
				if s1 ~= nil then
					if v1 ~= v and s1 == s then
						error('config setting "' .. v .. '" and "' .. v1 ..'" cannot be the same');
					end
				end
			end
			if v1 ~= v and s == pilight.default[v1:upper():gsub('-', '_')] then
				error('config setting "' .. v .. '" value is already the default gpio of "' .. v1 ..'"');
			end
		end
	end

	data['settings'] = settings;

	return 1;
end

function M.set(key, idx, val)
	local config = pilight.config();
	local data = config.getData();

	if key == nil then
		return nil;
	end

	if data == nil then
		error("config data table not properly initialized");
	end

	if data['settings'] == nil then
		error("config data table not properly initialized");
	end

	if data['settings'][key] == nil then
		error("config data table not properly initialized");
	end

	data['settings'][key] = val;

	return 1;
end

function M.get(key, idx)
	local config = pilight.config();
	local data = config.getData();

	if key == nil then
		return nil;
	end

	if data == nil or data['settings'] == nil then
		error("config data table not properly initialized");
	end

	if type(data['settings'][key]) == 'table' then
		return data['settings'][key][idx+1];
	end

	return data['settings'][key];
end

function M.write()
	local config = pilight.config();
	local data = config.getData();

	if data ~= nil and data['settings'] ~= nil then
		return json.stringify(data['settings']);
	else
		return nil;
	end
end

function M.info()
	return {
		name = "settings",
		version = "1.0",
		reqversion = "5.0",
		reqcommit = "87"
	}
end

return M;
