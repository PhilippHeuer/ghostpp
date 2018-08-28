# GHost++

## Description

This fork builds a ready-to-run docker container around the ghost++ source.

Additionally it contains the following changes:

### Commands

| Command       | Alias         | Description                   | Permissions    | Credits        |
| ------------- |:-------------:| -----------------------------:| --------------:| --------------:|
| startnow      | sn            | Start the game instantly      | Admin          | @maxemann96    |
| startin       | si            | Start the game in x seconds   | Admin          | @maxemann96    |
| votestart     | vs            | Vote to start the game        | Everyone       | @maxemann96    |
| dlmap         |               | Download a map based on a url | Rootadmin      |                |

### Features

* Support to host random maps when using autohost to have bots host random maps of a specific gametype.

## Upstream Projects

GHost++ is no longer actively maintained by it's original creators. Nevertheless, GHost++ remains widely used for hosting Warcraft III games, and there are no major known bugs.

Here are other ghostpp projects:

* [ghostpp](https://github.com/uakfdotb/ghostpp): upstream project, official ghostpp repository
* [Aura](https://github.com/Josko/aura-bot/): simple bot to host games from a server, with greatly modernized core but with many features (MySQL support, autohosting) stripped out
* [maxemann96/ghostpp](https://github.com/maxemann96/ghostpp): a few additional features like votestart, commands from terminal

## Getting Started

### Plain Docker

In order to run the Ghost++ Bot, use the following command and fill the values:

```sh
docker run -d --restart=unless-stopped \
    --name=ghostpp-alpha \
    -p 6112:6112/tcp \
    -v {FOLDER}:/opt/ghostpp/data \
    --env DB_TYPE="mysql" \
    --env DB_MYSQL_SERVER="localhost" \
    --env DB_MYSQL_DATABASE="bot" \
    --env DB_MYSQL_USER="username" \
    --env DB_MYSQL_PASSWORD="userpw" \
    --env DB_MYSQL_PORT="3306" \
    --env DB_MYSQL_BOTID="1" \
    --env BOT_HOSTPORT="6112" \
    --env BNET_SERVER="europe.battle.net" \
    --env BNET_SERVERALIAS="Europe" \
    --env BNET_CDKEYROC="FFF..." \
    --env BNET_CDKEYTFT="FFF..." \
    --env BNET_USERNAME="bnetUser" \
    --env BNET_PASSWORD="bnetPW" \
    --env BNET_FIRSTCHANNEL="test" \
    --env BNET_ROOTADMIN="yourBnetAccount" \
    --env BNET_COMMANDTRIGGER="!" \
    registry.gitlab.com/docker-gameserver/ghostpp:master
```
