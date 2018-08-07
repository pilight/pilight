-- Copyright (C) 2013 - 2016 CurlyMo

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

local M = {}

function is_dir(path)
	f = io.open(path)
	if f == nil then
		return false;
	end
	return (f:seek("end") ~= 0)
end

local json = {}

-- Internal functions.

local function kind_of(obj)
  if type(obj) ~= 'table' then return type(obj) end
  local i = 1
  for _ in pairs(obj) do
    if obj[i] ~= nil then i = i + 1 else return 'table' end
  end
  if i == 1 then return 'table' else return 'array' end
end

local function escape_str(s)
  local in_char  = {'\\', '"', '\b', '\f', '\n', '\r', '\t'}
  local out_char = {'\\', '"', 'b',  'f',  'n',  'r',  't'}
  for i, c in ipairs(in_char) do
    s = s:gsub(c, '\\' .. out_char[i])
  end
  return s
end

-- Returns pos, did_find; there are two cases:
-- 1. Delimiter found: pos = pos after leading space + delim; did_find = true.
-- 2. Delimiter not found: pos = pos after leading space;     did_find = false.
-- This throws an error if err_if_missing is true and the delim is not found.
local function skip_delim(str, pos, delim, err_if_missing)
  pos = pos + #str:match('^%s*', pos)
  if str:sub(pos, pos) ~= delim then
    if err_if_missing then
      error('Expected ' .. delim .. ' near position ' .. pos)
    end
    return pos, false
  end
  return pos + 1, true
end

-- Expects the given pos to be the first character after the opening quote.
-- Returns val, pos; the returned pos is after the closing quote character.
local function parse_str_val(str, pos, val)
  val = val or ''
  local early_end_error = 'End of input found while parsing string.'
  if pos > #str then error(early_end_error) end
  local c = str:sub(pos, pos)
  if c == '"'  then return val, pos + 1 end
  if c ~= '\\' then return parse_str_val(str, pos + 1, val .. c) end
  -- We must have a \ character.
  local esc_map = {b = '\b', f = '\f', n = '\n', r = '\r', t = '\t'}
  local nextc = str:sub(pos + 1, pos + 1)
  if not nextc then error(early_end_error) end
  return parse_str_val(str, pos + 2, val .. (esc_map[nextc] or nextc))
end

-- Returns val, pos; the returned pos is after the number's final character.
local function parse_num_val(str, pos)
  local num_str = str:match('^-?%d+%.?%d*[eE]?[+-]?%d*', pos)
  local val = tonumber(num_str)
  if not val then error('Error parsing number at position ' .. pos .. '.') end
  return val, pos + #num_str
end


-- Public values and functions.

function json.stringify(obj, as_key, indent)
  local s = {}  -- We'll build the string as an array of strings to be concatenated.
  local kind = kind_of(obj)  -- This is 'array' if it's an array or type(obj) otherwise.
	if indent == nil then indent = 0 end
  if kind == 'array' then
    if as_key then error('Can\'t encode array as key.') end
    s[#s + 1] = '['
		local t = nil;
    for i, val in ipairs(obj) do
      if i > 1 then s[#s + 1] = ',' end
			t = json.stringify(val, nil, indent);
			if t:sub(0, 1) ~= '{' and t:sub(0, 1) ~= '[' then
				s[#s + 1] = ' ';
			end
      s[#s + 1] = t;
    end
		if t ~= nil and t:sub(0, 1) ~= '{' and t:sub(0, 1) ~= '[' then
			s[#s + 1] = ' ';
		end
    s[#s + 1] = ']'
  elseif kind == 'table' then
    if as_key then error('Can\'t encode table as key.') end
    s[#s + 1] = '{\n'
    for k, v in pairs(obj) do
      if #s > 1 then s[#s + 1] = ',\n' end
      s[#s + 1] = string.rep("\t", indent + 1) .. json.stringify(k, true, indent + 1)
      s[#s + 1] = ': '
			local t = json.stringify(v, nil, indent + 1);
			if t ~= '{' and t ~= '[' then
				s[#s + 1] = t
			else
				s[#s + 1] = '}';
			end
    end
    s[#s + 1] = '\n' .. string.rep("\t", indent) .. '}'
  elseif kind == 'string' then
    return '"' .. escape_str(obj) .. '"'
  elseif kind == 'number' then
    if as_key then return '"' .. tostring(obj) .. '"' end
    return tostring(obj)
  elseif kind == 'boolean' then
    return tostring(obj)
  elseif kind == 'nil' then
    return 'null'
  else
    error('Unjsonifiable type: ' .. kind .. '.')
  end
  return table.concat(s)
end

json.null = {}  -- This is a one-off table to represent the null value.

function json.parse(str, pos, end_delim)
  pos = pos or 1
  if pos > #str then error('Reached unexpected end of input.') end
  local pos = pos + #str:match('^%s*', pos)  -- Skip whitespace.
  local first = str:sub(pos, pos)
  if first == '{' then  -- Parse an object.
    local obj, key, delim_found = {}, true, true
    pos = pos + 1
    while true do
      key, pos = json.parse(str, pos, '}')
      if key == nil then return obj, pos end
      if not delim_found then error('Comma missing between object items.') end
      pos = skip_delim(str, pos, ':', true)  -- true -> error if missing.
      obj[key], pos = json.parse(str, pos)
      pos, delim_found = skip_delim(str, pos, ',')
    end
  elseif first == '[' then  -- Parse an array.
    local arr, val, delim_found = {}, true, true
    pos = pos + 1
    while true do
      val, pos = json.parse(str, pos, ']')
      if val == nil then return arr, pos end
      if not delim_found then error('Comma missing between array items.') end
      arr[#arr + 1] = val
      pos, delim_found = skip_delim(str, pos, ',')
    end
  elseif first == '"' then  -- Parse a string.
    return parse_str_val(str, pos + 1)
  elseif first == '-' or first:match('%d') then  -- Parse a number.
    return parse_num_val(str, pos)
  elseif first == end_delim then  -- End of an object or array.
    return nil, pos + 1
  else  -- Parse true, false, or null.
    local literals = {['true'] = true, ['false'] = false, ['null'] = json.null}
    for lit_str, lit_val in pairs(literals) do
      local lit_end = pos + #lit_str - 1
      if str:sub(pos, lit_end) == lit_str then return lit_val, lit_end + 1 end
    end
    local pos_info_str = 'position ' .. pos .. ': ' .. str:sub(pos, pos + 10)
    error('Invalid json syntax starting at ' .. pos_info_str)
  end
end

function dump(o)
	if type(o) == 'table' then
		local s = '{\n'
		for k,v in pairs(o) do
			if type(k) ~= 'number' then k = '"'..k..'"' end
				s = s .. '['..k..'] = ' .. dump(v) .. ',\n'
			end
		return s .. '}\n'
	else
		return tostring(o)
	end
end

function M.read(f)
	if f == nil then
		return 0;
	end

	content = "";
	for line in io.lines(f) do
		content = content .. line;
	end

	local k = nil;
	local s = nil;
	local valid = false;
	local config = pilight.config();
	local data = config.getData();
	local port = -1;
	local http_port = pilight.defaults.WEBSERVER_HTTP_PORT;
	local https_port = pilight.defaults.WEBSERVER_HTTPS_PORT;
	local jobject = json.parse(content);
	local settings = jobject['settings'];

	if data == nil then
		error("config data table not properly initialized");
	end

	if settings == nil then
		error('no config settings were given');
	end

	local keys = {
		'port',

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
			if s ~= nil and not is_dir(s) then
				error('config setting "' .. v .. '" must contain a valid path');
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
		'standalone', 'watchdog-enable', 'stats-enable',
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
		if #settings[v] ~= 2 then
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
		if #settings[v] == 0 then
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
		if #s == 0 then
			error('config setting "' .. v .. '" must contain a valid ip address');
		end
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

	v = 'gpio-platform';
	if settings[v] ~= nil then
		s = settings[v];
		if type(v) ~= 'string' then
		end
		if wiringX.setup(s) ~= true then
			error('config setting "' .. v .. '" must contain a supported gpio platform');
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
			if wiringX.hasGPIO(s) == false then
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
			if v1 ~= v and s == pilight.defaults[v1:upper():gsub('-', '_')] then
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