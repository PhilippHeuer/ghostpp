/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "ghost.h"
#include "util.h"
#include "crc32.h"
#include "sha1.h"
#include "csvparser.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "ghostdbsqlite.h"
#include "ghostdbmysql.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game_base.h"
#include "game.h"

#include <signal.h>
#include <stdlib.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

#ifdef WIN32
 #include <ws2tcpip.h>		// for WSAIoctl
#endif

#define __STORMLIB_SELF__
#include <stormlib/StormLib.h>

#ifdef WIN32
 #include <windows.h>
 #include <winsock.h>
#endif

#include <time.h>

#ifndef WIN32
 #include <sys/time.h>
#endif

#ifdef __APPLE__
 #include <mach/mach_time.h>
#endif

string gCFGFile;
string gLogFile;
string gLogLevel;
CGHost *gGHost = NULL;

uint32_t GetTime( )
{
	return GetTicks( ) / 1000;
}

uint32_t GetTicks( )
{
#ifdef WIN32
	// don't use GetTickCount anymore because it's not accurate enough (~16ms resolution)
	// don't use QueryPerformanceCounter anymore because it isn't guaranteed to be strictly increasing on some systems and thus requires "smoothing" code
	// use timeGetTime instead, which typically has a high resolution (5ms or more) but we request a lower resolution on startup

	return timeGetTime( );
#elif __APPLE__
	uint64_t current = mach_absolute_time( );
	static mach_timebase_info_data_t info = { 0, 0 };
	// get timebase info
	if( info.denom == 0 )
		mach_timebase_info( &info );
	uint64_t elapsednano = current * ( info.numer / info.denom );
	// convert ns to ms
	return elapsednano / 1e6;
#else
	uint32_t ticks;
	struct timespec t;
	clock_gettime( CLOCK_MONOTONIC, &t );
	ticks = t.tv_sec * 1000;
	ticks += t.tv_nsec / 1000000;
	return ticks;
#endif
}

void SignalCatcher2( int s )
{
	BOOST_LOG_TRIVIAL(fatal) << "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting NOW";

	if( gGHost )
	{
		if( gGHost->m_Exiting )
			exit( 1 );
		else
			gGHost->m_Exiting = true;
	}
	else
		exit( 1 );
}

void SignalCatcher( int s )
{
	// signal( SIGABRT, SignalCatcher2 );
	signal( SIGINT, SignalCatcher2 );

	BOOST_LOG_TRIVIAL(fatal) << "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting nicely";

	if( gGHost )
		gGHost->m_ExitingNice = true;
	else
		exit( 1 );
}

//
// main
//

int main( int argc, char **argv )
{
	// logging
	boost::log::add_common_attributes();
	boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");

	// - console
	boost::log::add_console_log(std::cout, boost::log::keywords::format = "[%TimeStamp%][%Severity%]: %Message%");

	// initialize random number generator
	srand( time( NULL ) );

	// arguments
	gCFGFile = "ghost.cfg";

	if( argc > 1 && argv[1] )
		gCFGFile = argv[1];

	// read config file

	CConfig CFG;
	CFG.Read( "default.cfg" );
	CFG.Read( gCFGFile );
	gLogFile = CFG.GetString( "bot_log", string( ) );
	gLogLevel = CFG.GetString( "bot_loglevel", "INFO" );

	
	// logging - logfile
	if( !gLogFile.empty( ) )
	{
		// configure logfile
		boost::log::add_file_log(
        	boost::log::keywords::target = "logs/", boost::log::keywords::file_name = gLogFile + "_%y%m%d_%3N.log",
        	boost::log::keywords::rotation_size = 10 * 1024 * 1024,
        	boost::log::keywords::scan_method = boost::log::sinks::file::scan_matching,
			boost::log::keywords::format = "[%TimeStamp%][%Severity%]: %Message%");
	}

	// - loglevel
	if ( boost::iequals(gLogLevel, "TRACE") )
	{
		boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::trace );
	}
	else if ( boost::iequals(gLogLevel, "DEBUG") )
	{
		boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::debug );
	}
	else if ( boost::iequals(gLogLevel, "INFO") )
	{
		boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
	}
	else if ( boost::iequals(gLogLevel, "WARNING") )
	{
		boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::warning );
	}
	else if ( boost::iequals(gLogLevel, "ERROR") )
	{
		boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::error );
	}
	else if ( boost::iequals(gLogLevel, "FATAL") )
	{
		boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::fatal );
	}

	BOOST_LOG_TRIVIAL(info) << "[GHOST] starting up";
	
	// catch SIGABRT and SIGINT

	// signal( SIGABRT, SignalCatcher );
	signal( SIGINT, SignalCatcher );

#ifndef WIN32
	// disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

	signal( SIGPIPE, SIG_IGN );
#endif

#ifdef WIN32
	// initialize timer resolution
	// attempt to set the resolution as low as possible from 1ms to 5ms

	unsigned int TimerResolution = 0;

	for( unsigned int i = 1; i <= 5; ++i )
	{
		if( timeBeginPeriod( i ) == TIMERR_NOERROR )
		{
			TimerResolution = i;
			break;
		}
		else if( i < 5 )
			BOOST_LOG_TRIVIAL(error) << "[GHOST] error setting Windows timer resolution to " + UTIL_ToString( i ) + " milliseconds, trying a higher resolution";
		else
		{
			BOOST_LOG_TRIVIAL(error) << "[GHOST] error setting Windows timer resolution";
			return 1;
		}
	}

	BOOST_LOG_TRIVIAL(info) << "[GHOST] using Windows timer with resolution " + UTIL_ToString( TimerResolution ) + " milliseconds";
#elif __APPLE__
	// not sure how to get the resolution
#else
	// print the timer resolution

	struct timespec Resolution;

	if( clock_getres( CLOCK_MONOTONIC, &Resolution ) == -1 )
		BOOST_LOG_TRIVIAL(error) << "[GHOST] error getting monotonic timer resolution";
	else
		BOOST_LOG_TRIVIAL(info) << "[GHOST] using monotonic timer with resolution " + UTIL_ToString( (double)( Resolution.tv_nsec / 1000 ), 2 ) + " microseconds";
#endif

#ifdef WIN32
	// initialize winsock

	BOOST_LOG_TRIVIAL(info) << "[GHOST] starting winsock";
	WSADATA wsadata;

	if( WSAStartup( MAKEWORD( 2, 2 ), &wsadata ) != 0 )
	{
		BOOST_LOG_TRIVIAL(fatal) << "[GHOST] error starting winsock";
		return 1;
	}

	// increase process priority

	BOOST_LOG_TRIVIAL(info) << "[GHOST] setting process priority to \"above normal\"";
	SetPriorityClass( GetCurrentProcess( ), ABOVE_NORMAL_PRIORITY_CLASS );
#endif

	// initialize ghost

	gGHost = new CGHost( &CFG );

	while( 1 )
	{
		// block for 50ms on all sockets - if you intend to perform any timed actions more frequently you should change this
		// that said it's likely we'll loop more often than this due to there being data waiting on one of the sockets but there aren't any guarantees

		if( gGHost->Update( 50000 ) )
			break;
	}

	// shutdown ghost

	BOOST_LOG_TRIVIAL(info) << "[GHOST] shutting down";
	delete gGHost;
	gGHost = NULL;

#ifdef WIN32
	// shutdown winsock

	BOOST_LOG_TRIVIAL(info) << "[GHOST] shutting down winsock";
	WSACleanup( );

	// shutdown timer

	timeEndPeriod( TimerResolution );
#endif

	return 0;
}

//
// CGHost
//

CGHost :: CGHost( CConfig *CFG )
{
	m_UDPSocket = new CUDPSocket( );
	m_UDPSocket->SetBroadcastTarget( CFG->GetString( "udp_broadcasttarget", string( ) ) );
	m_UDPSocket->SetDontRoute( CFG->GetInt( "udp_dontroute", 0 ) == 0 ? false : true );
	m_ReconnectSocket = NULL;
	m_GPSProtocol = new CGPSProtocol( );
	m_CRC = new CCRC32( );
	m_CRC->Initialize( );
	m_SHA = new CSHA1( );
	m_CurrentGame = NULL;
	string DBType = CFG->GetString( "db_type", "sqlite3" );
	BOOST_LOG_TRIVIAL(info) << "[GHOST] opening primary database";

	if( DBType == "mysql" )
	{
#ifdef GHOST_MYSQL
		m_DB = new CGHostDBMySQL( CFG );
#else
		BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - this binary was not compiled with MySQL database support, using SQLite database instead";
		m_DB = new CGHostDBSQLite( CFG );
#endif
	}
	else
		m_DB = new CGHostDBSQLite( CFG );

	BOOST_LOG_TRIVIAL(info) << "[GHOST] opening secondary (local) database";
	m_DBLocal = new CGHostDBSQLite( CFG );

	// get a list of local IP addresses
	// this list is used elsewhere to determine if a player connecting to the bot is local or not

	BOOST_LOG_TRIVIAL(info) << "[GHOST] attempting to find local IP addresses";

#ifdef WIN32
	// use a more reliable Windows specific method since the portable method doesn't always work properly on Windows
	// code stolen from: http://tangentsoft.net/wskfaq/examples/getifaces.html

	SOCKET sd = WSASocket( AF_INET, SOCK_DGRAM, 0, 0, 0, 0 );

	if( sd == SOCKET_ERROR )
		BOOST_LOG_TRIVIAL(warning) << "[GHOST] error finding local IP addresses - failed to create socket (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")";
	else
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;

		if( WSAIoctl( sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList, sizeof(InterfaceList), &nBytesReturned, 0, 0 ) == SOCKET_ERROR )
			BOOST_LOG_TRIVIAL(warning) << "[GHOST] error finding local IP addresses - WSAIoctl failed (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")";
		else
		{
			int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

			for( int i = 0; i < nNumInterfaces; ++i )
			{
				sockaddr_in *pAddress;
				pAddress = (sockaddr_in *)&(InterfaceList[i].iiAddress);
				BOOST_LOG_TRIVIAL(info) << "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( pAddress->sin_addr ) ) + "]";
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)pAddress->sin_addr.s_addr, false ) );
			}
		}

		closesocket( sd );
	}
#else
	// use a portable method

	char HostName[255];

	if( gethostname( HostName, 255 ) == SOCKET_ERROR )
		BOOST_LOG_TRIVIAL(warning) << "[GHOST] error finding local IP addresses - failed to get local hostname";
	else
	{
		BOOST_LOG_TRIVIAL(info) << "[GHOST] local hostname is [" + string( HostName ) + "]";
		struct hostent *HostEnt = gethostbyname( HostName );

		if( !HostEnt )
			BOOST_LOG_TRIVIAL(warning) << "[GHOST] error finding local IP addresses - gethostbyname failed";
		else
		{
			for( int i = 0; HostEnt->h_addr_list[i] != NULL; ++i )
			{
				struct in_addr Address;
				memcpy( &Address, HostEnt->h_addr_list[i], sizeof(struct in_addr) );
				BOOST_LOG_TRIVIAL(info) << "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( Address ) ) + "]";
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)Address.s_addr, false ) );
			}
		}
	}
#endif

	m_Language = NULL;
	m_Exiting = false;
	m_ExitingNice = false;
	m_Enabled = true;
	m_Version = "17.2";
	m_HostCounter = 1;
	m_AutoHostMaximumGames = CFG->GetInt( "autohost_maxgames", 0 );
	m_AutoHostAutoStartPlayers = CFG->GetInt( "autohost_startplayers", 0 );
	m_AutoHostGameName = CFG->GetString( "autohost_gamename", string( ) );
	m_AutoHostOwner = CFG->GetString( "autohost_owner", string( ) );
	m_LastAutoHostTime = GetTime( );
	m_AutoHostMatchMaking = false;
	m_AutoHostMinimumScore = 0.0;
	m_AutoHostMaximumScore = 0.0;
	m_AutoHostRandomizeMapType = CFG->GetString( "autohost_randommap_type", string( "none" ) );
	m_AutoHostRamdomizeMapList = CFG->GetString( "autohost_randommap_list", string( ) );
	m_AllGamesFinished = false;
	m_AllGamesFinishedTime = 0;
	m_TFT = CFG->GetInt( "bot_tft", 1 ) == 0 ? false : true;

	if( m_TFT )
		BOOST_LOG_TRIVIAL(info) << "[GHOST] acting as Warcraft III: The Frozen Throne";
	else
		BOOST_LOG_TRIVIAL(info) << "[GHOST] acting as Warcraft III: Reign of Chaos";

	m_HostPort = CFG->GetInt( "bot_hostport", 6112 );
	m_Reconnect = CFG->GetInt( "bot_reconnect", 1 ) == 0 ? false : true;
	m_ReconnectPort = CFG->GetInt( "bot_reconnectport", 6114 );
	m_DefaultMap = CFG->GetString( "bot_defaultmap", "" );
	m_DefaultMapCfg = CFG->GetString( "bot_defaultmapcfg", "" );
	m_LANWar3Version = CFG->GetInt( "lan_war3version", 30 );
	m_ReplayWar3Version = CFG->GetInt( "replay_war3version", 30 );
	m_ReplayBuildNumber = CFG->GetInt( "replay_buildnumber", 6060 );
	SetConfigs( CFG );

	// load the battle.net connections
	// we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)

	for( uint32_t i = 1; i < 10; ++i )
	{
		string Prefix;

		if( i == 1 )
			Prefix = "bnet_";
		else
			Prefix = "bnet" + UTIL_ToString( i ) + "_";

		string Server = CFG->GetString( Prefix + "server", string( ) );
		string ServerAlias = CFG->GetString( Prefix + "serveralias", string( ) );
		string CDKeyROC = CFG->GetString( Prefix + "cdkeyroc", string( ) );
		string CDKeyTFT = CFG->GetString( Prefix + "cdkeytft", string( ) );
		string CountryAbbrev = CFG->GetString( Prefix + "countryabbrev", "USA" );
		string Country = CFG->GetString( Prefix + "country", "United States" );
		string Locale = CFG->GetString( Prefix + "locale", "system" );
		uint32_t LocaleID;

		if( Locale == "system" )
		{
#ifdef WIN32
			LocaleID = GetUserDefaultLangID( );
#else
			LocaleID = 1033;
#endif
		}
		else
			LocaleID = UTIL_ToUInt32( Locale );

		string UserName = CFG->GetString( Prefix + "username", string( ) );
		string UserPassword = CFG->GetString( Prefix + "password", string( ) );
		string FirstChannel = CFG->GetString( Prefix + "firstchannel", "The Void" );
		string RootAdmin = CFG->GetString( Prefix + "rootadmin", string( ) );
		string BNETCommandTrigger = CFG->GetString( Prefix + "commandtrigger", "!" );

		if( BNETCommandTrigger.empty( ) )
			BNETCommandTrigger = "!";

		bool HoldFriends = CFG->GetInt( Prefix + "holdfriends", 1 ) == 0 ? false : true;
		bool HoldClan = CFG->GetInt( Prefix + "holdclan", 1 ) == 0 ? false : true;
		bool PublicCommands = CFG->GetInt( Prefix + "publiccommands", 1 ) == 0 ? false : true;
		string BNLSServer = CFG->GetString( Prefix + "bnlsserver", string( ) );
		int BNLSPort = CFG->GetInt( Prefix + "bnlsport", 9367 );
		int BNLSWardenCookie = CFG->GetInt( Prefix + "bnlswardencookie", 0 );
		unsigned char War3Version = CFG->GetInt( Prefix + "custom_war3version", 30 );
		BYTEARRAY EXEVersion = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversion", string( ) ), 4 );
		BYTEARRAY EXEVersionHash = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversionhash", string( ) ), 4 );
		string PasswordHashType = CFG->GetString( Prefix + "custom_passwordhashtype", string( ) );
		string PVPGNRealmName = CFG->GetString( Prefix + "custom_pvpgnrealmname", "PvPGN Realm" );
		uint32_t MaxMessageLength = CFG->GetInt( Prefix + "custom_maxmessagelength", 200 );

		if( Server.empty( ) )
			break;

		if( CDKeyROC.empty( ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] missing " + Prefix + "cdkeyroc, skipping this battle.net connection";
			continue;
		}

		if( m_TFT && CDKeyTFT.empty( ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] missing " + Prefix + "cdkeytft, skipping this battle.net connection";
			continue;
		}

		if( UserName.empty( ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] missing " + Prefix + "username, skipping this battle.net connection";
			continue;
		}

		if( UserPassword.empty( ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] missing " + Prefix + "password, skipping this battle.net connection";
			continue;
		}

		BOOST_LOG_TRIVIAL(info) << "[GHOST] found battle.net connection #" + UTIL_ToString( i ) + " for server [" + Server + "]";

		if( Locale == "system" )
		{
#ifdef WIN32
			BOOST_LOG_TRIVIAL(info) << "[GHOST] using system locale of " + UTIL_ToString( LocaleID );
#else
			BOOST_LOG_TRIVIAL(info) << "[GHOST] unable to get system locale, using default locale of 1033";
#endif
		}

		m_BNETs.push_back( new CBNET( this, Server, ServerAlias, BNLSServer, (uint16_t)BNLSPort, (uint32_t)BNLSWardenCookie, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, LocaleID, UserName, UserPassword, FirstChannel, RootAdmin, BNETCommandTrigger[0], HoldFriends, HoldClan, PublicCommands, War3Version, EXEVersion, EXEVersionHash, PasswordHashType, PVPGNRealmName, MaxMessageLength, i ) );
	}

	if( m_BNETs.empty( ) )
		BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - no battle.net connections found in config file";

	// load the default maps

	if ( !m_DefaultMap.empty() )
	{
		// load map - no user name / whisper since no user is triggering this event
		BOOST_LOG_TRIVIAL(info) << "[GHOST] initial map specified in bot_defaultmap [" + m_DefaultMap + "]";
		LoadMap( m_DefaultMap, NULL, "", false );
	}
	else if ( !m_DefaultMapCfg.empty() )
	{
		// load map - no user name / whisper since no user is triggering this event
		BOOST_LOG_TRIVIAL(info) << "[GHOST] initial map specified in bot_defaultmapcfg [" + m_DefaultMapCfg + "]";
		LoadMapConfig( m_DefaultMapCfg, NULL, "", false );
	}

	// if default map couldn't be loaded, use the default wormwar
	if ( m_Map == NULL )
	{
		BOOST_LOG_TRIVIAL(warning) << "[GHOST] error - no default map configuration could be loaded";
	}

	m_AutoHostMap = new CMap( *m_Map );
	m_SaveGame = new CSaveGame( );

	// load the iptocountry data

	LoadIPToCountryData( );

	if( m_BNETs.empty( ) )
		BOOST_LOG_TRIVIAL(warning) << "[GHOST] warning - no battle.net connections found";

#ifdef GHOST_MYSQL
	BOOST_LOG_TRIVIAL(info) << "[GHOST] GHost++ Version " + m_Version + " (with MySQL support)";
#else
	BOOST_LOG_TRIVIAL(info) << "[GHOST] GHost++ Version " + m_Version + " (without MySQL support)";
#endif
}

CGHost :: ~CGHost( )
{
	delete m_UDPSocket;
	delete m_ReconnectSocket;

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); ++i )
		delete *i;

	delete m_GPSProtocol;
	delete m_CRC;
	delete m_SHA;

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		delete *i;

	if( m_CurrentGame )
		m_CurrentGame->doDelete();

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
		(*i)->doDelete();

	delete m_DB;
	delete m_DBLocal;

	// warning: we don't delete any entries of m_Callables here because we can't be guaranteed that the associated threads have terminated
	// this is fine if the program is currently exiting because the OS will clean up after us
	// but if you try to recreate the CGHost object within a single session you will probably leak resources!

	if( !m_Callables.empty( ) )
		BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - " + UTIL_ToString( m_Callables.size( ) ) + " orphaned callables were leaked (this is not an error)";

	delete m_Language;
	delete m_Map;
	delete m_AutoHostMap;
	delete m_SaveGame;
}

bool CGHost :: Update( long usecBlock )
{
	// todotodo: do we really want to shutdown if there's a database error? is there any way to recover from this?

	if( m_DB->HasError( ) )
	{
		BOOST_LOG_TRIVIAL(warning) << "[GHOST] database error - " + m_DB->GetError( );
		return true;
	}

	if( m_DBLocal->HasError( ) )
	{
		BOOST_LOG_TRIVIAL(warning) << "[GHOST] local database error - " + m_DBLocal->GetError( );
		return true;
	}

	boost::mutex::scoped_lock gamesLock( m_GamesMutex );
	
	// get rid of any deleted games
	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); )
	{
		if( (*i)->readyDelete( ) )
		{
			delete *i;
			m_Games.erase( i );
		} else {
			++i;
		}
	}

	if( m_CurrentGame && m_CurrentGame->readyDelete( ) )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			(*i)->QueueGameUncreate( );
			(*i)->QueueEnterChat( );
		}
		
		delete m_CurrentGame;
		m_CurrentGame = NULL;
	}
	
	gamesLock.unlock( );

	// try to exit nicely if requested to do so

	if( m_ExitingNice )
	{
		if( !m_BNETs.empty( ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] deleting all battle.net connections in preparation for exiting nicely";

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
				delete *i;

			m_BNETs.clear( );
		}

		if( m_CurrentGame )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] deleting current game in preparation for exiting nicely";
			m_CurrentGame->doDelete( );
			m_CurrentGame = NULL;
		}

		if( m_Games.empty( ) )
		{
			if( !m_AllGamesFinished )
			{
				BOOST_LOG_TRIVIAL(info) << "[GHOST] all games finished, waiting 60 seconds for threads to finish";
				BOOST_LOG_TRIVIAL(info) << "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads in progress";
				m_AllGamesFinished = true;
				m_AllGamesFinishedTime = GetTime( );
			}
			else
			{
				if( m_Callables.empty( ) )
				{
					BOOST_LOG_TRIVIAL(info) << "[GHOST] all threads finished, exiting nicely";
					m_Exiting = true;
				}
				else if( GetTime( ) - m_AllGamesFinishedTime >= 60 )
				{
					BOOST_LOG_TRIVIAL(info) << "[GHOST] waited 60 seconds for threads to finish, exiting anyway";
					BOOST_LOG_TRIVIAL(info) << "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads still in progress which will be terminated";
					m_Exiting = true;
				}
			}
		}
	}

	// update callables
	boost::mutex::scoped_lock callablesLock( m_CallablesMutex );

	for( vector<CBaseCallable *> :: iterator i = m_Callables.begin( ); i != m_Callables.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_DB->RecoverCallable( *i );
			delete *i;
			i = m_Callables.erase( i );
		}
		else
			++i;
	}
	
	callablesLock.unlock( );

	// create the GProxy++ reconnect listener

	if( m_Reconnect )
	{
		if( !m_ReconnectSocket )
		{
			m_ReconnectSocket = new CTCPServer( );

			if( m_ReconnectSocket->Listen( m_BindAddress, m_ReconnectPort ) )
				BOOST_LOG_TRIVIAL(info) << "[GHOST] listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort );
			else
			{
				BOOST_LOG_TRIVIAL(info) << "[GHOST] error listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort );
				delete m_ReconnectSocket;
				m_ReconnectSocket = NULL;
				m_Reconnect = false;
			}
		}
		else if( m_ReconnectSocket->HasError( ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] GProxy++ reconnect listener error (" + m_ReconnectSocket->GetErrorString( ) + ")";
			delete m_ReconnectSocket;
			m_ReconnectSocket = NULL;
			m_Reconnect = false;
		}
	}

	unsigned int NumFDs = 0;

	// take every socket we own and throw it in one giant select statement so we can block on all sockets

	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	// 1. all battle.net sockets

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 5. the GProxy++ reconnect socket(s)

	if( m_Reconnect && m_ReconnectSocket )
	{
		m_ReconnectSocket->SetFD( &fd, &send_fd, &nfds );
		++NumFDs;
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); ++i )
	{
		(*i)->SetFD( &fd, &send_fd, &nfds );
		++NumFDs;
	}

	// before we call select we need to determine how long to block for
	// previously we just blocked for a maximum of the passed usecBlock microseconds
	// however, in an effort to make game updates happen closer to the desired latency setting we now use a dynamic block interval
	// note: we still use the passed usecBlock as a hard maximum

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
	{
		if( (*i)->GetNextTimedActionTicks( ) * 1000 < usecBlock )
			usecBlock = (*i)->GetNextTimedActionTicks( ) * 1000;
	}

	// always block for at least 1ms just in case something goes wrong
	// this prevents the bot from sucking up all the available CPU if a game keeps asking for immediate updates
	// it's a bit ridiculous to include this check since, in theory, the bot is programmed well enough to never make this mistake
	// however, considering who programmed it, it's worthwhile to do it anyway

	if( usecBlock < 1000 )
		usecBlock = 1000;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, NULL, NULL, &tv );
	select( 1, NULL, &send_fd, NULL, &send_tv );
#else
	select( nfds + 1, &fd, NULL, NULL, &tv );
	select( nfds + 1, NULL, &send_fd, NULL, &send_tv );
#endif

	if( NumFDs == 0 )
	{
		// we don't have any sockets (i.e. we aren't connected to battle.net maybe due to a lost connection and there aren't any games running)
		// select will return immediately and we'll chew up the CPU if we let it loop so just sleep for 50ms to kill some time

		MILLISLEEP( 50 );
	}

	bool AdminExit = false;
	bool BNETExit = false;

	// update battle.net connections

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		if( (*i)->Update( &fd, &send_fd ) )
			BNETExit = true;
	}

	// update GProxy++ reliable reconnect sockets

	if( m_Reconnect && m_ReconnectSocket )
	{
		CTCPSocket *NewSocket = m_ReconnectSocket->Accept( &fd );

		if( NewSocket )
			m_ReconnectSockets.push_back( NewSocket );
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); )
	{
		if( (*i)->HasError( ) || !(*i)->GetConnected( ) || GetTime( ) - (*i)->GetLastRecv( ) >= 10 )
		{
			delete *i;
			i = m_ReconnectSockets.erase( i );
			continue;
		}

		(*i)->DoRecv( &fd );
		string *RecvBuffer = (*i)->GetBytes( );
		BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

		// a packet is at least 4 bytes

		if( Bytes.size( ) >= 4 )
		{
			if( Bytes[0] == GPS_HEADER_CONSTANT )
			{
				// bytes 2 and 3 contain the length of the packet

				uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

				if( Length >= 4 )
				{
					if( Bytes.size( ) >= Length )
					{
						if( Bytes[1] == CGPSProtocol :: GPS_RECONNECT && Length == 13 )
						{
							GProxyReconnector *Reconnector = new GProxyReconnector;
							Reconnector->PID = Bytes[4];
							Reconnector->ReconnectKey = UTIL_ByteArrayToUInt32( Bytes, false, 5 );
							Reconnector->LastPacket = UTIL_ByteArrayToUInt32( Bytes, false, 9 );
							Reconnector->PostedTime = GetTicks( );
							Reconnector->socket = (*i);
							
							// update the receive buffer
							*RecvBuffer = RecvBuffer->substr( Length );
							i = m_ReconnectSockets.erase( i );

							// post in the reconnects buffer and wait to see if a game thread will pick it up
							boost::mutex::scoped_lock lock( m_ReconnectMutex );
							m_PendingReconnects.push_back( Reconnector );
							lock.unlock();
							continue;
						}
						else
						{
							(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
							(*i)->DoSend( &send_fd );
							delete *i;
							i = m_ReconnectSockets.erase( i );
							continue;
						}
					}
				}
				else
				{
					(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
					(*i)->DoSend( &send_fd );
					delete *i;
					i = m_ReconnectSockets.erase( i );
					continue;
				}
			}
			else
			{
				(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
				(*i)->DoSend( &send_fd );
				delete *i;
				i = m_ReconnectSockets.erase( i );
				continue;
			}
		}

		(*i)->DoSend( &send_fd );
		++i;
	}
	
	// delete any old pending reconnects that have not been handled by games
	if( !m_PendingReconnects.empty( ) ) {
		boost::mutex::scoped_lock lock( m_ReconnectMutex );
	
		for( vector<GProxyReconnector *> :: iterator i = m_PendingReconnects.begin( ); i != m_PendingReconnects.end( ); )
		{
			if( GetTicks( ) - (*i)->PostedTime > 1500 )
			{
				(*i)->socket->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_NOTFOUND ) );
				(*i)->socket->DoSend( &send_fd );
				delete (*i)->socket;
				delete (*i);
				i = m_PendingReconnects.erase( i );
				continue;
			}
			
			i++;
		}
	
		lock.unlock();
	}

	// autohost

	if( !m_AutoHostGameName.empty( ) && m_AutoHostMaximumGames != 0 && m_AutoHostAutoStartPlayers != 0 && GetTime( ) - m_LastAutoHostTime >= 30 )
	{
		// copy all the checks from CGHost :: CreateGame here because we don't want to spam the chat when there's an error
		// instead we fail silently and try again soon

		if( !m_ExitingNice && m_Enabled && !m_CurrentGame && m_Games.size( ) < m_MaxGames && m_Games.size( ) < m_AutoHostMaximumGames )
		{
			CMap *mapToHost = m_AutoHostMap;

			// check if we should load a random map for the next game 
			if( m_AutoHostRandomizeMapType != "none" )
			{
				BOOST_LOG_TRIVIAL(info) << "[GHOST] Loading random map for next autohosted game"; 
				 
				// load random map
				boost::filesystem::path newMap;
				if( m_AutoHostRandomizeMapType == "random" )
				{
					std::vector<boost::filesystem::path> fileList = GetMapsInDirectory( m_MapPath, "w3" );

					int randomIndex = rand() % fileList.size();
					LoadMap( fileList[randomIndex].filename( ).string( ), NULL, "", false );
				}
				else if( m_AutoHostRandomizeMapType == "list" )
				{
					vector<string> mapList;
					stringstream ss( m_AutoHostRamdomizeMapList );
					while( ss.good() )
					{
						string substr;
						getline( ss, substr, ',' );
						mapList.push_back( substr );
					}

					int randomIndex = rand() % mapList.size();
					LoadMap( mapList[randomIndex], NULL, "", false );
				}

				// overwrite autohost map
				mapToHost = m_Map;
			}
 
			// autohost game
			if( mapToHost->GetValid( ) )
			{
				string GameName = m_AutoHostGameName + " #" + UTIL_ToString( m_HostCounter );

				if( GameName.size( ) <= 31 )
				{
					CreateGame( mapToHost, GAME_PUBLIC, false, GameName, m_AutoHostOwner, m_AutoHostOwner, m_AutoHostServer, false );

					if( m_CurrentGame )
					{
						m_CurrentGame->SetAutoStartPlayers( m_AutoHostAutoStartPlayers );

						if( m_AutoHostMatchMaking )
						{
							if( !m_Map->GetMapMatchMakingCategory( ).empty( ) )
							{
								if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) )
									BOOST_LOG_TRIVIAL(info) << "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found but matchmaking can only be used with fixed player settings, matchmaking disabled";
								else
								{
									BOOST_LOG_TRIVIAL(info) << "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found, matchmaking enabled";

									m_CurrentGame->SetMatchMaking( true );
									m_CurrentGame->SetMinimumScore( m_AutoHostMinimumScore );
									m_CurrentGame->SetMaximumScore( m_AutoHostMaximumScore );
								}
							}
							else
								BOOST_LOG_TRIVIAL(info) << "[GHOST] autohostmm - map_matchmakingcategory not found, matchmaking disabled";
						}
					}
				}
				else
				{
					BOOST_LOG_TRIVIAL(info) << "[GHOST] stopped auto hosting, next game name [" + GameName + "] is too long (the maximum is 31 characters)";
					m_AutoHostGameName.clear( );
					m_AutoHostOwner.clear( );
					m_AutoHostServer.clear( );
					m_AutoHostMaximumGames = 0;
					m_AutoHostAutoStartPlayers = 0;
					m_AutoHostMatchMaking = false;
					m_AutoHostMinimumScore = 0.0;
					m_AutoHostMaximumScore = 0.0;
				}
			}
			else
			{
				BOOST_LOG_TRIVIAL(info) << "[GHOST] stopped auto hosting, map config file [" + m_AutoHostMap->GetCFGFile( ) + "] is invalid";
				m_AutoHostGameName.clear( );
				m_AutoHostOwner.clear( );
				m_AutoHostServer.clear( );
				m_AutoHostMaximumGames = 0;
				m_AutoHostAutoStartPlayers = 0;
				m_AutoHostMatchMaking = false;
				m_AutoHostMinimumScore = 0.0;
				m_AutoHostMaximumScore = 0.0;
			}
		}

		m_LastAutoHostTime = GetTime( );
	}

	return m_Exiting || AdminExit || BNETExit;
}

void CGHost :: EventBNETConnecting( CBNET *bnet )
{

}

void CGHost :: EventBNETConnected( CBNET *bnet )
{

}

void CGHost :: EventBNETDisconnected( CBNET *bnet )
{

}

void CGHost :: EventBNETLoggedIn( CBNET *bnet )
{

}

void CGHost :: EventBNETGameRefreshed( CBNET *bnet )
{
	boost::mutex::scoped_lock lock( m_GamesMutex );
	
	if( m_CurrentGame )
		m_CurrentGame->EventGameRefreshed( bnet->GetServer( ) );
	
	lock.unlock( );
}

void CGHost :: EventBNETGameRefreshFailed( CBNET *bnet )
{
	boost::mutex::scoped_lock lock( m_GamesMutex );
	
	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

			if( (*i)->GetServer( ) == m_CurrentGame->GetCreatorServer( ) )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ), m_CurrentGame->GetCreatorName( ), true );
		}

		boost::mutex::scoped_lock sayLock( m_CurrentGame->m_SayGamesMutex );
		m_CurrentGame->m_DoSayGames.push_back( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );
		sayLock.unlock( );

		// we take the easy route and simply close the lobby if a refresh fails
		// it's possible at least one refresh succeeded and therefore the game is still joinable on at least one battle.net (plus on the local network) but we don't keep track of that
		// we only close the game if it has no players since we support game rehosting (via !priv and !pub in the lobby)

		if( m_CurrentGame->GetNumHumanPlayers( ) == 0 )
			m_CurrentGame->SetExiting( true );

		m_CurrentGame->SetRefreshError( true );
	}
	
	lock.unlock( );
}

void CGHost :: EventBNETConnectTimedOut( CBNET *bnet )
{

}

void CGHost :: EventBNETWhisper( CBNET *bnet, string user, string message )
{

}

void CGHost :: EventBNETChat( CBNET *bnet, string user, string message )
{

}

void CGHost :: EventBNETEmote( CBNET *bnet, string user, string message )
{

}

void CGHost :: EventGameDeleted( CBaseGame *game )
{
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ) );

		if( (*i)->GetServer( ) == game->GetCreatorServer( ) )
			(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ), game->GetCreatorName( ), true );
	}
}

void CGHost :: ReloadConfigs( )
{
	CConfig CFG;
	CFG.Read( "default.cfg" );
	CFG.Read( gCFGFile );
	SetConfigs( &CFG );
}

void CGHost :: SetConfigs( CConfig *CFG )
{
	// this doesn't set EVERY config value since that would potentially require reconfiguring the battle.net connections
	// it just set the easily reloadable values

	m_LanguageFile = CFG->GetString( "bot_language", "language.cfg" );
	delete m_Language;
	m_Language = new CLanguage( m_LanguageFile );
	m_Warcraft3Path = UTIL_AddPathSeperator( CFG->GetString( "bot_war3path", "C:\\Program Files\\Warcraft III\\" ) );
	m_BindAddress = CFG->GetString( "bot_bindaddress", string( ) );
	m_ReconnectWaitTime = CFG->GetInt( "bot_reconnectwaittime", 3 );
	m_MaxGames = CFG->GetInt( "bot_maxgames", 5 );
	string BotCommandTrigger = CFG->GetString( "bot_commandtrigger", "!" );

	if( BotCommandTrigger.empty( ) )
		BotCommandTrigger = "!";

	m_CommandTrigger = BotCommandTrigger[0];
	m_MapCFGPath = UTIL_AddPathSeperator( CFG->GetString( "bot_mapcfgpath", string( ) ) );
	m_SaveGamePath = UTIL_AddPathSeperator( CFG->GetString( "bot_savegamepath", string( ) ) );
	m_MapPath = UTIL_AddPathSeperator( CFG->GetString( "bot_mappath", string( ) ) );
	m_SaveReplays = CFG->GetInt( "bot_savereplays", 0 ) == 0 ? false : true;
	m_ReplayPath = UTIL_AddPathSeperator( CFG->GetString( "bot_replaypath", string( ) ) );
	m_VirtualHostName = CFG->GetString( "bot_virtualhostname", "|cFF4080C0GHost" );
	m_HideIPAddresses = CFG->GetInt( "bot_hideipaddresses", 0 ) == 0 ? false : true;
	m_CheckMultipleIPUsage = CFG->GetInt( "bot_checkmultipleipusage", 1 ) == 0 ? false : true;

	if( m_VirtualHostName.size( ) > 15 )
	{
		m_VirtualHostName = "|cFF4080C0GHost";
		BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - bot_virtualhostname is longer than 15 characters, using default virtual host name";
	}

	m_SpoofChecks = CFG->GetInt( "bot_spoofchecks", 2 );
	m_RequireSpoofChecks = CFG->GetInt( "bot_requirespoofchecks", 0 ) == 0 ? false : true;
	m_ReserveAdmins = CFG->GetInt( "bot_reserveadmins", 1 ) == 0 ? false : true;
	m_RefreshMessages = CFG->GetInt( "bot_refreshmessages", 0 ) == 0 ? false : true;
	m_AutoLock = CFG->GetInt( "bot_autolock", 0 ) == 0 ? false : true;
	m_AutoSave = CFG->GetInt( "bot_autosave", 0 ) == 0 ? false : true;
	m_AllowDownloads = CFG->GetInt( "bot_allowdownloads", 0 );
	m_PingDuringDownloads = CFG->GetInt( "bot_pingduringdownloads", 0 ) == 0 ? false : true;
	m_MaxDownloaders = CFG->GetInt( "bot_maxdownloaders", 3 );
	m_MaxDownloadSpeed = CFG->GetInt( "bot_maxdownloadspeed", 100 );
	m_LCPings = CFG->GetInt( "bot_lcpings", 1 ) == 0 ? false : true;
	m_AutoKickPing = CFG->GetInt( "bot_autokickping", 400 );
	m_VoteStartAllowed = CFG->GetInt( "bot_votestartallowed", 1 ) == 0 ? false : true;
	m_VoteStartMinPlayers = CFG->GetInt("bot_votestartplayers", 2);
	m_VoteStartPercentage = CFG->GetInt("bot_votestartpercentage", 60);
	m_BanMethod = CFG->GetInt( "bot_banmethod", 1 );
	m_IPBlackListFile = CFG->GetString( "bot_ipblacklistfile", "ipblacklist.txt" );
	m_LobbyTimeLimit = CFG->GetInt( "bot_lobbytimelimit", 10 );
	m_Latency = CFG->GetInt( "bot_latency", 100 );
	m_SyncLimit = CFG->GetInt( "bot_synclimit", 50 );
	m_VoteKickAllowed = CFG->GetInt( "bot_votekickallowed", 1 ) == 0 ? false : true;
	m_VoteKickPercentage = CFG->GetInt( "bot_votekickpercentage", 100 );

	if( m_VoteKickPercentage > 100 ) {
		m_VoteKickPercentage = 100;
		BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - bot_votekickpercentage is greater than 100, using 100 instead";
	}

	if( m_VoteStartPercentage > 100 ) {
		m_VoteStartPercentage = 100;
		BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - bot_votekickpercentage is greater than 100, using 100 instead";
	}

	m_MOTDFile = CFG->GetString( "bot_motdfile", "motd.txt" );
	m_GameLoadedFile = CFG->GetString( "bot_gameloadedfile", "gameloaded.txt" );
	m_GameOverFile = CFG->GetString( "bot_gameoverfile", "gameover.txt" );
	m_TCPNoDelay = CFG->GetInt( "tcp_nodelay", 0 ) == 0 ? false : true;
	m_MatchMakingMethod = CFG->GetInt( "bot_matchmakingmethod", 1 );
	m_MapGameType = CFG->GetUInt32( "bot_mapgametype", 0 );
}

void CGHost :: LoadIPToCountryData( )
{
	ifstream in;
	in.open( "ip-to-country.csv" );

	if( in.fail( ) )
		BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - unable to read file [ip-to-country.csv], iptocountry data not loaded";
	else
	{
		BOOST_LOG_TRIVIAL(info) << "[GHOST] started loading [ip-to-country.csv]";

		// the begin and commit statements are optimizations
		// we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time
		// todotodo: handle begin/commit failures a bit more gracefully

		if( !m_DBLocal->Begin( ) )
			BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - failed to begin local database transaction, iptocountry data not loaded";
		else
		{
			unsigned char Percent = 0;
			string Line;
			string IP1;
			string IP2;
			string Country;
			CSVParser parser;

			// get length of file for the progress meter

			in.seekg( 0, ios :: end );
			uint32_t FileLength = in.tellg( );
			in.seekg( 0, ios :: beg );

			while( !in.eof( ) )
			{
				getline( in, Line );

				if( Line.empty( ) )
					continue;

				parser << Line;
				parser >> IP1;
				parser >> IP2;
				parser >> Country;
				m_DBLocal->FromAdd( UTIL_ToUInt32( IP1 ), UTIL_ToUInt32( IP2 ), Country );

				// it's probably going to take awhile to load the iptocountry data (~10 seconds on my 3.2 GHz P4 when using SQLite3)
				// so let's print a progress meter just to keep the user from getting worried

				unsigned char NewPercent = (unsigned char)( (float)in.tellg( ) / FileLength * 100 );

				if( NewPercent != Percent && NewPercent % 10 == 0 )
				{
					Percent = NewPercent;
					BOOST_LOG_TRIVIAL(info) << "[GHOST] iptocountry data: " + UTIL_ToString( Percent ) + "% loaded";
				}
			}

			if( !m_DBLocal->Commit( ) )
				BOOST_LOG_TRIVIAL(info) << "[GHOST] warning - failed to commit local database transaction, iptocountry data not loaded";
			else
				BOOST_LOG_TRIVIAL(info) << "[GHOST] finished loading [ip-to-country.csv]";
		}

		in.close( );
	}
}

void CGHost :: CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, string creatorName, string creatorServer, bool whisper )
{
	if( !m_Enabled )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameDisabled( gameName ), creatorName, whisper );
		}

		return;
	}

	if( gameName.size( ) > 31 )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameNameTooLong( gameName ), creatorName, whisper );
		}

		return;
	}

	if( !map->GetValid( ) )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidMap( gameName ), creatorName, whisper );
		}

		return;
	}

	if( saveGame )
	{
		if( !m_SaveGame->GetValid( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidSaveGame( gameName ), creatorName, whisper );
			}

			return;
		}

		string MapPath1 = m_SaveGame->GetMapPath( );
		string MapPath2 = map->GetMapPath( );
		transform( MapPath1.begin( ), MapPath1.end( ), MapPath1.begin( ), (int(*)(int))tolower );
		transform( MapPath2.begin( ), MapPath2.end( ), MapPath2.begin( ), (int(*)(int))tolower );

		if( MapPath1 != MapPath2 )
		{
			BOOST_LOG_TRIVIAL(info) << "[GHOST] path mismatch, saved game path is [" + MapPath1 + "] but map path is [" + MapPath2 + "]";

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameSaveGameMapMismatch( gameName ), creatorName, whisper );
			}

			return;
		}

		if( m_EnforcePlayers.empty( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameMustEnforceFirst( gameName ), creatorName, whisper );
			}

			return;
		}
	}
	
	boost::mutex::scoped_lock lock( m_GamesMutex );

	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ), creatorName, whisper );
		}

		return;
	}

	if( m_Games.size( ) >= m_MaxGames )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ), creatorName, whisper );
		}

		return;
	}

	lock.unlock( );

	BOOST_LOG_TRIVIAL(info) << "[GHOST] creating game [" + gameName + "]";

	if( saveGame )
		m_CurrentGame = new CGame( this, map, m_SaveGame, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
	else
		m_CurrentGame = new CGame( this, map, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );

	// todotodo: check if listening failed and report the error to the user

	if( m_SaveGame )
	{
		m_CurrentGame->SetEnforcePlayers( m_EnforcePlayers );
		m_EnforcePlayers.clear( );
	}

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		if( whisper && (*i)->GetServer( ) == creatorServer )
		{
			// note that we send this whisper only on the creator server

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ), creatorName, whisper );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ), creatorName, whisper );
		}
		else
		{
			// note that we send this chat message on all other bnet servers

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ) );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ) );
		}

		if( saveGame )
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, m_SaveGame, m_CurrentGame->GetHostCounter( ) );
		else
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, NULL, m_CurrentGame->GetHostCounter( ) );
	}

	// if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
	// unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
	// so don't rejoin the chat if we're using PVPGN

	if( gameState == GAME_PRIVATE )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			if( (*i)->GetPasswordHashType( ) != "pvpgn" )
				(*i)->QueueEnterChat( );
		}
	}

	// hold friends and/or clan members

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		if( (*i)->GetHoldFriends( ) )
			(*i)->HoldFriends( m_CurrentGame );

		if( (*i)->GetHoldClan( ) )
			(*i)->HoldClan( m_CurrentGame );
	}
	
	// start the game thread
	boost::thread(&CBaseGame::loop, m_CurrentGame);
	BOOST_LOG_TRIVIAL(info) << "[GameThread] Made new game thread";
}

//
// Load a map
//
void CGHost :: LoadMap( string MapName, CBNET *bnet, string User, bool Whisper )
{
	string realmName = bnet == NULL ? "SYSTEM" : "BNET: " + bnet->GetServerAlias( );
	BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] trying to load map  [" + MapName + "] ...";

	try
	{
		string Pattern = MapName;
		transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

		if( !boost::filesystem::exists( m_MapPath ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] error listing maps - map path doesn't exist";

			if ( !User.empty() )
				bnet->QueueChatCommand( m_Language->ErrorListingMaps( ), User, Whisper );
		}
		else
		{
			string Pattern = MapName;
			transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );
			std::vector<boost::filesystem::path> fileList = GetMapsInDirectory( m_MapPath, Pattern );

			if( fileList.size() == 0 )
			{
				BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] " + m_Language->NoMapsFound( );
				if ( !User.empty() )
					bnet->QueueChatCommand( m_Language->NoMapsFound( ), User, Whisper );
			}
			else if( fileList.size() == 1 )
			{
				string File = fileList.front( ).filename( ).string( );
				
				BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] " + m_Language->LoadingConfigFile( m_MapPath + File );
				if ( !User.empty() )
					bnet->QueueChatCommand( m_Language->LoadingConfigFile( m_MapPath + File ), User, Whisper );

				// create a config file in memory with the required information to load the map

				CConfig MapCFG;
				MapCFG.Set( "map_path", "Maps\\Download\\" + File );
				MapCFG.Set( "map_localpath", File );
				m_Map = new CMap( this, &MapCFG, fileList.front( ).string( ) );
			}
			else
			{
				/*
				BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] " + m_Language->FoundMaps( boost::algorithm::join(fileList, ",") );
				if ( !User.empty() )
					bnet->QueueChatCommand( m_Language->FoundMaps( boost::algorithm::join(fileList, ",") ), User, Whisper );
				*/
			}
		}
	}
	catch( const exception &ex )
	{
		BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] error listing maps - caught exception [" + ex.what( ) + "]";

		if ( !User.empty() )
			bnet->QueueChatCommand( m_Language->ErrorListingMaps( ), User, Whisper );
	}
}

//
// Load a map configuration file
//
void CGHost :: LoadMapConfig( string MapConfigName, CBNET *bnet, string User, bool Whisper )
{
	string realmName = bnet == NULL ? "SYSTEM" : "BNET: " + bnet->GetServerAlias( );
	BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] trying to load map config  [" + MapConfigName + "] ...";

	try
	{
		if( !boost::filesystem::exists( m_MapCFGPath ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] error listing map configs - map config path doesn't exist";

			if ( !User.empty() )
				bnet->QueueChatCommand( m_Language->ErrorListingMapConfigs( ), User, Whisper );
		}
		else
		{
			string Pattern = MapConfigName;
			transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );
			std::vector<boost::filesystem::path> fileList = GetMapConfigsInDirectory( m_MapCFGPath, Pattern );

			if( fileList.size() == 0 )
			{
				BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] " + m_Language->NoMapConfigsFound( );
				if ( !User.empty() )
					bnet->QueueChatCommand( m_Language->NoMapConfigsFound( ), User, Whisper );
			}
			else if( fileList.size() == 1 )
			{
				string File = fileList.front( ).filename( ).string( );
				
				BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] " + m_Language->LoadingConfigFile( m_MapCFGPath + File );
				if ( !User.empty() )
					bnet->QueueChatCommand( m_Language->LoadingConfigFile( m_MapCFGPath + File ), User, Whisper );

				CConfig MapCFG;
				MapCFG.Read( fileList.front( ).string( ) );
				m_Map = new CMap( this, &MapCFG, m_MapCFGPath + File );
			}
			else
			{
				/*
				BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] " + m_Language->FoundMapConfigs( boost::algorithm::join(fileList, ",") );
				if ( !User.empty() )
					bnet->QueueChatCommand( m_Language->FoundMapConfigs( boost::algorithm::join(fileList, ",") ), User, Whisper );
				*/
			}
		}
	}
	catch( const exception &ex )
	{
		BOOST_LOG_TRIVIAL(info) << "[" + realmName + "] error listing map configs - caught exception [" + ex.what( ) + "]";

		if ( !User.empty() )
			bnet->QueueChatCommand( m_Language->ErrorListingMapConfigs( ), User, Whisper );
	}
}

//
// Get Files in Directory
//
std::vector<boost::filesystem::path> CGHost :: GetFilesInDirectory( boost::filesystem::path ParentDirectory, string Pattern )
{
	std::vector<boost::filesystem::path> fileList;

	try
	{
		transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

		if( !exists( ParentDirectory ) )
		{
			BOOST_LOG_TRIVIAL(info) << "[SYSTEM] error listing map configs - map config path doesn't exist";
		}
		else
		{
			boost::filesystem::directory_iterator EndIterator;
			for( boost::filesystem::directory_iterator i( ParentDirectory ); i != EndIterator; ++i )
			{
				string FileName = i->path( ).filename( ).string( );
				transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );

				if( !boost::filesystem::is_directory( i->status( ) ) && FileName.find( Pattern ) != string :: npos )
				{
					fileList.push_back( i->path( ) );
				}
			}
		}
	}
	catch( const exception &ex )
	{
		BOOST_LOG_TRIVIAL(info) << "[SYSTEM] error listing directories in folder [" + ParentDirectory.string( ) + "]";
	}

	return fileList;
}

//
// Get all maps within a directory that match the filename 
//
std::vector<boost::filesystem::path> CGHost :: GetMapsInDirectory( boost::filesystem::path ParentDirectory, string Filename )
{
	std::vector<boost::filesystem::path> resultList;
	std::vector<boost::filesystem::path> fileList = GetFilesInDirectory( ParentDirectory, Filename );

	for (std::vector<boost::filesystem::path>::const_iterator i = fileList.begin(), end = fileList.end(); i != end; ++i)
	{
		if ( (*i).extension() == ".w3m" || (*i).extension() == ".w3x" )
		{
			resultList.push_back((*i));
		}
	}

	return resultList;
}

//
// Get all map configs within a directory that match the filename 
//
std::vector<boost::filesystem::path> CGHost :: GetMapConfigsInDirectory( boost::filesystem::path ParentDirectory, string Filename )
{
	std::vector<boost::filesystem::path> resultList;
	std::vector<boost::filesystem::path> fileList = GetFilesInDirectory( ParentDirectory, Filename );

	for (std::vector<boost::filesystem::path>::const_iterator i = fileList.begin(), end = fileList.end(); i != end; ++i)
	{
		if ( (*i).extension() == ".cfg" )
		{
			resultList.push_back((*i));
		}
	}

	return resultList;
}
