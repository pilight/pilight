#!/usr/bin/python
import socket
import httplib
import StringIO
import struct
import re

def discover(service, timeout=2, retries=1):
    group = ("239.255.255.250", 1900)
    message = "\r\n".join([
        'M-SEARCH * HTTP/1.1',
        'HOST: {0}:{1}'.format(*group),
        'MAN: "ssdp:discover"',
        'ST: {st}','MX: 3','',''])

    responses = {}
    i = 0;
    for _ in range(retries):
        i += 1
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVTIMEO, struct.pack('LL', 0, 10000));
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
        sock.sendto(message.format(st=service), group)
        while True:
            try:
                responses[i] = sock.recv(1024);
                break;
            except socket.timeout:
				break;
            except:
                print "no pilight ssdp connections found"
                break;
    return responses.values()

responses = discover("urn:schemas-upnp-org:service:pilight:1");
if len(responses) > 0:
	locationsrc = re.search('Location:([0-9.]+):(.*)', str(responses[0]), re.IGNORECASE)
	if locationsrc:
		location = locationsrc.group(1)
		port = locationsrc.group(2)

	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	socket.setdefaulttimeout(0)
	s.connect((location, int(port)))
	s.send('{"message":"client receiver"}\n')
	if s.recv(1024) == '{"message":"accept client"}\n':
		while(1):
			buffer = s.recv(1024);
			for f in iter(buffer.splitlines()):
				print(f);
	s.close()
