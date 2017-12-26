#!/bin/bash -e

if [ "${TRAVIS_OS_NAME}" == "linux" ]; then
	sudo sh -c 'echo 0 > /proc/sys/net/ipv6/conf/all/disable_ipv6';
fi

