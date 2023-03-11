#!/bin/bash

set -e

which go || (echo "Go not found" && exit)
go install github.com/github-release/github-release@latest

KV=$(echo "$(out/scripts/kernelversion)" | sed 's/@.*//')
#git tag "$KV"
git push origin "$KV"

GHREL=$HOME/go/bin/github-release
$GHREL release --user A51-Development --repo kernel_samsung_a51 --tag "$KV" \
	--name "$KV Build" --description "Compile ended at $(date) With $(out/scripts/ccversion)"
sleep 5
$GHREL upload --user A51-Development --repo kernel_samsung_a51 --tag "$KV" \
	--name "$(echo $(basename scripts/packaging/*.zip) | sed 's/@.*//').zip" --file scripts/packaging/*.zip
