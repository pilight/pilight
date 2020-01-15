#!/bin/bash -e

if [[ $PLATFORM == "Unix" ]]; then
    curl https://apt.pilight.org/pilight.key | sudo apt-key add - && echo "deb http://apt.pilight.org/ stable main" | sudo tee -a /etc/apt/sources.list
    sudo apt-get update
    sudo apt-cache search libmbedtls
    sudo apt-get install libwiringx-dev libwiringx libpcap-dev libunwind8-dev lcov libluajit-5.1-2 libluajit-5.1-dev -y --allow-unauthenticated

    mkdir depends && cd depends
    wget http://apt.pilight.org/pool/stable/main/l/libmbedcrypto0/libmbedcrypto0_2.5.1-1ubuntu1_amd64.deb
    wget http://apt.pilight.org/pool/stable/main/l/libmbedtls-dev/libmbedtls-dev_2.5.1-1ubuntu1_amd64.deb
    wget http://apt.pilight.org/pool/stable/main/l/libmbedx509-0/libmbedx509-0_2.5.1-1ubuntu1_amd64.deb
    wget http://apt.pilight.org/pool/stable/main/l/libmbedtls10/libmbedtls10_2.5.1-1ubuntu1_amd64.deb
    sudo dpkg -i *.deb
    cd ..
fi
