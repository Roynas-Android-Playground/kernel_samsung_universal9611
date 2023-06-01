#/bin/bash
set -e

[ ! -e "scripts/packaging/pack.sh" ] && git submodule init && git submodule update
[ ! -d "toolchain" ] && echo "Make toolchain avaliable at $(pwd)/toolchain" && exit

export KBUILD_BUILD_USER=Royna
export KBUILD_BUILD_HOST=GrassLand

PATH=$PWD/toolchain/bin:$PATH

if [ "$1" = "oneui" ]; then
TARGET=oneui
FLAGS=ONEUI=1
else
TARGET=aosp
fi

if [ "$2" = "ksu" ]; then
CONFIG_EXT=vendor/ksu.config
fi

if [ -z "$DEVICE" ]; then
export DEVICE=a51
fi

rm -rf out
make O=out CROSS_COMPILE=aarch64-linux-gnu- CC=clang LD=ld.lld AS=llvm-as AR=llvm-ar \
	OBJDUMP=llvm-objdump READELF=llvm-readelf -j$(nproc) \
	vendor/${DEVICE}-${TARGET}_defconfig $CONFIG_EXT
make O=out CROSS_COMPILE=aarch64-linux-gnu- CC=clang LD=ld.lld AS=llvm-as AR=llvm-ar \
	OBJDUMP=llvm-objdump READELF=llvm-readelf ${FLAGS} -j$(nproc)
