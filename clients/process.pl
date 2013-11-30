#!/usr/bin/env perl

use IO::Socket;

socket(sock, PF_INET, SOCK_DGRAM, getprotobyname("udp")) or die "socket:$@";
setsockopt(sock, SOL_SOCKET, SO_BROADCAST, 1) or die "setsockopt:$@";
setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, pack('l!l!', 0, 10000)) or die "setsockopt:$@";
my $dest = sockaddr_in(1900, inet_aton('239.255.255.250'));

$msg = "M-SEARCH * HTTP/1.1\r\n";
$msg .= "Host:239.255.255.250:1900\r\n";
$msg .= "ST:urn:schemas-upnp-org:service:pilight:1\r\n";
$msg .= "Man:\"ssdp:discover\"\r\n";
$msg .= "MX:3\r\n\r\n";

my $ip = '';
my $port = '';
send(sock, $msg, 1024, $dest);
while(recv(sock, $data, 1024, $dest)) {
	if(index($data, "pilight") != -1) {
		foreach(split(/\r\n/, $data)) {
			$_ =~ /Location:([0-9\.]+):([0-9]+)/ || next;
			($ip,$port) = ($1,$2);
		}
	}
}

$socket = new IO::Socket::INET (
    PeerHost  => $ip,
    PeerPort  =>  $port,
    Proto => 'tcp',
) or die "no pilight ssdp connections found\n";

$socket->send('{"message":"client receiver"}');
$socket->recv($recv_data, 1024);
print $recv_data;
while(1) {
	$socket->recv($recv_data, 1024);
	print $recv_data;
}