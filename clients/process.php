#!/usr/bin/env php
<?php
$sMsg = "M-SEARCH * HTTP/1.1\r\n";
$sMsg .= "Host:239.255.255.250:1900\r\n";
$sMsg .= "ST:urn:schemas-upnp-org:service:pilight:1\r\n";
$sMsg .= "Man:\"ssdp:discover\"\r\n";
$sMsg .= "MX:3\r\n\r\n";

$sBuffer = NULL;
$sTmp = NULL;
$rSocket = socket_create(AF_INET, SOCK_DGRAM, 0);
socket_set_option($rSocket, SOL_SOCKET, SO_RCVTIMEO, array('sec'=>0, 'usec'=>10000));
$send_ret = socket_sendto($rSocket, $sMsg, 1024, 0, '239.255.255.250', 1900);
$aHosts = Array();
$x = 0;
while(@socket_recvfrom($rSocket, $sBuffer, 1024, MSG_WAITALL, $sTmp, $sTmp)) {
	if(strpos($sBuffer, 'pilight') !== false) {
		$aLines = explode("\r\n", $sBuffer); 
		foreach($aLines as $sLine) {
			if(sscanf($sLine, "Location:%d.%d.%d.%d:%d\r\n", $ip1, $ip2, $ip3, $ip4, $port) > 0) {
				$ip = $ip1.'.'.$ip2.'.'.$ip3.'.'.$ip4;
				$aHosts[$x]['ip'] = $ip;
				$aHosts[$x]['port'] = $port;
				$x++;
			}
		}
	}
}
socket_close($rSocket);

if(count($aHosts) > 0) {
	$fp = fsockopen($aHosts[0]['ip'], $aHosts[0]['port'], $errno, $errdesc) or die ("Couldn't connect to server");
	fputs($fp, '{"message":"client receiver"}');
	fgets($fp, 1024);
	while(!feof($fp)){
		echo fgets($fp, 1024);
	}
} else {
	die("no pilight ssdp connections found\n");
}
?>