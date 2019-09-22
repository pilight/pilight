--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_event1
--

local M = {};

function M.run()
	local event = pilight.async.event();
	local event1 = pilight.async.event();
	print("event", event());
	print("event1", event1());
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