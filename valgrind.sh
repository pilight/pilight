#!/bin/bash

sudo valgrind --show-reachable=yes --dsymutil=yes --tool=memcheck --leak-check=full --track-origins=yes ./pilight-daemon -D -C /media/rpi/433.92/daemon2/config.json
