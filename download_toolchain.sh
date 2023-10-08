#!/bin/bash
set -e
if [ ! -e toolchain ]; then
echo 'mkdir toolchain'
mkdir toolchain
elif [ ! -d toolchain ]; then
echo '$(pwd)/toolchain is not a directory'
exit 1
fi
echo 'Setting up toolchain in $(pwd)/toolchain'
cd toolchain
echo 'Download antman and sync'
bash <(curl -s "https://raw.githubusercontent.com/Neutron-Toolchains/antman/main/antman") -S
echo 'Clone libarchive for bsdtar'
git clone https://github.com/libarchive/libarchive || true
sudo apt install -y pkg-config m4 libtool automake autoconf
cd libarchive
echo 'Build libarchive'
bash build/autogen.sh
./configure
make -j$(nproc)
cd ..
echo 'Patch for glibc'
PATH=$(pwd)/libarchive:$PATH bash <(curl -s "https://raw.githubusercontent.com/Neutron-Toolchains/antman/main/antman") --patch=glibc
echo 'Done'