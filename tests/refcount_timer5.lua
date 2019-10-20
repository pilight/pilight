--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_timer4
--

local M = {};

function M.timer(timer)
	print('b', "timer");
	local data = timer.getUserdata();

	data['repeat'] = data['repeat'] + 1;

	if data['repeat'] <= 1 then
		timer.start();
	end
end

function M.run()
	local timer = pilight.async.timer();
	print("timer", timer());

	local data = {};
	data['repeat']=0;

	timer.setCallback("timer");
	timer.setUserdata(data);
	timer.setTimeout(100);
	timer.setRepeat(0);
	timer.start();
	timer.start();
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