#pragma once

#if 0
#include <string>
#include <map>
#include <deque>

class P4Connection;
class StrPtr;
class P4BridgeServer;

using std::string;
using std::map;
using std::deque;

#ifndef time
#include <time.h>
#endif

//forward references
class ConnectionManager;

#ifdef _DEBUG_MEMORY
class ConnectionManager : p4base
#else
class ConnectionManager
#endif
{
private:
	string client;
	string user;
	string port;
	string password;
	string programName;
	string programVer;
	string cwd;

	CharSetApi::CharSet charset;
	CharSetApi::CharSet file_charset;

	P4BridgeServer *pServer;

	ConnectionManager(void);

	deque<P4Connection*> IdleConnections;
	map<int, P4Connection*> ActiveConnections;

	void DiscardConnections();
	void InitializeConnection(P4Connection *con);

	P4Connection* NewConnection(int cmdId);

public:
	int Initialized;

	ConnectionManager(P4BridgeServer *pserver);

	virtual ~ConnectionManager(void);

	P4Connection* GetConnection(int cmdId);
	void ReleaseConnection(int cmdId, unsigned _int64 releaseTime);
	void ReleaseConnection(P4Connection* pCon, unsigned _int64 releaseTime);
	int FreeConnections( unsigned _int64 currentTime );

	// Set the connection data used
	void SetClient( const char* newVal );
	void SetUser( const char* newVal );
	void SetPort( const char* newVal );
	void SetPassword( const char* newVal );
	void SetProgramName( const char* newVal );
	void SetProgramVer( const char* newVal );
	void SetCwd( const char* newCwd );
	void SetCharset( CharSetApi::CharSet c, CharSetApi::CharSet filec );

	void SetConnection(const char* newPort, const char* newUser, const char* newPassword, const char* newClient)
		{SetClient(newClient); SetUser(newUser);SetPort(newPort);SetPassword(newPassword);}

	const StrPtr* GetCharset( );

	int Disconnect( void );

#ifdef _DEBUG_MEMORY
	    // Simple type identification for registering objects
	virtual int Type(void) {return tConnectionManager;}
#endif
};

#endif