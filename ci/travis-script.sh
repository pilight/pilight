#!/bin/bash -e

if [[ "$LINT" == "1" ]]; then
	. ci/lint.sh
	exit 0
fi

if [[ ${PLATFORM} == "Unix" ]]; then
    mkdir -p build
    cd build || exit 1

    cmake -DPILIGHT_UNITTEST=1 \
        -DCOVERALLS=ON \
        -DCMAKE_BUILD_TYPE=Debug ..
    make

    sudo LD_PRELOAD=libgpio.so ./pilight-unittest && exit 0
fi
