--
-- Used inside lua_async_timer.c
-- - test_lua_async_thread_nonexisting_callback
--

local M = {};

function M.run()
	local timer = pilight.async.timer();
	local data = timer.getUserdata();

	print(type(timer));
	print(type(data));

	data['status'] = "main";
	print(data['status']);

	print(type(timer.setCallback("foo")));

	print(type(timer.start()));
end

function M.info()
	return {
		name = "timer",
		version = "1.0",
		reqversion = "1.0",
		reqcommit = "0"
	}
end

return M;