--
-- Used inside lua_network_http.c
-- - test_lua_network_http
--

local M = {}

function M.callback(obj, nr, pulses)
	local pulse = 0;
	local length = 0;
	data = obj.getUserdata();
	if data['length'] == nil then
		data['length'] = 1;
		length = 1;
	end
	if data['pulses'] == nil then
		data['pulses'] = {};
	end
	for i = 1, nr - 1, 1 do
		pulse = pulses[i];
		data['length'] = data['length'] + 1;
		length = data['length'];
		data['pulses'][length - 1] = pulse;
		if length > 512 then
			data['length'] = 1;
			length = 1;
		end
		if pulse > 5100 then -- mingaplen
			if length >= 25 and -- minrawlen
				length <= 512 then -- maxrawlen
				pilight.async.event(pilight.reason.RECEIVED_PULSETRAIN, data());
			end
			data['length'] = 1;
			length = 1;
		end
	end
end

function M.run()
	local config = pilight.config();
	local platform = config.getSetting("gpio-platform");
	local obj = wiringX.setup(platform);
	obj.ISR(1, wiringX.ISR_MODE_BOTH, "callback", 250);
	return 1;
end

function M.info()
	return {
		name = "433gpio",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;