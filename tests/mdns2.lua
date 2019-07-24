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


function create_payload(obj)
	obj[1] = {};
	obj[1]['type'] = 12;
	obj[1]['name'] = "testname.local.foo";
	obj[1]['domain'] = "testname.local.foo";
	obj[1]['flush'] = 1;
	obj[1]['ttl'] = 2;
	obj[1]['class'] = 3;
	obj[2] = {};
	obj[2]['type'] = 33;
	obj[2]['name'] = "testname.local.foo";
	obj[2]['domain'] = "testname.local.foo";
	obj[2]['flush'] = 1;
	obj[2]['ttl'] = 2;
	obj[2]['class'] = 3;
	obj[2]['weight'] = 1;
	obj[2]['priority'] = 1;
	obj[2]['port'] = 80;
	obj[3] = {};
	obj[3]['type'] = 1;
	obj[3]['name'] = "testname.local.foo";
	obj[3]['flush'] = 1;
	obj[3]['ttl'] = 2;
	obj[3]['class'] = 3;
	obj[3]['ip'] = '192.168.1.2';
	obj[4] = {};
	obj[4]['type'] = 28;
	obj[4]['name'] = "testname.local.foo";
	obj[4]['flush'] = 1;
	obj[4]['ttl'] = 2;
	obj[4]['class'] = 3;
	obj[4]['ip'] = '0a:1b:2c:3d:4e:5f:70:81:92:a3:b4:c5:d6:e7:f8:09';
	obj[5] = {};
	obj[5]['type'] = 16;
	obj[5]['name'] = "testname.local.foo";
	obj[5]['flush'] = 1;
	obj[5]['ttl'] = 2;
	obj[5]['class'] = 3;
	obj[5]['options'] = {};
	obj[5]['options'][0] = "foo";
	obj[5]['options'][1] = "lorem";
end

function M.run()
	local mdns = pilight.network.mdns();

	local send = {};
	send['id'] = tonumber("AABB", 16);
	send['qr'] = 1;
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
	send['answers'] = {};
	send['records'] = {};

	create_payload(send['answers']);
	create_payload(send['records']);

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