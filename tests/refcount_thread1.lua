--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_thread1
--

local M = {};

function M.run()
	local thread = pilight.async.thread();
	local thread1 = pilight.async.thread();
	print("thread", thread());
	print("thread1", thread1());
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