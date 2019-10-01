--
-- Used inside lua_reference_count.c
-- - test_lua_pass_interface_event7
--

local M = {};

function M.event(event)
	print("event");
end

function M.run()
	local event = pilight.async.event();
	local event1 = pilight.async.event();
	print("event", event());
	print("event1", event1());

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