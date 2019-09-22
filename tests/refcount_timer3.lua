--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_timer3
--

local M = {};

function M.timer1(timer)
	print("timer1");

	local data = timer.getUserdata();
	data['repeat'] = data['repeat'] + 1;
	if data['repeat'] == 1 then
		timer.stop();
	end
end

function M.timer(timer)
	print("timer");
	local data = timer.getUserdata();
	local timer1 = pilight.async.timer(data['timer']);
	timer1.start();
end

function M.run()
	local timer = pilight.async.timer();
	local timer1 = pilight.async.timer();
	print("timer", timer());
	print("timer1", timer1());

	timer.setUserdata({['timer']=timer1()});

	local data1 = {};
	data1['repeat'] = 0;

	timer1.setCallback("timer1");
	timer1.setTimeout(250);
	timer1.setRepeat(250);
	timer1.setUserdata(data1);

	timer.setCallback("timer");
	timer.setTimeout(100);
	timer.setRepeat(0);
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