#!/usr/bin/env bash

find . -name hardware_header.h -exec rm {} \;
find . -name hardware_init.h -exec rm {} \;
find . -name protocol_header.h -exec rm {} \;
find . -name protocol_init.h -exec rm {} \;
find . -name operator_header.h -exec rm {} \;
find . -name operator_init.h -exec rm {} \;
find . -name action_header.h -exec rm {} \;
find . -name action_init.h -exec rm {} \;

git checkout master
git stash
git stash clear
git pull origin master
git fetch --tags

cp CMakeLists.txt CMakeLists.txt.original

GIT=$(git describe --always | sed -e 's/^v//g');
COMMIT=$(echo $GIT | cut -d'-' -f2);
SHA=$(echo $GIT | cut -d'-' -f3);
VERSION=$(echo $GIT | cut -d'-' -f1 | sed -e 's/^v//g');

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-amd64.cmake
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh amd64

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-amd64.cmake -DCMAKE_BUILD_TYPE=Debug
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh amd64 debug

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-i386.cmake
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh i386

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-i386.cmake -DCMAKE_BUILD_TYPE=Debug
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh i386 debug

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-arm.cmake -DCMAKE_C_FLAGS=-fomit-frame-pointer
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh armhf

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-arm.cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-fomit-frame-pointer
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh armhf debug

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-mipsel.cmake
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh mipsel

sed -e"" 's/MODULESPACK ON/MODULESPACK OFF/g' CMakeLists.txt > CMakeLists.txt.tmp
cp CMakeLists.txt.tmp CMakeLists.txt
./setup.sh clear
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/pilight/tools/toolchain-mipsel.cmake -DCMAKE_BUILD_TYPE=Debug
make -j4
cpack -G DEB
mv *.deb ../
cd ..
./gen_package.sh mipsel debug

mv CMakeLists.txt.original CMakeLists.txt
rm CMakeLists.txt.tmp

rm -r build/
