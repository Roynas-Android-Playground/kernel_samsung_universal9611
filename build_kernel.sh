#/bin/bash
set -e

[ ! -e "scripts/packaging/pack.sh" ] && git submodule init && git submodule update
[ ! -e "toolchain" ] && echo "Make toolchain avaliable at $(pwd)/toolchain" && exit

export KBUILD_BUILD_USER=Royna
export KBUILD_BUILD_HOST=GrassLand

PATH=$PWD/toolchain/bin:$PATH

TARGET=aosp
FLAGS=
if [ "$1" = "oneui" ]; then
TARGET=oneui
FLAGS=ONEUI=1
fi
if [ -z "$DEVICE" ]; then
DEVICE=a51
fi

rm -rf out
make O=out CROSS_COMPILE=aarch64-linux-gnu- CC=clang LD=ld.lld AS=llvm-as AR=llvm-ar \
	OBJDUMP=llvm-objdump READELF=llvm-readelf -j$(nproc) vendor/${DEVICE}-${TARGET}_defconfig
make O=out CROSS_COMPILE=aarch64-linux-gnu- CC=clang LD=ld.lld AS=llvm-as AR=llvm-ar \
	OBJDUMP=llvm-objdump READELF=llvm-readelf ${FLAGS} -j$(nproc)
