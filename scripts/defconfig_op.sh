#!/bin/bash

if [ $# -ne 2 ] && [ $# -ne 0 ]; then
	echo "Usage: $0 <operation> <config>"
	exit 1
fi
BEFORE=
AFTER=
REGEN=0
if [ "$1" = "set" ]; then
	BEFORE="# $2 is not set"
	AFTER="$2=y"
elif [ "$1" = "unset" ]; then
	BEFORE="$2=y"
	AFTER="# $2 is not set"
else
	REGEN=1
fi

for i in arch/arm64/configs/vendor/a51-aosp_defconfig arch/arm64/configs/vendor/a51-oneui_defconfig; do
	make O=out ARCH=arm64 CC=clang LD=ld.lld CROSS_COMPILE=aarch64-linux-gnu- vendor/$(basename $i)
	if [ $REGEN -ne 1 ]; then
		sed -i "s/$BEFORE/$AFTER/" out/.config
	fi
	cp out/.config $i;
done
