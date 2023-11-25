#!/bin/bash
# script that is called after the build

set -e

# Telegram functions. Requires CHAT_ID and BOT_TOKEN
function tg_sendText() {
        curl -s "https://api.telegram.org/bot$BOT_TOKEN/sendMessage" \
                -d "parse_mode=html" \
                -d text="${1}" \
                -d chat_id=$CHAT_ID \
                -d "disable_web_page_preview=true"
}
function tg_sendFile() {
        curl -s "https://api.telegram.org/bot$BOT_TOKEN/sendDocument" \
                -F parse_mode=markdown \
                -F chat_id=$CHAT_ID \
                -F document=@"${1}" \
                -F caption="$2"
}

if [ ! -f "${srctree}/local.config" ]; then
echo "Please fill in your credentials in local.config"
exit 0
fi

source "${srctree}/local.config"
if [ -n "${DEVICE+x}" ]; then
DEVICE_STR=" for $DEVICE"
fi
tg_sendText "Build completed${DEVICE_STR}!"
if [ ! -n "${OBJ+x}" ]; then
echo "OBJ was not set. Not uploading"
exit 0
fi
${HOSTCC} ${srctree}/scripts/kernelversion.c -Iinclude/generated/ -o scripts/kernelversion -D__UTS__
${HOSTCC} ${srctree}/scripts/kernelversion.c -Iinclude/generated/ -o scripts/ccversion -D__CC__
KERNELSTR="$(echo "$(./scripts/kernelversion)" | sed 's/@.*//')"
CCSTR="$(./scripts/ccversion)"
MY_PWD=$(pwd)
TIME="$(date "+%Y%m%d")"
SUFFIX=
if [ -z "${ONEUI}" ]; then
SUFFIX=-AOSP
else
SUFFIX=-OneUI
fi
KERNELZIP="$(echo "${KERNELSTR}" | sed s/^.*-//)${SUFFIX}-${DEVICE}_${TIME}.zip"
COMMITMSG="$(git log --pretty=format:'"%h : %s"' -1)"
BRANCH="$(git branch --show-current)"
FOR=

cd ${srctree}/scripts/packaging/ || exit
bash pack.sh "${MY_PWD}/${OBJ}" "${KERNELZIP}"
if [ -z "$ONEUI" ]; then
FOR="For AOSP ROMs"
else
FOR="For OneUI5"
fi
tg_sendText "<b>${KERNELSTR} Kernel Build</b>%0A${FOR}%0Abranch <code>${BRANCH}</code>%0AUnder commit <code>${COMMITMSG}</code>%0AUsing compiler: <code>${CCSTR}</code>%0AEnded on <code>$(date)</code>"
tg_sendFile "${KERNELZIP}"
