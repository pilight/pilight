--
-- Used inside lua_async_timer.c
-- - test_lua_async_timer
--

local M = {};

function M.timer(timer)
	local data = timer.getUserdata();
	if(data['status'] == 'main') then
		data['status'] = "timer";
	else
		data['status'] = "timer1";
	end
	print(data['status']);
	print(type(timer.stop()));
end

function M.run()
	local timer = pilight.async.timer();
	local timer1 = pilight.async.timer();
	local data = timer.getUserdata();
	local data1 = timer1.getUserdata();

	print(type(timer));
	print(type(timer1));
	print(type(data));
	print(type(data1));

	data['status'] = "main";
	data1['status'] = "main1";

	print(data['status']);
	print(data1['status']);

	print(type(timer.setCallback("timer")));
	print(type(timer1.setCallback("timer")));
	print(type(timer.setTimeout(1000)));
	print(type(timer1.setTimeout(2000)));
	print(type(timer.setRepeat(1000)));
	print(type(timer1.setRepeat(2000)));
	print(type(timer.start()));
	print(type(timer1.start()));

	return 1;
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