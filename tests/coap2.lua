--
-- Used inside lua_network_coap.c
-- - test_lua_network_coap
--

local M = {}

function M.run()
	local coap = pilight.network.coap();

	--
	-- Keep prints in sync with coap.lua
	--
	print(5);
	print('main');

	local send = {};

	print(coap.setData(send));
	print(coap.setCallback("callback"));
	print(coap.send());

	return 1;
end

function M.info()
	return {
		name = "coap",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;