--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_mqtt1
--

local M = {};

function M.run()
	local timer = pilight.async.timer();
	local mqtt = pilight.network.mqtt();
	print("timer", timer());
	print("mqtt", mqtt());
end

function M.info()
	return {
		name = "refcount",
		version = "1.0",
		reqversion = "1.0",
		reqcommit = "0"
	}
end

return M;