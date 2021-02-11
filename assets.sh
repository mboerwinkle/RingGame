#!/bin/bash
#set to correct directory
cd $(dirname $0)
#fetch assets
curl "https://789012.xyz/RingGame/RingGameAssets.tar.gz" -o RingGameAssets.tar.gz
#unpack assets
tar -xf RingGameAssets.tar.gz
#generate stars
python3 stargen/stargen.py > RingGameAssets/stars.json
#create symlinks into the client and server directories
rm -f client/assets
ln -s ../RingGameAssets client/assets
rm -f server/assets
ln -s ../RingGameAssets server/assets
