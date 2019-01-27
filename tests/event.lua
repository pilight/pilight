--
-- Used inside lua_async_event.c
-- - test_lua_async_event
--

local M = {};

function M.event(event, reason, data)
	print(0, reason, nil);
	print(1, reason, data['status']);

	if data['status'] == false then
		print(2, reason, event.unregister(10001));
	end

	if reason == 10001 then
		print(3, reason, event.unregister(10000));
		print(4, reason, event.trigger({status = false}));
	end
end

function M.run()
	local event = pilight.async.event();

	print(1, 0, event.setCallback("event"));
	print(2, 0, event.register(10000));
	print(3, 0, event.register(10001));
	print(4, 0, event.register(10002));
	print(5, 0, event.trigger({status = true}));

	return 1;
end

function M.info()
	return {
		name = "event",
		version = "1.0",
		reqversion = "1.0",
		reqcommit = "0"
	}
end

return M;