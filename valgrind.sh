#!/bin/bash

sudo valgrind --dsymutil=yes --tool=memcheck --leak-check=full --track-origins=yes ./pilight-daemon -D
