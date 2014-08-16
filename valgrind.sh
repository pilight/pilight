#!/bin/bash

sudo valgrind --show-reachable=yes --dsymutil=yes --tool=memcheck --leak-check=full --track-origins=yes ./pilight-daemon -D
