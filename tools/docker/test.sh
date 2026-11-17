#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."
img=megaboy-test
docker build -q -t "$img" -f tools/docker/test.Dockerfile tools/docker >/dev/null
docker run --rm --network none --user "$(id -u):$(id -g)" \
    -e ASAN_OPTIONS=detect_leaks=0 -e HOME=/tmp \
    -v "$PWD:/src:ro" "$img" sh -c '
set -eu
mkdir /tmp/w
tar -C /src --exclude=./bridge/build --exclude="*/build" -cf - . | tar -C /tmp/w -xf -
cd /tmp/w
make -C common test SAN=address,undefined
make -C gbc test SAN=address,undefined
make -C bridge/test test SAN=address,undefined
'
