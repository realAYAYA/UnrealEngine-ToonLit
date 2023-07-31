#pragma once
/*******************************************************************************

Copyright (c) 2010, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: P4BridgeServer.h
 *
 * Author	: dbb
 *
 * Description	:  P4BridgeServer
 *
 ******************************************************************************/
#include "P4BridgeClient.h"
#include "P4Connection.h"
#include "P4BridgeEnviro.h"
#include "Lock.h"

#include <string>
#include <map>

using std::string;

class P4BridgeServer;

/*
	Parallel transfer object - the P4API manages the lifetime of this object, so it needs to live on its own
*/

class ParallelTransfer : public ClientTransfer, public p4base
{
public:
	ParallelTransfer(P4BridgeServer* pServer = NULL);
	virtual ~ParallelTransfer();

	void SetBridgeServer(P4BridgeServer* pServer);

	virtual int Transfer(ClientApi *, ClientUser *, const char *, StrArray &, StrDict &, int, Error *);

	virtual int Type(void) { return tParallelTransfer; }

protected:
	P4BridgeServer *pBridgeServer;
};

/*******************************************************************************
 *
 *  This is the function prototypes for the call backs used to log status, 
 *      error conditions, debugging messages, and the like to the client. It is
 *      up to the client to log the strings into a file if desired.
 *
 ******************************************************************************/

typedef int _stdcall LogCallbackFn(int, const char*, int, const char*);


// Number returned by GetProtocol() if the server supports login, logout, etc.
#define SERVER_SECURITY_PROTOCOL	18

// Minimum server level supporting extended submit options
#define SERVER_EXTENDED_SUBMIT		22

/*******************************************************************************
 *
 *  P4BridgeClient
 *
 *  Class used to wrap the ClientApi in the p4api. It provides the capability to
 *      connect to a P4 Server and execute commands. It initializes two 
 *      connections, one each for tagged/untagged output.
 *
 ******************************************************************************/

class P4BridgeServer : public p4base
{
	friend class TestP4BridgeServer;
	friend class ParallelTransfer;

public:
 
	// logging support
	static int LogMessage(int log_level, const char * file, int line, const char * message, ...);
	static int LogMessageNoArgs(int log_level, const char * file, int line, const char * message);

	ILockable Locker;

protected:
	// Cannot create without initialization
	P4BridgeServer(void);

	// Get the protocol information from the server
	int GetServerProtocols(P4ClientError **err);

	// UI support
	// Internal exception handler to handle platform exceptions i.e. Null 
	//      pointer access
	int HandleException(const char* fname, unsigned int line, const char* func, unsigned int c, struct _EXCEPTION_POINTERS *e, string** pErrorString);
	static int sHandleException(unsigned int c, struct _EXCEPTION_POINTERS *e);

	// Call back function used to send text results back to the client
	//
	// The function prototype is:
	//
	// void TextCallbackFn(const char*);
	//
	// The first parameter is the text data. Multiple callbacks might be made 
	//      a single command and the text should be concatenated to obtain
	//      the entire results.

	TextCallbackFn* pTextResultsCallbackFn;

	// Call back function used to send informational messages back 
	//      to the client. This is generally the output from a command
	//      when not using tagged protocol.
	//
	// The function prototype is:
	//
	// void void IntTextCallbackFn(int, const char*);
	//
	// The first parameter is the message level. Generally, a message of a 
	//      higher level is a sub field of the preceding lower level message,
	//      i.e a level zero message may be followed by one or more level one
	//      messages containing details about the output. This is an older way
	//      of grouping output into logical objects that is superseded by
	//      tagged output.
	// The second parameter is the information

	IntIntIntTextCallbackFn* pInfoResultsCallbackFn;
		
	// Callback function used to send tagged output to the client. Tagged or
	// data is sent from the api as one or more StrDict objects representing
	// one or more data objects. Each dictionary represents one data objects.
	// I.e. for a list of file stats, each dictionary object will represent the
	// data for one file. The data is stored as Key:Value pairs, i.e 
	// Filename:"MyCode.cpp"
	//
	// The function prototype is:
	//
	//  void IntTextTextCallbackFn(int, const char*, const char*);
	//      
	// The first parameter is an object ID, a given command can return 
	//      multiple objects. ID's will start at 0 and increment for each
	//      successive object.
	// The second parameter is the text 'key'
	// The third parameter is the text value
	// 
	// The client will receive the call back multiple times, once for each
	// Key:Value pair in all of the dictionaries. For m data objects each
	// with n Key:Value pairs, the client will receive m * n call backs. The
	// object id can be used to group the data with there correct objects.
	//

	IntTextTextCallbackFn* pTaggedOutputCallbackFn;
	
	// Call back function used to send error messages back to the client
	//
	// The function prototype is:
	//
	// void IntTextCallbackFn( int, const char* );
	//
	// The first parameter is the error level, i.e Error or Warning
	// The second parameter is the error message

	IntIntIntTextCallbackFn* pErrorCallbackFn;
	
	// Call back function used to send text results back to the client
	//
	// The function prototype is:
	//
	// void BinaryCallbackFn(const void *, int);
	//
	// The first parameter is the binary data. Multiple callbacks might be made
	//      a single command and the results should be concatenated to obtain
	//      the entire results.
	// The second parameter is the size of the data in bytes.

	BinaryCallbackFn* pBinaryResultsCallbackFn;
	
	PromptCallbackFn * pPromptCallbackFn;
	ParallelTransferCallbackFn* pParallelTransferCallbackFn;

	ResolveCallbackFn * pResolveCallbackFn;
	ResolveACallbackFn * pResolveACallbackFn;

	int Resolve_int( int cmdId, P4ClientMerge *merger);
	int Resolve_int( int cmdId, P4ClientResolve *resolver, int preview, Error *e);
		
	ParallelTransfer *pTransfer;

	// pTransfer calls this
	int DoTransfer(
		ClientApi* client,
		ClientUser *ui,
		const char *cmd,
		StrArray &args,
		StrDict &pVars,
		int threads,
		Error *e);

	int DoTransferInternal(const char *cmd,
		std::vector<const char*> &argList,
		StrDictListIterator *varDictIterator,
		int threads,
		Error *e);


public:
	// Create a server object and connect to the server
	P4BridgeServer( const char *p4port, 
		const char *user, const char *pass,
		const char *ws_client );

	// Finalize the connection to the server and release resources
	virtual ~P4BridgeServer(void);
	
	// Type for p4base
	virtual int Type(void) { return tP4BridgeServer; }

	// TODO: is this even required?
	P4BridgeClient * get_ui();
	P4BridgeClient * get_ui(int id) {
		return get_ui();
	}
	P4BridgeClient * find_ui(int id) {
		return get_ui();
	}

	// server connection handling
	int connected( P4ClientError **err );
	int connected_int( P4ClientError **err );
	int connect_and_trust( P4ClientError **err, char* trust_flag, char* fingerprint );
	int connect_and_trust_int(P4ClientError **err, char* trust_flag, char* fingerprint );
	int close_connection( );
	int disconnect( void );

	// Does the server support Unicode for meta data?
	int unicodeServer( );

	// The APIlevel the connected sever supports
	int APILevel();

	// Does the connected sever require login?
	int UseLogin();
	
	//Does the connected sever support extended submit options (2006.2 higher)?
	int SupportsExtSubmit();

	// If the server supports Unicode, set the character set used to
	//  communicate with the server
	string set_charset( const char* c, const char * filec = NULL );

	// Get/Set the working directory
	void set_cwd( const char* newCwd );
	string get_cwd( void );

	void Run_int(P4Connection* client, const char *cmd, P4BridgeClient* ui);

	// The 800 pound gorilla in the room, execute a command
	int run_command( const char *cmd, int cmdId, int tagged, char **args, int argc );

	int resolve( const char *file, int tagged );

	// Set the connection data used
	void set_connection(const char* newPort, const char* newUser, const char* newPassword, const char* newClient);
	void set_client( const char* newVal );
	void set_user( const char* newVal );
	void set_port( const char* newVal );
	void set_password( const char* newVal );
	void set_ticketFile( const char* newVal );
	void set_programName(const char* newVal);
	void set_programVer( const char* newVal );

	// Get the connection data used
	string get_client();
	string get_user();
	string get_port();
	string get_password();
	string get_ticketFile();
	string get_charset();
	string get_programName();
	string get_programVer();

	string get_config();
	static string get_config( const char* cwd );
	static const char* Get( const char *var );
	static void Set( const char *var, const char *value );
	static void Update(const char *var, const char *value );
	static void Reload();

	static LogCallbackFn *SetLogCallFn(LogCallbackFn *log_fn);

	void SetProtocol(const char *var, const char *value);

	void cancel_command(int cmdId);
	bool IsConnected();
		
	// If the P4 Server is Unicode enabled, the output will be in
	// UTF-8 or UTF-16 based on the char set specified by the client
	void UseUnicode(int val) { isUnicode = val; }

	// Put the calls to the callback in Structured Exception Handlers to catch
	//  any problems in the call like bad function pointers.
	void CallTextResultsCallbackFn( int cmdId, const char *data) ;
	void CallInfoResultsCallbackFn( int cmdId, int msgId, char level, const char *data );
	void CallTaggedOutputCallbackFn( int cmdId, int objId, const char *pKey, const char * pVal );
	void CallErrorCallbackFn( int cmdId, int severity, int errorId, const char * errMsg );
	void CallBinaryResultsCallbackFn( int cmdId, void * data, int length );

	// Set the call back function to receive the tagged output
	void SetTaggedOutputCallbackFn(IntTextTextCallbackFn* pNew);

	// Set the call back function to receive the error output
	void SetErrorCallbackFn(IntIntIntTextCallbackFn* pNew);

	void Prompt( int cmdId, const StrPtr &msg, StrBuf &rsp, 
				int noEcho, Error *e );

	void SetPromptCallbackFn( PromptCallbackFn * pNew);

	// Set the call back function to handle parallel transfer (see ClientTransfer)
	// TODO: it's not going to be a simple text callback
	void SetParallelTransferCallbackFn(ParallelTransferCallbackFn* pNew);

	// Set the call back function to receive the information output
	void SetInfoResultsCallbackFn(IntIntIntTextCallbackFn* pNew);

	// Set the call back function to receive the text output
	void SetTextResultsCallbackFn(TextCallbackFn* pNew);

	// Set the call back function to receive the binary output
	void SetBinaryResultsCallbackFn(BinaryCallbackFn* pNew);

	// Callbacks for handling interactive resolve
	int	Resolve( int cmdId, ClientMerge *m, Error *e );
	int	Resolve( int cmdId, ClientResolveA *r, int preview, Error *e );

	void SetResolveCallbackFn(ResolveCallbackFn * pNew);
	void SetResolveACallbackFn(ResolveACallbackFn * pNew);
	
	static int IsIgnored( const StrPtr &path );

	static string GetTicketFile( );
	static string GetTicket( char* port, char* user );

	friend class TestP4BridgeServer;
	friend class TestP4BridgeClient;

	// Epic
	void SetConnectionHost(const char* hostname);

protected:
	// the one and only connection
	P4Connection* pConnection;
	unsigned long long runThreadId;

	string user;
	string password;
	string ticketFile;
	string client;
	string p4port;

	string pProgramName;
	string pProgramVer;
	CharSetApi::CharSet charset;
	CharSetApi::CharSet fileCharset;

	string pCwd;	// cache it for reconnects

	// UI support

	// If the P4 Server is Unicode enabled, the output will be in
	// UTF-8 or UTF-16 based on the char set specified by the client
	int isUnicode;

	bool initialized;

	void setInitialized(bool initialized);
	bool isInitialized() const;

	
	P4Connection* getConnection(int id = 99999999);

	// TODO: messy, make a class for this?
	static ILockable envLock;
	static Enviro _enviro;	// storage, lock() first

	static int IsIgnored_Int( const StrPtr &path );
	string get_config_Int( );
	static string get_config_Int( const char* cwd );
	static const char* Get_Int( const char *var );
	static void Set_Int( const char *var, const char *value );
	static void Update_Int( const char *var, const char *value );
	static void Reload_Int();
	// protocol key/values need to be set at the start of a connection, not after one has been established
	// store the values and use in our getConnection() handler
	std::map<string, string> extraProtocols;

	void SetProtocol_Int(const char *var, const char *value);

   // This is where the pointer to the log callback is stored if set by the user.
	static LogCallbackFn * pLogFn;

	// The APIlevel the connected sever supports
	int apiLevel;

	// Does the connected sever require login?
	int useLogin;

	//Does the connected sever support extended submit options (2006.2 higher)?
	int supportsExtSubmit;

	int connecting;

	int disposed;
};


/*******************************************************************************
 *
 * Macros for logging
 *
 ******************************************************************************/
#define LOG_FATAL(msg) P4BridgeServer::LogMessageNoArgs(0, __FILE__ , __LINE__, msg)
#define LOG_ERROR(msg) P4BridgeServer::LogMessageNoArgs(1, __FILE__ , __LINE__, msg)
#define LOG_WARNING(msg) P4BridgeServer::LogMessageNoArgs(2, __FILE__ , __LINE__, msg)
#define LOG_INFO(msg) P4BridgeServer::LogMessageNoArgs(3, __FILE__ , __LINE__, msg)
#define LOG_DEBUG(lvl,msg) P4BridgeServer::LogMessageNoArgs(lvl, __FILE__ , __LINE__, msg)

#define LOG_FATAL1(msg, a1) P4BridgeServer::LogMessage(0, __FILE__ , __LINE__, msg, a1)
#define LOG_ERROR1(msg, a1) P4BridgeServer::LogMessage(1, __FILE__ , __LINE__, msg, a1)
#define LOG_WARNING1(msg, a1) P4BridgeServer::LogMessage(2, __FILE__ , __LINE__, msg, a1)
#define LOG_INFO1(msg, a1) P4BridgeServer::LogMessage(3, __FILE__ , __LINE__, msg, a1)
#define LOG_DEBUG1(lvl,msg, a1) P4BridgeServer::LogMessage(lvl, __FILE__ , __LINE__, msg, a1)

#define LOG_FATAL2(msg, a1, a2) P4BridgeServer::LogMessage(0, __FILE__ , __LINE__, msg, a1, a2)
#define LOG_ERROR2(msg, a1, a2) P4BridgeServer::LogMessage(1, __FILE__ , __LINE__, msg, a1, a2)
#define LOG_WARNING2(msg, a1, a2) P4BridgeServer::LogMessage(2, __FILE__ , __LINE__, msg, a1, a2)
#define LOG_INFO2(msg, a1, a2) P4BridgeServer::LogMessage(3, __FILE__ , __LINE__, msg, a1, a2)
#define LOG_DEBUG2(lvl,msg, a1, a2) P4BridgeServer::LogMessage(lvl, __FILE__ , __LINE__, msg, a1, a2)
 
#define LOG_FATAL3(msg, a1, a2, a3) P4BridgeServer::LogMessage(0, __FILE__ , __LINE__, msg, a1, a2, a3)
#define LOG_ERROR3(msg, a1, a2, a3) P4BridgeServer::LogMessage(1, __FILE__ , __LINE__, msg, a1, a2, a3)
#define LOG_WARNING3(msg, a1, a2, a3) P4BridgeServer::LogMessage(2, __FILE__ , __LINE__, msg, a1, a2, a3)
#define LOG_INFO3(msg, a1, a2, a3) P4BridgeServer::LogMessage(3, __FILE__ , __LINE__, msg, a1, a2, a3)
#define LOG_DEBUG3(lvl,msg, a1, a2, a3) P4BridgeServer::LogMessage(lvl, __FILE__ , __LINE__, msg, a1, a2, a3)
#define LOG_DEBUG4(lvl,msg, a1, a2, a3, a4) P4BridgeServer::LogMessage(lvl, __FILE__ , __LINE__, msg, a1, a2, a3, a4)

#define LOG_ENTRY() P4BridgeServer::LogMessage(4, __FILE__, __LINE__, "Entry: %s", __FUNCTION__)
#define LOG_LOC()	P4BridgeServer::LogMessage(4, __FILE__, __LINE__, "Location: %s", __FUNCTION__)
