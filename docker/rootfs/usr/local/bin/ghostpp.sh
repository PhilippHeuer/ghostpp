#!/bin/bash

# LD Library Path
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# create environment based configuration
envsubst < /opt/ghostpp/docker.cfg > /opt/ghostpp/ghost.cfg

# create directories
mkdir -p /opt/ghostpp/data/mapcfgs /opt/ghostpp/data/maps /opt/ghostpp/data/savegames /opt/ghostpp/data/replays
# copy files
cp /opt/ghostpp/war3/scripts/blizzard.j /opt/ghostpp/data/mapcfgs/blizzard.j
cp /opt/ghostpp/war3/scripts/common.j /opt/ghostpp/data/mapcfgs/common.j

# start bot
cd /opt/ghostpp
./ghost++
