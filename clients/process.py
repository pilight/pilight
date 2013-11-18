#!/usr/bin/env python

import sys
import os
import socket
import time
from subprocess import call

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 5000))
s.send('{"message":"client receiver"}\n')
if s.recv(1024) == '{"message":"accept client"}\n':
	while(1):
		buffer = s.recv(1024);
		for f in iter(buffer.splitlines()):
			print(f);
s.close()
