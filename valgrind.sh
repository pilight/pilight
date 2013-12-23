#!/bin/bash

sudo valgrind --tool=memcheck --leak-check=full --track-origins=yes ./pilight-daemon -D
