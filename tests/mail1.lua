--
-- Used inside lua_network_mail.c
-- - test_lua_network_mail_nonexisting_callback
--

local M = {}

function M.run()
	local mailobj = pilight.network.mail();

	--
	-- Keep prints in sync with mail.lua
	--
	print(5);
	print('main');

	print(mailobj.setSubject("foo"));
	print(mailobj.getSubject());
	print(mailobj.setFrom("order@pilight.org"));
	print(mailobj.getFrom());
	print(mailobj.setTo("info@pilight.nl"));
	print(mailobj.getTo());
	print(mailobj.setMessage("bar"));
	print(mailobj.getMessage());
	print(mailobj.setHost("127.0.0.1"));
	print(mailobj.getHost());
	print(mailobj.setPort(10025));
	print(mailobj.getPort());
	print(mailobj.setUser("pilight"));
	print(mailobj.getUser());
	print(mailobj.setPassword("test"));
	print(mailobj.getPassword());
	print(mailobj.setSSL(0));
	print(mailobj.getSSL());
	print(mailobj.setCallback("callback"));
	print(mailobj.getCallback());
	print(mailobj.getStatus());
	print(mailobj.send());

	return 1;
end

function M.info()
	return {
		name = "sendmail",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;