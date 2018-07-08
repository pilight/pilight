--
-- Used inside lua_network_http.c
-- - test_lua_network_http
--

local M = {}

function M.callback(http)
	print(http.getCode());
	print(http.getData());
	print(http.getUrl());
	print(http.getMimetype());
	print(http.getSize());
end

function M.run()
	local httpobj = pilight.network.http();
	local data = httpobj.getUserdata();
	local a = {};
	a[1] = {};
	a[2] = 3;
	a[1]['a'] = 4;
	a[1]['b'] = 5;

	data['foo'] = a;
	data['status'] = "main";

	print(data['foo'][1]['b']);
	print(data['status']);

	print(httpobj.setUrl("http://127.0.0.1:10080/"));
	print(httpobj.getCode());
	print(httpobj.setCallback("callback"));
	print(httpobj.get());

	return 1;
end

function M.info()
	return {
		name = "http",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;