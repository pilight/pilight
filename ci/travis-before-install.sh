#!/bin/bash -e

if [[ "$LINT" == "1" ]]; then
    # Build up-to-date astyle, takes <10 seconds
    # Change to different directory to prevent astyle linting itself
    pushd ..
    wget https://sourceforge.net/projects/astyle/files/astyle/astyle%203.0.1/astyle_3.0.1_linux.tar.gz -O astyle.tgz
    tar -xzf astyle.tgz
    cd astyle/build/gcc
    make -j2 release
    sudo make install
    exit 0
    popd
fi

if [[ $PLATFORM == "Unix" ]]; then
    curl https://apt.pilight.org/pilight.key | sudo apt-key add - && echo "deb http://apt.pilight.org/ stable main" | sudo tee -a /etc/apt/sources.list
    sudo apt-get update
    sudo apt-get install libwiringx-dev libwiringx libpcap-dev libunwind8-dev lcov liblua5.2-0 liblua5.2-dev -y

    mkdir depends && cd depends
    wget http://security.ubuntu.com/ubuntu/pool/universe/m/mbedtls/libmbedcrypto0_2.5.1-1ubuntu1_amd64.deb
    wget http://security.ubuntu.com/ubuntu/pool/universe/m/mbedtls/libmbedtls10_2.5.1-1ubuntu1_amd64.deb
    wget http://security.ubuntu.com/ubuntu/pool/universe/m/mbedtls/libmbedx509-0_2.5.1-1ubuntu1_amd64.deb
    wget http://security.ubuntu.com/ubuntu/pool/universe/m/mbedtls/libmbedtls-dev_2.5.1-1ubuntu1_amd64.deb
    sudo dpkg -i *.deb
    cd ..
fi
