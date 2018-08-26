############################################################
# Dockerfile
############################################################

# Base Image
FROM ubuntu:18.10

############################################################
# Environment Configuration
############################################################

## bot configuration
### the log file
ENV BOT_LOG ghost.log

### the log method
###  set this to 0 if you use ghost in a container and don't want to write your logs into a file - use the default log collector in your cluster
###  set this to 1 to leave the log unlocked while GHost++ is running (may be slower, particularly on Windows)
###  set this to 2 to lock the log while GHost++ is running (may be faster, particularly on Windows)
###  note: if the log is locked you will not be able to edit/move/delete it while GHost++ is running
ENV BOT_LOGMETHOD 0

### the language file
ENV BOT_LANGUAGE language.cfg

### the path to your local Warcraft III directory
###  this path must contain war3.exe, storm.dll, and game.dll
###  if this path contains War3Patch.mpq the bot will attempt to extract "Scripts\common.j" and "Scripts\blizzard.j" on startup and write them to bot_mapcfgpath (which is defined later in this file)
###  common.j and blizzard.j are only required for automatically calculating map_crc, you do not need them if your map config files already contain map_crc
ENV BOT_WAR3PATH /opt/ghostpp/war3

### whether to act as Warcraft III: The Frozen Throne or not
###  set this to 0 to act as Warcraft III: Reign of Chaos (you WILL NOT need to enter a TFT cd key to login to battle.net)
###  set this to 1 to act as Warcraft III: The Frozen Throne (you WILL need to enter a TFT cd key to login to battle.net)
ENV BOT_TFT 1

### the address GHost++ will bind to when hosting games (leave it blank to bind to all available addresses)
###  if you don't know what this is just leave it blank
ENV BOT_BINDADDRESS ""

### the port GHost++ will host games on (this must be different from your admingame_port)
ENV BOT_HOSTPORT 6112

### whether to allow GProxy++ reliable reconnects or not
###  you should ensure that bot_synclimit is set to a reasonable value if you choose to allow GProxy++ reliable reconnects
###  a reasonable value is 5000 divided by bot_latency, e.g. if bot_latency is 100 use a value of 50 for bot_synclimit
ENV BOT_RECONNECT 0

### the port GHost++ will listen for GProxy++ reliable reconnects on
ENV BOT_RECONNECTPORT 6114

### the maximum number of minutes to wait for a GProxy++ client to reconnect to the game
###  if you set this to 0 or 1 GHost++ will wait for up to 1 minute
###  if you set this to 10 or more GHost++ will only wait for up to 10 minutes
###  due to the way GProxy++ works, increasing this value increases bandwidth requirements and CPU requirements on the players' computers
###  players can always vote to drop a player who is lagging after waiting 45 seconds regardless of this value
ENV BOT_RECONNECTWAITTIME 3

### maximum number of games to host at once
ENV BOT_MAXGAMES 5

### command trigger for ingame only (battle.net command triggers are defined later)
ENV BOT_COMMANDTRIGGER !

### the path to the directory where you keep your map config files
###  this directory can also contain common.j and blizzard.j (extracted from War3Patch.mpq)
###  common.j and blizzard.j are only required for automatically calculating map_crc, you do not need them if your map config files already contain map_crc
ENV BOT_MAPCFGPATH data/mapcfgs

### the path to the directory where you keep your savegame files
ENV BOT_SAVEGAMEPATH data/savegames

### the path to the directory where you keep your map files
###  GHost++ doesn't require map files but if it has access to them it can send them to players and automatically calculate most map config values
###  GHost++ will search [bot_mappath + map_localpath] for the map file (map_localpath is set in each map's config file)
ENV BOT_MAPPATH data/maps

### whether to save replays or not
ENV BOT_SAVEREPLAYS 0

### the path to the directory where you want GHost++ to save replays
ENV BOT_REPLAYPATH data/replays

### the Warcraft 3 version to save replays as
ENV REPLAY_WAR3VERSION 30

### the Warcraft 3 build number to save replays as (this is specific to each Warcraft 3 version)
###  patch 1.23: 		war3version 23, buildnumber 6058
###  patch 1.24 - 1.28: war3version 2X, buildnumber 6059
###  patch 1.29: 		war3version 29, buildnumber 6060
###  patch 1.30: 		war3version 30, buildnumber 6060?
ENV REPLAY_BUILDNUMBER 6060

### the bot's virtual host name as it appears in the game lobby
###  colour codes are defined by the sequence "|cFF" followed by a six character hexadecimal colour in RRGGBB format (e.g. 0000FF for pure blue)
###  the virtual host name cannot be longer than 15 characters including the colour code, if you try to go over this limit GHost++ will use the default virtual host name
ENV BOT_VIRTUALHOSTNAME |cFF4080C0GHost

### whether to hide each player's IP address from other players or not
ENV BOT_HIDEIPADDRESSES 1

### whether to check for multiple IP address usage or not
ENV BOT_CHECKMULTIPLEIPUSAGE 1

### whether to do automatic spoof checks or not
###  you can always manually spoof check by whispering the bot (and in fact this is required before running admin commands)
###  set to 0 to disable automatic spoof checks
###  set to 1 to enable automatic spoof checks on all players
###  set to 2 to enable automatic spoof checks on potential admins only
ENV BOT_SPOOFCHECKS 1

### whether to require spoof checks or not
###  this controls whether the bot will require players to spoof check before starting the game
###  it does NOT control whether the bot will require players to spoof check before running admin commands - spoof checks are ALWAYS required for admin status
###  if you require spoof checks, players will be kicked from the lobby if they haven't spoof checked within 20 seconds of joining (autohosted games only)
ENV BOT_REQUIRESPOOFCHECKS 0

### whether to consider admins and root admins as reserved players or not
###  reserved players are allowed to join full games because another player will be kicked to allow them to join
ENV BOT_RESERVEADMINS 1

### whether to display game refresh messages by default
###  this can always be changed for a particular game with the !refresh command
ENV BOT_REFRESHMESSAGES 0

### whether to automatically lock games when the owner joins
ENV BOT_AUTOLOCK 1

### whether to automatically save games when a player disconnects
###  this can always be changed for a particular game with the !autosave command
ENV BOT_AUTOSAVE 0

### whether to allow map downloads or not
###  set to 0 to disable map downloads
###  set to 1 to enable map downloads
###  set to 2 to enable conditional map downloads (an admin must start each map download with the !download or !dl command)
ENV BOT_ALLOWDOWNLOADS 1

### whether to ping players during map downloads or not
###  GHost++ will always stop pinging any players who are downloading the map
###  this config value determines whether GHost++ should stop pinging *all* players when at least one player is downloading the map
ENV BOT_PINGDURINGDOWNLOADS 1

### the maximum number of players allowed to download the map at the same time
ENV BOT_MAXDOWNLOADERS 24

### the maximum combined download speed of all players downloading the map (in KB/sec)
ENV BOT_MAXDOWNLOADSPEED 20000

### use LC style pings (divide actual pings by two)
ENV BOT_LCPINGS 1

### auto kick players with ping higher than this
ENV BOT_AUTOKICKPING 400

### the ban method
###  if bot_banmethod = 1, GHost++ will automatically reject players using a banned name
###  if bot_banmethod = 2, GHost++ will automatically reject players using a banned IP address
###  if bot_banmethod = 3, GHost++ will automatically reject players using a banned name or IP address
###  if bot_banmethod is anything else GHost++ will print a message when a banned player joins but will not automatically reject them
ENV BOT_BANMETHOD 1

### the IP blacklist file
ENV BOT_IPBLACKLISTFILE ipblacklist.txt

### automatically close the game lobby if a reserved player (or admin) doesn't join it for this many minutes
###  games which are set to automatically start when enough players join are exempt from this limit (e.g. autohosted games)
ENV BOT_LOBBYTIMELIMIT 10

### the game latency
###  this can always be changed for a particular game with the !latency command (which enforces a minimum of 20 and a maximum of 500)
ENV BOT_LATENCY 100

### the maximum number of packets a player is allowed to get out of sync by before starting the lag screen
###  before version 8.0 GHost++ did not have a lag screen which is the same as setting this to a very high number
###  this can always be changed for a particular game with the !synclimit command (which enforces a minimum of 10 and a maximum of 10000)
ENV BOT_SYNCLIMIT 50

### whether votekicks are allowed or not
ENV BOT_VOTEKICKALLOWED 1

### the percentage of players required to vote yes for a votekick to pass
###  the player starting the votekick is assumed to have voted yes and the player the votekick is started against is assumed to have voted no
###  the formula for calculating the number of votes needed is votes_needed = ceil( ( num_players - 1 ) * bot_votekickpercentage / 100 )
###  this means it will round UP the number of votes required
###  if you set it to 100 it will require 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, 9/10, 10/11, and 11/12 votes to pass
###  if you set it to 90 it will require 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, 9/10, 9/11, and 10/12 votes to pass
###  if you set it to 80 it will require 2/3, 3/4, 4/5, 4/6, 5/7, 6/8, 7/9, 8/10, 8/11, and 9/12 votes to pass
###  if you set it to 70 it will require 2/3, 3/4, 3/5, 4/6, 5/7, 5/8, 6/9, 7/10, 7/11, and 8/12 votes to pass
###  if you set it to 60 it will require 2/3, 2/4, 3/5, 3/6, 4/7, 5/8, 5/9, 6/10, 6/11, and 7/12 votes to pass
ENV BOT_VOTEKICKPERCENTAGE 70

### the default map (mapcfg will be created in-memory)
ENV BOT_DEFAULTMAP ""

### the default map config (the ".cfg" will be added automatically if you leave it out)
ENV BOT_DEFAULTMAPCFG wormwar

### the MOTD file
###  the first 8 lines of this file will be displayed when a player joins the game
###  if this file doesn't exist a default MOTD will be used
ENV BOT_MOTDFILE motd.txt

### the gameloaded file
###  the first 8 lines of this file will be displayed when the game finished loading (after the player loading times are displayed)
ENV BOT_GAMELOADEDFILE gameloaded.txt

### the gameover file
###  the first 8 lines of this file will be displayed when the game is over
###  this only works when using a stats class - note: at the time of this writing the only stats class is for DotA maps
ENV BOT_GAMEOVERFILE gameover.txt

### the "TCP no delay" flag
###  this controls whether or not your operating system should use the "no delay" algorithm on game sockets
###  the algorithm is designed to reduce latency by sending data in small packets as soon as possible rather than waiting to send a single larger packet
###  enabling this algorithm requires additional bandwidth because it is a less efficient way of sending data
###  however, it may reduce game latencies in some cases
ENV TCP_NODELAY 1

### the matchmaking method
###  this controls how the bot matches players when they join the game when using !autohostmm
###  set it to 0 to disable matchmaking (first come first served, even if their scores are very different)
###  set it to 1 to use the "furthest score" method (the player with the furthest score from the average is kicked to make room for another player)
###  set it to 2 to use the "lowest score" method (the player with the lowest score is kicked to make room for another player)
ENV BOT_MATCHMAKINGMETHOD 1

### the mapgametype overwrite (advance)
###  This controls the mapgametype overwrite. Use with caution as this can result in an ipban from battle.net services or make users unavailable to join your bot with an invalid number
###  Example numbers can be found at (http://www.codelain.com/forum/index.php?topic=11373.msg135301#msg135301)
###  set it to 0 to disable mapgametype overwrite
ENV BOT_MAPGAMETYPE 0

### GeoIP File
ENV BOT_GEOIPFILE /opt/ghostpp/geoip.dat

## LAN CONFIGURATION
### the Warcraft 3 version to use when broadcasting LAN games
ENV LAN_WAR3VERSION 30

### the UDP broadcast target
###  if this value is blank the bot will try to broadcast LAN games on the default interface which is chosen by your operating system
###  sometimes your operating system will choose the wrong interface when more than one exists
###  therefore you can use this value to force the bot to use a specific interface
###  for example you may set it to "192.168.1.255" to broadcast LAN games to the 192.168.1.x subnet
ENV UDP_BROADCASTTARGET ""

### the UDP "don't route" flag
ENV UDP_DONTROUTE 0

## AUTOHOST CONFIGURATION
### this section of the config file is for enabling autohost when the bot starts up without having to issue a command
### you can activate the autohost feature without changing anything here by using the !autohost command
ENV AUTOHOST_MAXGAMES 0
ENV AUTOHOST_STARTPLAYERS 0
ENV AUTOHOST_GAMENAME ""
ENV AUTOHOST_OWNER ""
ENV AUTOHOST_RANDOMMAP_TYPE "none"
ENV AUTOHOST_RANDOMMAP_LIST ""

## Database Configuration
### database type
###  use "sqlite3" for a local SQLite database
###  use "mysql" for any MySQL database
ENV DB_TYPE sqlite3

### sqlite3 database configuration
###  this is only used if your database type is SQLite
ENV DB_SQLITE3_FILE ghost.dbs

### mysql database configuration
###  this is only used if your database type is MySQL
ENV DB_MYSQL_SERVER localhost
ENV DB_MYSQL_DATABASE ghost
ENV DB_MYSQL_USER YOUR_USERNAME
ENV DB_MYSQL_PASSWORD YOUR_PASSWORD
ENV DB_MYSQL_PORT 0

### the bot ID is included each time the bot adds data to the MySQL database
### it is used to identify where each row of data came from when you configure multiple bots to connect to the same MySQL database
### GHost++ does not use the bot ID number itself, it's just to help you keep track of the data in your database
ENV DB_MYSQL_BOTID 1

## BNet Connection
### Connection: 1st
ENV BNET_SERVER uswest.battle.net
ENV BNET_SERVERALIAS USWest
ENV BNET_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET_LOCALE system
ENV BNET_USERNAME ""
ENV BNET_PASSWORD ""
ENV BNET_FIRSTCHANNEL "The Void"
ENV BNET_ROOTADMIN ""
ENV BNET_COMMANDTRIGGER "!"
ENV BNET_HOLDFRIENDS 1
ENV BNET_HOLDCLAN 1
ENV BNET_PUBLICCOMMANDS 1
ENV BNET_BNLSSERVER pyro.no-ip.biz
ENV BNET_BNLSPORT 9367
ENV BNET_BNLSWARDENCOOKIE 2
ENV BNET_CUSTOM_WAR3VERSION 30
ENV BNET_CUSTOM_EXEVERSION ""
ENV BNET_CUSTOM_EXEVERSIONHASH ""
ENV BNET_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 2nd
ENV BNET2_SERVER uswest.battle.net
ENV BNET2_SERVERALIAS USWest
ENV BNET2_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET2_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET2_LOCALE system
ENV BNET2_USERNAME ""
ENV BNET2_PASSWORD ""
ENV BNET2_FIRSTCHANNEL "The Void"
ENV BNET2_ROOTADMIN ""
ENV BNET2_COMMANDTRIGGER "!"
ENV BNET2_HOLDFRIENDS 1
ENV BNET2_HOLDCLAN 1
ENV BNET2_PUBLICCOMMANDS 1
ENV BNET2_BNLSSERVER pyro.no-ip.biz
ENV BNET2_BNLSPORT 9367
ENV BNET2_BNLSWARDENCOOKIE 2
ENV BNET2_CUSTOM_WAR3VERSION 30
ENV BNET2_CUSTOM_EXEVERSION ""
ENV BNET2_CUSTOM_EXEVERSIONHASH ""
ENV BNET2_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET2_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 3rd
ENV BNET3_SERVER uswest.battle.net
ENV BNET3_SERVERALIAS USWest
ENV BNET3_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET3_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET3_LOCALE system
ENV BNET3_USERNAME ""
ENV BNET3_PASSWORD ""
ENV BNET3_FIRSTCHANNEL "The Void"
ENV BNET3_ROOTADMIN ""
ENV BNET3_COMMANDTRIGGER "!"
ENV BNET3_HOLDFRIENDS 1
ENV BNET3_HOLDCLAN 1
ENV BNET3_PUBLICCOMMANDS 1
ENV BNET3_BNLSSERVER pyro.no-ip.biz
ENV BNET3_BNLSPORT 9367
ENV BNET3_BNLSWARDENCOOKIE 2
ENV BNET3_CUSTOM_WAR3VERSION 30
ENV BNET3_CUSTOM_EXEVERSION ""
ENV BNET3_CUSTOM_EXEVERSIONHASH ""
ENV BNET3_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET3_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 4th
ENV BNET4_SERVER uswest.battle.net
ENV BNET4_SERVERALIAS USWest
ENV BNET4_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET4_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET4_LOCALE system
ENV BNET4_USERNAME ""
ENV BNET4_PASSWORD ""
ENV BNET4_FIRSTCHANNEL "The Void"
ENV BNET4_ROOTADMIN ""
ENV BNET4_COMMANDTRIGGER "!"
ENV BNET4_HOLDFRIENDS 1
ENV BNET4_HOLDCLAN 1
ENV BNET4_PUBLICCOMMANDS 1
ENV BNET4_BNLSSERVER pyro.no-ip.biz
ENV BNET4_BNLSPORT 9367
ENV BNET4_BNLSWARDENCOOKIE 2
ENV BNET4_CUSTOM_WAR3VERSION 30
ENV BNET4_CUSTOM_EXEVERSION ""
ENV BNET4_CUSTOM_EXEVERSIONHASH ""
ENV BNET4_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET4_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 5th
ENV BNET5_SERVER uswest.battle.net
ENV BNET5_SERVERALIAS USWest
ENV BNET5_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET5_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET5_LOCALE system
ENV BNET5_USERNAME ""
ENV BNET5_PASSWORD ""
ENV BNET5_FIRSTCHANNEL "The Void"
ENV BNET5_ROOTADMIN ""
ENV BNET5_COMMANDTRIGGER "!"
ENV BNET5_HOLDFRIENDS 1
ENV BNET5_HOLDCLAN 1
ENV BNET5_PUBLICCOMMANDS 1
ENV BNET5_BNLSSERVER pyro.no-ip.biz
ENV BNET5_BNLSPORT 9367
ENV BNET5_BNLSWARDENCOOKIE 2
ENV BNET5_CUSTOM_WAR3VERSION 30
ENV BNET5_CUSTOM_EXEVERSION ""
ENV BNET5_CUSTOM_EXEVERSIONHASH ""
ENV BNET5_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET5_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 6th
ENV BNET6_SERVER uswest.battle.net
ENV BNET6_SERVERALIAS USWest
ENV BNET6_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET6_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET6_LOCALE system
ENV BNET6_USERNAME ""
ENV BNET6_PASSWORD ""
ENV BNET6_FIRSTCHANNEL "The Void"
ENV BNET6_ROOTADMIN ""
ENV BNET6_COMMANDTRIGGER "!"
ENV BNET6_HOLDFRIENDS 1
ENV BNET6_HOLDCLAN 1
ENV BNET6_PUBLICCOMMANDS 1
ENV BNET6_BNLSSERVER pyro.no-ip.biz
ENV BNET6_BNLSPORT 9367
ENV BNET6_BNLSWARDENCOOKIE 2
ENV BNET6_CUSTOM_WAR3VERSION 30
ENV BNET6_CUSTOM_EXEVERSION ""
ENV BNET6_CUSTOM_EXEVERSIONHASH ""
ENV BNET6_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET6_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 7th
ENV BNET7_SERVER uswest.battle.net
ENV BNET7_SERVERALIAS USWest
ENV BNET7_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET7_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET7_LOCALE system
ENV BNET7_USERNAME ""
ENV BNET7_PASSWORD ""
ENV BNET7_FIRSTCHANNEL "The Void"
ENV BNET7_ROOTADMIN ""
ENV BNET7_COMMANDTRIGGER "!"
ENV BNET7_HOLDFRIENDS 1
ENV BNET7_HOLDCLAN 1
ENV BNET7_PUBLICCOMMANDS 1
ENV BNET7_BNLSSERVER pyro.no-ip.biz
ENV BNET7_BNLSPORT 9367
ENV BNET7_BNLSWARDENCOOKIE 2
ENV BNET7_CUSTOM_WAR3VERSION 30
ENV BNET7_CUSTOM_EXEVERSION ""
ENV BNET7_CUSTOM_EXEVERSIONHASH ""
ENV BNET7_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET7_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 8th
ENV BNET8_SERVER uswest.battle.net
ENV BNET8_SERVERALIAS USWest
ENV BNET8_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET8_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET8_LOCALE system
ENV BNET8_USERNAME ""
ENV BNET8_PASSWORD ""
ENV BNET8_FIRSTCHANNEL "The Void"
ENV BNET8_ROOTADMIN ""
ENV BNET8_COMMANDTRIGGER "!"
ENV BNET8_HOLDFRIENDS 1
ENV BNET8_HOLDCLAN 1
ENV BNET8_PUBLICCOMMANDS 1
ENV BNET8_BNLSSERVER pyro.no-ip.biz
ENV BNET8_BNLSPORT 9367
ENV BNET8_BNLSWARDENCOOKIE 2
ENV BNET8_CUSTOM_WAR3VERSION 30
ENV BNET8_CUSTOM_EXEVERSION ""
ENV BNET8_CUSTOM_EXEVERSIONHASH ""
ENV BNET8_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET8_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 9th
ENV BNET9_SERVER uswest.battle.net
ENV BNET9_SERVERALIAS USWest
ENV BNET9_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET9_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET9_LOCALE system
ENV BNET9_USERNAME ""
ENV BNET9_PASSWORD ""
ENV BNET9_FIRSTCHANNEL "The Void"
ENV BNET9_ROOTADMIN ""
ENV BNET9_COMMANDTRIGGER "!"
ENV BNET9_HOLDFRIENDS 1
ENV BNET9_HOLDCLAN 1
ENV BNET9_PUBLICCOMMANDS 1
ENV BNET9_BNLSSERVER pyro.no-ip.biz
ENV BNET9_BNLSPORT 9367
ENV BNET9_BNLSWARDENCOOKIE 2
ENV BNET9_CUSTOM_WAR3VERSION 30
ENV BNET9_CUSTOM_EXEVERSION ""
ENV BNET9_CUSTOM_EXEVERSIONHASH ""
ENV BNET9_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET9_CUSTOM_PVPGNREALMNAME "PvPGN Realm"
### Connection: 10th
ENV BNET10_SERVER uswest.battle.net
ENV BNET10_SERVERALIAS USWest
ENV BNET10_CDKEYROC FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET10_CDKEYTFT FFFFFFFFFFFFFFFFFFFFFFFFFF
ENV BNET10_LOCALE system
ENV BNET10_USERNAME ""
ENV BNET10_PASSWORD ""
ENV BNET10_FIRSTCHANNEL "The Void"
ENV BNET10_ROOTADMIN ""
ENV BNET10_COMMANDTRIGGER "!"
ENV BNET10_HOLDFRIENDS 1
ENV BNET10_HOLDCLAN 1
ENV BNET10_PUBLICCOMMANDS 1
ENV BNET10_BNLSSERVER pyro.no-ip.biz
ENV BNET10_BNLSPORT 9367
ENV BNET10_BNLSWARDENCOOKIE 2
ENV BNET10_CUSTOM_WAR3VERSION 30
ENV BNET10_CUSTOM_EXEVERSION ""
ENV BNET10_CUSTOM_EXEVERSIONHASH ""
ENV BNET10_CUSTOM_PASSWORDHASHTYPE ""
ENV BNET10_CUSTOM_PVPGNREALMNAME "PvPGN Realm"

############################################################
# Installation
############################################################

# Install packages. Notes:
#   * git: a git client to check out repositories
ENV PACKAGES="\
  gettext-base \
  curl \
  wget \
  unzip \
  gzip \
  build-essential \
  libgeoip-dev \
  libmysqlclient-dev \
  libboost-all-dev \
  libcurl4-gnutls-dev \
"

# Packages
RUN echo "Installing Packages ..." &&\
	# Update Package List
	apt-get update &&\
	# Package Install [no-cache, because the cache would be within the build - bloating up the file size]
	apt-get install -y $PACKAGES

# Copy Artifacts
ADD rootfs /

# Configuration
RUN echo "Configuration ..." &&\
	# Ghost++ Data 
	cd /opt/ghostpp &&\
	curl -o /opt/ghostpp/GeoIP.dat.gz http://geolite.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz &&\
	gunzip /opt/ghostpp/GeoIP.dat.gz &&\ 
	mv /opt/ghostpp/GeoIP.dat /opt/ghostpp/geoip.dat &&\
	# War3 Data
	curl -o "/opt/ghostpp/war3/Warcraft III.exe" https://gitlab.com/PhilippHeuer/war3-data/raw/master/1.30.0/Warcraft%20III.exe?inline=false &&\
	curl -o "/opt/ghostpp/war3/scripts.zip" https://gitlab.com/PhilippHeuer/war3-data/raw/master/1.30.0/scripts.zip?inline=false &&\
	unzip /opt/ghostpp/war3/scripts.zip -d /opt/ghostpp/war3 &&\
	# Permissions
	chmod +x /usr/local/bin/ghostpp.sh &&\
	chmod +x /opt/ghostpp/ghost++

# Build CleanUp
RUN echo "CleanUp ..."

############################################################
# Execution
############################################################
CMD [ "bash", "-c", "/usr/local/bin/ghostpp.sh"]
