--
-- Used inside lua_network_coap.c
-- - test_lua_network_coap_tx
--

local M = {}

function M.callback(coap, data, ip, port)
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
	local data = coap.getUserdata();
	local a = {};
	a[1] = {};
	a[2] = 3;
	a[1]['a'] = 4;
	a[1]['b'] = 5;

	data['foo'] = a;
	data['status'] = "main";

	print(data['foo'][1]['b']);
	print(data['status']);

	local send = {};
	send['ver'] = 1;
	send['t'] = 1;
	send['code'] = 0001;
	send['msgid'] = 0001;
	send['options'] = {};
	send['options'][0] = {};
	send['options'][0]['val'] = 'cit';
	send['options'][0]['num'] = 11;
	send['options'][1] = {};
	send['options'][1]['val'] = 'd';
	send['options'][1]['num'] = 11;

	print(coap.setCallback("callback"));
	print(coap.send(send));

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