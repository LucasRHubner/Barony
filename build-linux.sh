#!/bin/sh

steamworksDir="$(readlink -f "$STEAMWORKSROOT")"

rm -rf build/
mkdir -p build/
cd build/

STEAMWORKS_ENABLED=1 STEAMWORKS_ROOT="$steamworksDir" cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENAL_ENABLED=ON \
  ..

make
