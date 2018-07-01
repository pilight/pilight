--
-- Used inside lua_async_thread.c
-- - test_lua_async_thread_nonexisting_callback
--

local M = {};

function M.run()
	local thread = pilight.async.thread();
	local data = thread.getUserdata();
	print(type(thread));
	print(type(data));

	print(type(thread.setCallback("foo")));

	data['status'] = "main";
	print(data['status']);

	print(type(thread.trigger()));
end

function M.info()
	return {
		name = "thread",
		version = "1.0",
		reqversion = "1.0",
		reqcommit = "0"
	}
end

return M;