--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_thread3
--

local M = {};

function M.thread1(thread)
	print("thread1");
end

function M.thread(thread)
	print("thread");
	local data = thread.getUserdata();
	local thread1 = pilight.async.thread(data['thread']);
	data['thread'] = nil;
	thread1.trigger();
end

function M.run()
	local thread = pilight.async.thread();
	local thread1 = pilight.async.thread();
	print("thread", thread());
	print("thread1", thread1());
	thread1.setCallback("thread1");

	thread.setUserdata({['thread']=thread1()});
	thread.setCallback("thread");
	thread.trigger();
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