--
-- Used inside lua_network_coap.c
-- - test_lua_network_coap_rx
--

local M = {}

function M.callback(coap, data, ip, port)
	print(ip);
	print(port);
	print(data['ver']);
	print(data['t']);
	print(data['tkl']);
	print(data['code']);
	print(data['msgid']);
	print(data['numopt']);
	print(data['options'][0]['val']);
	print(data['options'][0]['num']);
	print(data['payload']);
end

function M.run()
	local coap = pilight.network.coap();

	print(coap.setCallback("callback"));
	print(coap.listen());

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