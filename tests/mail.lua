--
-- Copyright (C) 2018 CurlyMo
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--

local M = {}

function M.callback(mail)
	print(mail.getStatus());
end

function M.run()
	local mailobj = pilight.network.mail();
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