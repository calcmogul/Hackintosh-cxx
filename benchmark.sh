#!/bin/bash

sed -i 's/gMaxLength = 16/gMaxLength = 6/g' src/Main.cpp
sed -i 's/threadCount = 20/threadCount = 10/g' src/Main.cpp
make release
pushd Release
rm -f 0-3.txt
./Hackintosh-cxx brute 0
rm -f 0.3.txt
popd
sed -i 's/gMaxLength = 6/gMaxLength = 16/g' src/Main.cpp
sed -i 's/threadCount = 10/threadCount = 20/g' src/Main.cpp
make release

