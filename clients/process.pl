#!/usr/bin/env perl

use IO::Socket;

$socket = new IO::Socket::INET (
    PeerHost  => '127.0.0.1',
    PeerPort  =>  '5000',
    Proto => 'tcp',
) or die "Couldn't connect to Server\n";

$socket->send('{"message":"client receiver"}');
$socket->recv($recv_data, 1024);
print $recv_data;
while(1) {
	$socket->recv($recv_data, 1024);
	print $recv_data;
}