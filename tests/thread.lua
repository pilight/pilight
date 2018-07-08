--
-- Used inside lua_async_thread.c
-- - test_lua_async_thread
--

local M = {};

function M.thread(thread)
	local data = thread.getUserdata();
	if(data['status'] == 'main') then
		data['status'] = "thread";
	else
		data['status'] = "thread1";
	end
	print(data['status']);
end

function M.run()
	local thread = pilight.async.thread();
	local thread1 = pilight.async.thread();
	local data = thread.getUserdata();
	local data1 = thread1.getUserdata();
	local a = {};
	a[1] = {};
	a[2] = 3;
	a[1]['a'] = 4;
	a[1]['b'] = 5;

	print(type(thread));
	print(type(thread1));
	print(type(data));
	print(type(data1));

	print(type(thread.setCallback("thread")));
	print(type(thread1.setCallback("thread")));

	data['status'] = "main";
	data1['status'] = "main1";
	data['foo'] = a;
	data1['bar'] = a;
	print(data['foo'][1]['b']);
	print(data['status']);
	print(data1['bar'][1]['a']);
	print(data1['status']);

	print(type(thread1.trigger()));
	print(type(thread.trigger()));
	return 1;
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