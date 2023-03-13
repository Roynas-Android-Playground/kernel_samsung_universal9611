#!/bin/bash

set -e

which go || (echo "Go not found" && exit)
go install github.com/github-release/github-release@latest

KV=$(echo "$(out/scripts/kernelversion)" | sed 's/@.*//')
NUM=0
set +e
git tag "$KV-$NUM"
while [ $? -ne 0 ]; do
	NUM=$(expr $NUM + 1)
	git tag "$KV-$NUM"
done
set -e
KV="$KV-$NUM"
git push origin "$KV"

GHREL=$HOME/go/bin/github-release
$GHREL release --user A51-Development --repo kernel_samsung_a51 --tag "$KV" \
	--name "$KV Build" --description "Compile ended at $(date) with $(out/scripts/ccversion)"
sleep 5
for i in scripts/packaging/*.zip; do
$GHREL upload --user A51-Development --repo kernel_samsung_a51 --tag "$KV" \
	--name "$(basename $i)" --file $i
done
