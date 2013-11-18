#!/usr/bin/env php
<?PHP
$fp = fsockopen('127.0.0.1', 5000, $errno, $errdesc) or die ("Couldn't connect to Server");
fputs($fp, '{"message":"client receiver"}');
fgets($fp, 1024);
while(!feof($fp)){
	echo fgets($fp, 1024);
}
?>