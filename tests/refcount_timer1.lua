--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_timer1
--

local M = {};

function M.run()
	local timer = pilight.async.timer();
	local timer1 = pilight.async.timer();
	print("timer", timer());
	print("timer1", timer1());
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