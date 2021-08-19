#!/bin/bash -e

# set CC to the correct compiler
if [[ "${COMPILER}" == "gcc-6" ]]; then
    export CC=gcc-6
    export CXX=g++-6
elif [[ "${COMPILER}" == "gcc-7" ]]; then
    export CC=gcc-7
    export CXX=g++-7
elif [[ "${COMPILER}" == "clang-3.9" ]]; then
    export CC=clang-3.9
    export CXX=clang++-3.9
elif [[ "${COMPILER}" == "clang-5.0" ]]; then
    export CC=clang-5.0
    export CXX=clang++-5.0
fi

if [[ ${PLATFORM} == "Unix" ]]; then
    mkdir -p build
    cd build || exit 1

    cmake -DPILIGHT_UNITTEST=1 \
        -DCOVERALLS=ON \
        -DCMAKE_BUILD_TYPE=Debug ..
    make

    sudo ./pilight-unittest && exit 0
fi
