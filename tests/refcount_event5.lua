--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_event5
--

local M = {};

function M.event1(event1)
	print("event1");
	event1.unregister(10001);
end

function M.event(event)
	print("event");

	local data = event.getUserdata();
	local event1 = pilight.async.event(data['event']);
	data['event'] = nil;
	event1.trigger({});
	event.unregister(10000);
end

function M.run()
	local event = pilight.async.event();
	local event1 = pilight.async.event();
	print("event", event());
	print("event1", event1());

	event1.setCallback("event1");
	event1.register(10001);

	event.setUserdata({['event']=event1()});

	event.setCallback("event");
	event.register(10000);
	event.trigger({});
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