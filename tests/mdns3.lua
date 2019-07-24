--
-- Used inside lua_network_mdns.c
-- - test_lua_network_mdns_rx
--

function dump_(o, i)
	if type(o) == 'table' then
		local s = '{\n'
		for k,v in pairs(o) do
			if type(k) ~= 'number' then k = '"'..k..'"' end
			s = s .. string.rep("\t", i) .. '['..k..'] = ' .. dump_(v, i+1) .. ',\n'
		end
		return s .. string.rep("\t", i-1) .. '}'
	else
		return tostring(o)
	end
end

function dump(o)
	return dump_(o, 1);
end

local M = {}

function dump_payload(a)
	for k, v in pairs(a) do
		print('type', v['type']);
		print('name', v['name']);
		print('length', v['length']);
		print('ttl', v['ttl']);
		print('class', v['class']);
		if v['type'] == 12 then
			print('flush', v['flush']);
		end
		if v['type'] == 12 or v['type'] == 33 then
			print('domain', v['domain']);
		end
		if v['type'] == 33 then
			print('weight', v['weight']);
			print('priority', v['priority']);
			print('port', v['port']);
		end
		if v['type'] == 1 or v['type'] == 28 then
			print('ip', v['ip']);
		end
		if v['type'] == 16 then
			for k1, v1 in pairs(v['options']) do
				print('options', v1);
			end
		end
	end
end

function M.callback(mdns, data, ip, port)
	print('id', string.format("%x", data['id']));
	print('qr', data['qr']);
	print('opcode', data['opcode']);
	print('aa', data['aa']);
	print('tc', data['tc']);
	print('rd', data['rd']);
	print('ra', data['ra']);
	print('z', data['z']);
	print('ad', data['ad']);
	print('cd', data['cd']);
	print('rcode', data['rcode']);
	print('rr_auth', data['rr_auth']);
	if data['qr'] == 0 then
		print(#data['queries']);
		print(#data['answers']);
		print(#data['records']);
		if #data['queries'] > 0 then
			if data['qr'] == 0 then
				for k, v in pairs(data['queries']) do
					print('type', v['type']);
					print('qu', v['qu']);
					print('name', v['name']);
					print('class1', v['class']);
				end
			end
		end
	end
	if #data['answers'] > 0 then
		dump_payload(data['answers']);
	end
	if #data['records'] > 0 then
		dump_payload(data['records']);
	end
end

function M.run()
	local mdns = pilight.network.mdns();

	local send = {};
	send['id'] = tonumber("AABB", 16);
	send['qr'] = 0;
	send['opcode'] = 2;
	send['aa'] = 0;
	send['tc'] = 1;
	send['rd'] = 0;
	send['ra'] = 1;
	send['z'] = 0;
	send['ad'] = 1;
	send['cd'] = 0;
	send['rcode'] = 1;
	send['rr_auth'] = 0;
	send['queries'] = {};
	send['queries'][1] = {};
	send['queries'][1]['name'] = "testname.local.foo";
	send['queries'][1]['qu'] = 0;
	send['queries'][1]['type'] = 33;
	send['queries'][1]['class'] = 1;
	send['queries'][2] = {};
	send['queries'][2]['name'] = "testname1.local1.foo";
	send['queries'][2]['qu'] = 0;
	send['queries'][2]['type'] = 33;
	send['queries'][2]['class'] = 1;
	send['queries'][3] = {};
	send['queries'][3]['name'] = "testname.local1.foo";
	send['queries'][3]['qu'] = 0;
	send['queries'][3]['type'] = 33;
	send['queries'][3]['class'] = 1;

	print(mdns);
	print(mdns.setCallback("callback"));
	print(mdns.send(send));

	return 1;
end

function M.info()
	return {
		name = "mdns",
		version = "4.1",
		reqversion = "7.0",
		reqcommit = "94"
	}
end

return M;