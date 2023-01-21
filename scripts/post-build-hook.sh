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
gcc ${srctree}/scripts/kernelversion.c -Iinclude/generated/ -o scripts/kernelversion
KERNELSTR="$(./scripts/kernelversion)"
MY_PWD=$(pwd)
TIME="$(date "+%m%d-%H%M%S")"
KERNELZIP="$(echo "${KERNELSTR}" | sed s/^.*-//)@${TIME}.zip"
cd ${srctree}/scripts/packaging/ || exit
bash pack.sh "${MY_PWD}/${OBJ}" "${KERNELZIP}"
tg_sendFile "${KERNELZIP}" "${KERNELSTR}-${TIME}"
