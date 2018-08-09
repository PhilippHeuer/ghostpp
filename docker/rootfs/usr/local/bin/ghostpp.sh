#!/bin/bash

# LD Library Path
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# create environment based configuration
envsubst < /opt/ghostpp/docker.cfg > /opt/ghostpp/ghost.cfg

# copy files
cp /opt/ghostpp/war3/scripts/blizzard.j /opt/ghostpp/data/mapcfgs
cp /opt/ghostpp/war3/scripts/common.j /opt/ghostpp/data/mapcfgs

# start bot
cd /opt/ghostpp
./ghost++
