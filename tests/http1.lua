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
	print(httpobj.setUrl("http://127.0.0.1:10080/index.php?foo=bar"));
	print(httpobj.getCode());
	print(httpobj.setData("{\"foo\":\"bar\"}"));
	print(httpobj.setMimetype("application/json"));
	print(httpobj.setCallback("callback"));
	print(httpobj.post());

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