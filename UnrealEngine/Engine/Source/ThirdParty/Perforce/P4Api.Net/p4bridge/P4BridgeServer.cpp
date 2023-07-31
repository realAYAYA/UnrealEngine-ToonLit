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
 * Name		: P4BridgeServer.cpp
 *
 * Author	: dbb
 *
 * Description	:  P4BridgeServer
 *
 ******************************************************************************/
#include "stdafx.h"
#include "P4BridgeServer.h"
#include "ConnectionManager.h"
#include "P4Connection.h"

#include <spec.h>
#include <debug.h>
#include <ignore.h>
#include <hostenv.h>
#include "ticket.h"
#include "error.h"
#include "errornum.h"
#include "strarray.h"
#include "P4BridgeEnviro.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

#define DELETE_OBJECT(obj) { if( obj != NULL ) { delete obj; obj = NULL; } }
#define DELETE_ARRAY(obj)  { if( obj != NULL ) { delete[] obj; obj = NULL; } }

#define HANDLE_EXCEPTION(ppErrStr) HandleException(__FILE__, __LINE__, __FUNCTION__, GetExceptionCode(), GetExceptionInformation(), ppErrStr)
#define HANDLE_EXCEPTION_NOSTR() HandleException(__FILE__, __LINE__, __FUNCTION__, GetExceptionCode(), GetExceptionInformation(), NULL)
#pragma comment(lib,"Version.lib")

// Epic: Only define for local debugging as it is very, very spammy
// #define EPIC_DEBUG_SPAM_LOG 1


bool CheckErrorId(const ErrorId &eid, const ErrorId &tgt)
{
    return eid.Subsystem() == tgt.Subsystem() && eid.SubCode() == tgt.SubCode();
}

bool CheckErrorId(Error  &e, const ErrorId &tgt)
{
	if (e.Test())
	{
		// iterate through the ErrorIds in this Error
		for (int i = 0; ; ++i)
		{
			ErrorId    *eid = e.GetId(i);
			if (eid == NULL)
				break;
			if (CheckErrorId(*eid, tgt) )
			{
				return true;
			}
		}
	}
	return false;
}

P4BridgeEnviro P4BridgeServer::_enviro;
ILockable P4BridgeServer::envLock;

// This is were the pointer to the log callback is stored if set by the user.
LogCallbackFn * P4BridgeServer::pLogFn = NULL;

int HandleException_Static(unsigned int c, struct _EXCEPTION_POINTERS *e)
{
#ifdef _WIN32	
	return EXCEPTION_EXECUTE_HANDLER;
#else
	return 0;
#endif
}

/******************************************************************************
// LogMessage: no formatting version, avoids getting a %blah in message with no args
//
******************************************************************************/
int P4BridgeServer::LogMessageNoArgs(int log_level, const char * file, int line, const char * message)
{

#ifndef EPIC_DEBUG_SPAM_LOG
	if (log_level == 4)
	{
		return 0;
	}
#endif

	if (!pLogFn)
		return 0;

	__try
	{
		return (*pLogFn)(log_level, file, line, message);
	}
#ifdef _WIN32	
	__except (HandleException_Static(GetExceptionCode(), GetExceptionInformation()))
	{
		// bad ptr?
		pLogFn = NULL;
	}
#else
	catch (int e) {
		// bad ptr?
		pLogFn = NULL;
	}
#endif

	return 0;
}

/******************************************************************************
// LogMessage: Use the client logging callback function (if set) to log a
//   message in the callers log.
******************************************************************************/
int P4BridgeServer::LogMessage(int log_level, const char * file, int line, const char * message, ...)
{
#ifndef EPIC_DEBUG_SPAM_LOG
	if (log_level == 4)
	{
		return 0;
	}
#endif

	if (pLogFn)
	{
		va_list args;
		va_start(args, message);

		int buffSize = 1024;
		char* buff1 = NULL;

 		int len = -1;
		while (len < 0)
		{
			DELETE_ARRAY(buff1)
			buff1 = new char[buffSize];
#ifdef _WIN32			
			len = vsnprintf_s( buff1, buffSize, buffSize - 1, message, args);
#else
			len = vsnprintf( buff1, buffSize, message, args);
#endif			
			buffSize *= 2;
		}

		int ret = 0;

		__try
		{
			ret = (*pLogFn)(log_level, file, line, buff1);
		}
#ifdef _WIN32		
		__except (HandleException_Static(GetExceptionCode(), GetExceptionInformation()))
		{
			// bad ptr?
			pLogFn = NULL;
		}
#else
		catch (int e)
		{
			// bad ptr?
			pLogFn = NULL;
		}
#endif
		DELETE_ARRAY(buff1)

		return ret;
	}
	return 0;
}

/*******************************************************************************
 *
 *  Default Constructer
 *
 *  Protected, should not be used by a client to create a P4BridgeServer.
 *
 ******************************************************************************/

P4BridgeServer::P4BridgeServer(void) :
	p4base(Type()),
	isUnicode(-1),
	useLogin(0),
	supportsExtSubmit(0),
	initialized(false),
	pConnection(NULL),
	charset(CharSetApi::NOCONV),
	fileCharset(CharSetApi::NOCONV),
	runThreadId(0),
	pTransfer(NULL),
	pParallelTransferCallbackFn(NULL)
{
}

P4BridgeClient* P4BridgeServer::get_ui()
{
	LOG_ENTRY();
	return getConnection()->getUi();
}

/*******************************************************************************
 *
 *  Constructer
 *
 *  Create a P4BridgeServer and connect to the specified P4 Server.
 *
 ******************************************************************************/

P4BridgeServer::P4BridgeServer( const char *p4port,
								const char *user,
								const char *pass,
								const char *ws_client) :
	p4base(Type()),
	isUnicode(-1),
	useLogin(0),
	supportsExtSubmit(0),
	initialized(false),
	pConnection(NULL),
	charset(CharSetApi::NOCONV),
	fileCharset(CharSetApi::NOCONV),
	runThreadId(0),
	pTransfer(NULL),
	pParallelTransferCallbackFn(NULL)
{
	LOG_DEBUG3(4,"Creating a new P4BridgeServer on %s for user, %s, and client, %s", p4port, user, ws_client);
	Locker.InitCritSection();

	LOCK(&Locker);

	disposed = 0;

	isUnicode = -1;
	useLogin = 0;
	supportsExtSubmit = 0;
	connecting = 0;

	// Clear the the callbacks
	pTaggedOutputCallbackFn = NULL;
	pErrorCallbackFn = NULL;
	pInfoResultsCallbackFn = NULL;
	pTextResultsCallbackFn = NULL;
	pBinaryResultsCallbackFn = NULL;
	pPromptCallbackFn = NULL;
	pResolveCallbackFn = NULL;
	pResolveACallbackFn = NULL;

	// connect to the server using a untagged protocol
	if (p4port)		this->p4port = p4port;
	if (user)		this->user = user;
	if (ws_client)	this->client = ws_client;
	if (pass)		this->password = pass;
}

/*******************************************************************************
 *
 *  Destructor
 *
 *  Close the connection and free up resources.
 *
 ******************************************************************************/

P4BridgeServer::~P4BridgeServer(void)
{
	if (disposed != 0)
	{
		return;
	}
	else
	{
		LOCK(&Locker);

		disposed = 1;

		// Clear the the callbacks
		pTaggedOutputCallbackFn = NULL;
		pErrorCallbackFn = NULL;
		pInfoResultsCallbackFn = NULL;
		pTextResultsCallbackFn = NULL;
		pBinaryResultsCallbackFn = NULL;
		pPromptCallbackFn = NULL;
		pResolveCallbackFn = NULL;
		pResolveACallbackFn = NULL;
		pParallelTransferCallbackFn = NULL;

		close_connection();

		delete pConnection;
	}

	Locker.FreeCriticalSection();
}

/*******************************************************************************
 *
 *  connected
 *
 *  Connect to the specified P4 Server, create a UI.
 *
 ******************************************************************************/

int P4BridgeServer::connected( P4ClientError **err )
{
	__try
	{
		return connected_int( err );
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION_NOSTR())
	{
	}
#else
	catch (int e)
	{

	}
#endif

	connecting = 0;

	return 0;
}

void P4BridgeServer::setInitialized(bool _initialized)
{
	initialized = _initialized;
}

bool P4BridgeServer::isInitialized() const
{
	return initialized;
}

int P4BridgeServer::connected_int( P4ClientError **err )
{
	LOCK(&Locker);
	LOG_ENTRY();

	*err = NULL;

	if((connecting) || (isInitialized()))
	{
		LOG_DEBUG2(4, "Connecting: %d, isInitialized(): %d", connecting, isInitialized());
		return 1; // already connected
	}
	connecting = 1;
	// Set the Unicode flag to unknown, to force a retest
	isUnicode = -1;

	apiLevel = -1;
	useLogin = -1;
	supportsExtSubmit = -1;

	LOG_LOC();
	if (GetServerProtocols(err))
	{
		p4debug.SetLevel("-vnet.maxwait=5");

		setInitialized(true);

		connecting = 0;

		return 1;
	}

	LOG_LOC();
	close_connection();

	setInitialized(false);
	connecting = 0;
	return 0;
}

/*******************************************************************************
 *
 *  connect_and_trust
 *
 *  Connect to the specified P4 Server, create a UI, and establish a trust
 *	 relationship.
 *
 ******************************************************************************/

int P4BridgeServer::connect_and_trust( P4ClientError **err, char* trust_flag, char* fingerprint )
{
	__try
	{
		return connect_and_trust_int( err, trust_flag, fingerprint );
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION_NOSTR())
	{
	}
#else
	catch (int e)
	{
		
	}
#endif

	connecting = 0;

	return 0;
}

int P4BridgeServer::connect_and_trust_int( P4ClientError **err, char* trust_flag, char* fingerprint )
{
	LOCK(&Locker);

	if (connecting || isInitialized())
	{
		return 1; // already connected
	}
	connecting = 1;
	LOG_LOC();
	P4Connection* pCon = getConnection();

	char** args = new char*[2];
	args[0] = (char *) "-d";

	run_command( "trust", 0, 1, args, 1 );

	args[0] = trust_flag;
	args[1] = fingerprint;

	if (!run_command( "trust", 0, 1, args, (fingerprint != NULL)?2:1 ))
	{
		P4ClientError *e = pCon->getUi()->GetErrorResults();
		if ((e!= NULL) && (e->Severity >= E_FAILED))
		{
			*err = e;
		}

		disconnect();
		return 0;
	}

	// Set the Unicode flag to unknown, to force a retest
	isUnicode = -1;

	apiLevel = -1;
	useLogin = -1;
	supportsExtSubmit = -1;

	if (GetServerProtocols(err))
	{
		p4debug.SetLevel("-vnet.maxwait=5");

		setInitialized(true);

		connecting = 0;

		return 1;
	}

	LOG_LOC();
	close_connection();

	setInitialized(false);
	connecting = 0;

	return 0;
}

/*******************************************************************************
 *
 * close_connection
 *
 *  Final disconnect from the P4 Server.
 *
 ******************************************************************************/

int P4BridgeServer::close_connection()
{
	LOCK(&Locker);
	LOG_ENTRY();

	// Close connections
	Error e;
	if (pConnection)
	{
		LOG_LOC();
		pConnection->Final(&e);
	}

	// the ClientUser (P4BridgeClient) deletes the transfer object
	// this implies that we're probably not managing the transfer object
	// in the expected way due to the P4API.Net layer's configuration
	// abilities. Anyway, don't delete it.
	pTransfer = NULL;

	// should we check e?  if the connection was invalid Final()
	// will return a bad result, so ignore it and complete the cleanup
#if 0
	if (e.Test())
	{
		return 0;
	}
#endif

	DELETE_OBJECT(pConnection);

	// Set the Unicode flag to unknown, to force a retest
	isUnicode = -1;

	apiLevel = -1;
	useLogin = -1;
	supportsExtSubmit = -1;
	setInitialized(false);

	return 1;
}

/*******************************************************************************
 *
 * disconnect
 *
 *  Disconnect from the P4 Server after a command, but save protocols and other
 *	 settings.
 *
 ******************************************************************************/

int P4BridgeServer::disconnect( void )
{
	LOCK(&Locker);
	LOG_ENTRY();

	if (pConnection)
	{
		pConnection->Disconnect();
		// don't delete it.  it's possible that someone would
		// disconnect (p4api.net auto-disconnects after N seconds)
		// and fetch the results later.  This used to break a GetConfig()
		// test in p4api.net, but refactoring how set_cwd works seems
		// to have made the behavior identical without the explicit delete here
	}

	return 1;
}

/*******************************************************************************
 *
 *  get_charset
 *
 * Get the character set from the environment.
 *
 ******************************************************************************/

string P4BridgeServer::get_charset( )
{
	LOG_ENTRY();
	// TODO: store the string charset name instead of potentially regenerating a connection
	return getConnection()->GetCharset().Text();
}

CharSetApi::CharSet GetDefaultCharSet()
{
#ifdef _WIN32	
    switch (GetACP())
    {
        case 437:   return CharSetApi::WIN_US_OEM;
        case 737:	return CharSetApi::CP737;
        case 932:   return CharSetApi::SHIFTJIS;
        case 936:   return CharSetApi::CP936;
        case 949:   return CharSetApi::CP949;
        case 950:   return CharSetApi::CP950;
        case 1200:  return CharSetApi::UTF_16_LE_BOM;
        case 1201:  return CharSetApi::UTF_16_BE_BOM;
        case 1251:  return CharSetApi::WIN_CP_1251;
        case 1253:  return CharSetApi::CP1253;
        case 10000: return CharSetApi::MACOS_ROMAN;
        case 12000: return CharSetApi::UTF_32_LE_BOM;
        case 12001: return CharSetApi::UTF_32_BE_BOM;
        case 20866: return CharSetApi::KOI8_R;
        case 20932: return CharSetApi::EUCJP;
        case 28591: return CharSetApi::ISO8859_1;
        case 28595: return CharSetApi::ISO8859_5;
        case 28597: return CharSetApi::ISO8859_7;
        case 28605: return CharSetApi::ISO8859_15;
        case 65001: return CharSetApi::UTF_8;

        default:
        case 1252:  return CharSetApi::WIN_US_ANSI;
   }
#else 
	// EPIC: add locale info and map to char set here
	return CharSetApi::UTF_8;
#endif

}

/*******************************************************************************
 *
 *  set_charset
 *
 * Set the character set for encoding Unicode strings for command parameters
 *  and output. Optionally, a separate encoding can be specified for the
 *  contents of files that are directly saved in the client's file system.
 *
 ******************************************************************************/

string P4BridgeServer::set_charset( const char* c, const char * filec )
{
	CharSetApi::CharSet cs;
	if (c)
	{
		// Lookup the correct enum for the specified character set for the API
		cs = CharSetApi::Lookup( c );
		if( cs < 0 )
		{
			StrBuf	m;
			m = "Unknown or unsupported charset: ";
			m.Append( c );

			LOG_ERROR( m.Text() );
			return m.Text();
		}
	}
	else
	{
		cs = CharSetApi::UTF_8;
		c = CharSetApi::Name(cs);
	}

	CharSetApi::CharSet filecs;

	// Lookup the correct enum for the specified character set for file
	//  contents
	if (filec)
	{
		filecs = CharSetApi::Lookup( filec );
		if( filecs < 0 )
		{
			StrBuf	m;
			m = "Unknown or unsupported charset: ";
			m.Append( filec );

			LOG_ERROR( m.Text() );
			return m.Text();
		}
	}
	else
	{
		// default value
		filecs = CharSetApi::WIN_US_ANSI;

		LOG_LOC();
		string filec = getConnection()->GetCharset().Text();

		if (!filec.empty())
		{
			filecs = CharSetApi::Lookup(filec.c_str());
			if ((int)filecs <= 0)
			{
				// not set, get a value based on the system code page
				filecs = GetDefaultCharSet();
			}
		}
	}
	LOG_INFO1( "[P4] Setting charset: %s\n", CharSetApi::Name(cs) );

	// record for reconnects
	charset = cs;
	fileCharset = filecs;

	LOG_LOC();
	getConnection()->SetCharset( cs, filecs );

	return "";
}

/*******************************************************************************
 *
 * set_cwd
 *
 *  Set the working directory.
 *
 ******************************************************************************/

void P4BridgeServer::set_cwd( const char* newCwd )
{
	// cache for later
	pCwd = (newCwd) ? newCwd : "";
	LOG_DEBUG2(4, "Setting 0x%llu cwd to %s", pConnection, pCwd.c_str());
	LOG_DEBUG1(4, "P4CONFIG: %s", this->Get("P4CONFIG"));
	// don't create a connection just for the CWD, but if the connection existed
	// already and is unconnected, go ahead and delete it.  It will be re-created
	// on-demand
	if (pConnection)
	{
		if (!pConnection->IsConnected())
		{
			DELETE_OBJECT(pConnection);	// pCwd will get set when the connection is created
		}
		else
		{
			pConnection->SetCwd(pCwd.c_str());
		}
	}

}

/*******************************************************************************
 *
 * get_cwd
 *
 *  Get the working directory.
 *
 ******************************************************************************/

static StrBuf EmptStr("");

string P4BridgeServer::get_cwd( void )
{
	LOG_LOC();
	return getConnection()->GetCwd().Text();

}

// After the Run call is complete, with no exceptions thrown
//  it is time to look for the "hidden" parallel sync errors - see job076982
void checkForParallelError(int existingErrors, P4Connection* client, const char *cmd, P4BridgeClient* ui)
{
	// client does not clear its error count, so we need to detect that the error count changed
	// but we never got any HandleError() calls in our ClientUser object (P4BridgeClient)
	// we could check that it's a cmd that is parallel available (sync and submit), but
	// this is a little more future-friendly
	if (existingErrors != client->GetErrors() && ui->GetErrorResults() == NULL)
	{
		ui->HandleError(E_FAILED, 0, "Error detected during parallel operation");
	}
}

void P4BridgeServer::Run_int(P4Connection* client, const char *cmd, P4BridgeClient* ui)
{
	string* pErrorString = NULL;

	__try
	{
		int existingErrors = client->GetErrors();
		LOG_DEBUG1(4, "Run_int is running '%s'", cmd);
		client->Run(cmd, ui);
		LOG_DEBUG1(4, "Run_int returned from '%s'", cmd);
		checkForParallelError(existingErrors, client, cmd, ui);
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		if (ui)
		{
			ui->HandleError( E_FATAL, 0, pErrorString->c_str() );
			DELETE_OBJECT(pErrorString);
		}
	}
#else
	catch (int e) 
	{

	}
#endif
}



char *GetInfo(char* lpstrVffInfo, char *InfoItem)
{

#ifdef _WIN32
    char*   szResult = new char[256];
    char    szGetName[256];
    LPSTR   lpVersion;        // String pointer to Item text
    DWORD   dwVerHnd=0;       // An 'ignored' parameter, always '0'
    UINT    uVersionLen;
    BOOL    bRetCode;

    // Get a codepage from base_file_info_sctructure
    lstrcpy(szGetName, "\\VarFileInfo\\Translation");
    uVersionLen   = 0;
    lpVersion     = NULL;
    bRetCode = VerQueryValue((LPVOID)lpstrVffInfo,
            (LPSTR)szGetName,
            (void **)&lpVersion,
            (UINT *)&uVersionLen);
    if ( bRetCode && uVersionLen && lpVersion)
	{
        sprintf_s(szResult, 256, "%04x%04x", (WORD)(*((DWORD *)lpVersion)),
            (WORD)(*((DWORD *)lpVersion)>>16));
    }
    else {
        // 041904b0 is a very common one, because it means:
        //   US English/Russia, Windows MultiLingual characterset
        // Or to pull it all apart:
        // 04------        = SUBLANG_ENGLISH_USA
        // --09----        = LANG_ENGLISH
        // --19----        = LANG_RUSSIA
        // ----04b0 = 1200 = Codepage for Windows:Multilingual
        lstrcpy(szResult, "041904b0");
    }

    // Add a codepage to base_file_info_sctructure
    sprintf_s (szGetName, 256, "\\StringFileInfo\\%s\\", szResult);
    // Get a specific item
    lstrcat (szGetName, InfoItem);

    uVersionLen   = 0;
    lpVersion     = NULL;
    bRetCode = VerQueryValue((LPVOID)lpstrVffInfo,
            (LPSTR)szGetName,
            (void **)&lpVersion,
            (UINT *)&uVersionLen);
    if ( bRetCode && uVersionLen && lpVersion)
	{
        lstrcpy(szResult, lpVersion);
    }
    else
	{
		delete[] szResult;
        szResult = NULL;
    }

	// if szResult is NULL return an empty string
	return szResult == nullptr ? "" : szResult;
#else

	// EPIC: todo
	return (char *) "";

#endif
}

/*******************************************************************************
 *
 * run_command
 *
 * Run a command using the supplied parameters. The command can either be run
 *  in tagged or untagged protocol. If the target server supports Unicode, the
 *  strings in the parameter list need to be encoded in the character set
 *  specified by a previous call to set_charset().
 *
 ******************************************************************************/

int P4BridgeServer::run_command( const char *cmd, int cmdId, int tagged, char **args, int argc )
{
	P4ClientError *err = NULL;
	LOG_ENTRY();

	if( connected( &err ) )
	{
		LOG_LOC();
		DELETE_OBJECT(err)
	}
	Error e;

	StrBuf msg;

	P4Connection* connection = getConnection(cmdId);
	if(!connection)
	{
		LOG_ERROR1( "Error getting connection for command: %d", cmdId );
		return 0;
	}

	P4BridgeClient* ui = connection->getUi();
	if (ui)
	{
		if (err != NULL)
		{
			// couldn't connect
			ui->HandleError( err );
			return 0;
		}
		ui->clear_results();
	}
	else
	{
		LOG_ERROR("connection did not have a P4BridgeClient ui object");
		return 0;
	}

	connection->IsAlive(1);

	// Connect to server
	if(connection->Dropped())
	{
		connection->Final(&e);
		if( e.Test() )
		{
			ui->HandleError(&e);
			return 0;
		}
		connection->clientNeedsInit = 1;
	}
	if (connection->clientNeedsInit)
	{
		connection->Init( &e );
		if( e.Test() )
		{
			ui->HandleError(&e);
			return 0;
		}
		connection->clientNeedsInit = 0;
	}
	bool setProdName = pProgramName.empty();
	bool setProdVer = pProgramVer.empty();

#ifdef _WIN32
	char* pModPath = NULL;
	if (setProdName || setProdVer)
	{
		// need to get the module path to set either the name and/or version
		pModPath = new char[MAX_PATH];
		if ( GetModuleFileName( NULL, pModPath, MAX_PATH ) == 0 )
		{
			delete[] pModPath ;
			pModPath = NULL;
		}
	}
	// Label Connections for p4 monitor

	if (setProdVer && pModPath)
	{
		DWORD sz = GetFileVersionInfoSize(pModPath, NULL);
		UINT BufLen;

		if (sz > 0)
		{
			VS_FIXEDFILEINFO *pFileInfo;
			char* lpData = new char[sz];
			if (GetFileVersionInfo( pModPath, 0, sz, lpData  ))
			{
				pProgramVer = GetInfo(lpData, "ProductVersion");

				if (pProgramVer.empty())
				{
					if (VerQueryValue( lpData, "\\", (LPVOID *) &pFileInfo, (PUINT)&BufLen ) )
 					{
						WORD MajorVersion = HIWORD(pFileInfo->dwProductVersionMS);
						WORD MinorVersion = LOWORD(pFileInfo->dwProductVersionMS);
						WORD BuildNumber = HIWORD(pFileInfo->dwProductVersionLS);
						WORD RevisionNumber = LOWORD(pFileInfo->dwProductVersionLS);

						std::stringstream ss;
						ss << MajorVersion << "." << MinorVersion << "." << BuildNumber << "." << RevisionNumber;
						pProgramVer = ss.str();
					}
				}
			}
			if (setProdName)
			{
				char* prodName = GetInfo(lpData, "ProductName");
				if (prodName)
				{
					pProgramName = prodName;
					setProdName = false;
				}
			}
			free (lpData);
		}
#ifdef _DEBUG
		// in debug versions use the error message saying why we couldn't get a version string
		// as the version string.
		else
		{
			LPTSTR errorText = NULL;

			DWORD err = GetLastError();
			DWORD retSize=FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
                           FORMAT_MESSAGE_FROM_SYSTEM|
                           FORMAT_MESSAGE_ARGUMENT_ARRAY,
                           NULL,
                           err,
                           LANG_NEUTRAL,
                           (LPTSTR)&errorText,
                           0,
                           NULL );
			pProgramVer = errorText;
			LocalFree(errorText);
		}
#endif
	}
	if (setProdName && pModPath)
	{
		// need to set the product name, and we're not going to do
		// later when we set the product version, and we have the
		// path of the module that loaded the dll
		int idx1 = 0;
		int idx2 = 0;
		while ((idx1 < MAX_PATH) && (pModPath[idx1] != '\0'))
		{
			if (pModPath[idx1++] == '\\')
			{
				// interested in the character after the last '\\'
				idx2 = idx1;
			}
		}
		if (idx2 < idx1)
		{
			pProgramName = new char[(idx1 - idx2) + 1];
			idx1 = idx2;
			while ((idx1 < MAX_PATH) && (pModPath[idx1] != '\0'))
			{
				pProgramName[idx1-idx2] = pModPath[idx1++];
			}
			pProgramName[idx1-idx2] = '\0';
		}
	}
	DELETE_ARRAY(pModPath);

#endif

	if (!pProgramName.empty())
		connection->SetProg( pProgramName.c_str() );
	else
		connection->SetProg( "dot-net-api-p4" );

	if (!pProgramVer.empty())
		connection->SetVersion( pProgramVer.c_str() );
	else
		connection->SetVersion( "NoVersionSpecified"); //Nobody liked "1.0" );

	connection->SetVar(P4Tag::v_tag, tagged ? "yes" : 0);

	connection->SetArgv( argc, args );
	connection->SetBreak(connection);
	if (pParallelTransferCallbackFn) {
		// make a transfer object?
		if (!pTransfer)
			pTransfer = new ParallelTransfer(this);
		ui->SetTransfer(pTransfer);
	}
	else
	{
		// make sure we clear it
		ui->SetTransfer(NULL);
	}

	Run_int(connection, cmd, ui);

	P4ClientError* errors = ui->GetErrorResults();
	if (errors != NULL)
	{
		int maxSeverity = errors->MaxSeverity();
		if ( maxSeverity >= 3 )
		{
			return 0;
		}
	}
	if(connection->Dropped())
	{
		connection->Final(&e);
		if( e.Test() )
		{
			ui->HandleError(&e);
		}
		connection->clientNeedsInit = 1;
	}

	if (connection->IsAlive() == 0)
	{
		// the command was cancelled
		return 0;
	}

	// Fix for Job085941
	// In parallel activities, error reports may "get eaten" but the api now provides the GetErrors() call
	//  which returns a non-zero error count if one of the "transmit" threads has had a problem.
	//if (connection->GetErrors())
	//{
	//	return 0;
	//}

	return 1;
}

P4Connection* P4BridgeServer::getConnection(int id /*= 99999999*/)
{
	if (!pConnection)
	{
		LOG_LOC();
		pConnection = new P4Connection(this, id);
		if (!client.empty()) pConnection->SetClient(client.c_str());
		if (!user.empty()) pConnection->SetUser(user.c_str());
		if (!p4port.empty()) pConnection->SetPort(p4port.c_str());
		LOG_DEBUG2(4, "getting connection with P4PORT=%s/%s", p4port.c_str(), pConnection->GetPort().Text());
		if (!password.empty()) pConnection->SetPassword(password.c_str());
		if (!pProgramName.empty()) pConnection->SetProg(pProgramName.c_str());
		if (!pProgramVer.empty()) pConnection->SetVersion(pProgramVer.c_str());
		if (!pCwd.empty()) pConnection->SetCwd(pCwd.c_str());
		if (!ticketFile.empty()) pConnection->SetTicketFile(ticketFile.c_str());

		pConnection->SetProtocol("specstring", "");
		pConnection->SetProtocol("enableStreams", "");
		pConnection->SetProtocol("enableGraph", "");
		pConnection->SetProtocol("unicode", "");

		// also set the extraProtocols
		for (std::map<string, string>::iterator it = extraProtocols.begin(); it != extraProtocols.end(); ++it)
			pConnection->SetProtocol(it->first.c_str(), it->second.c_str());

		// Set the character set for the untagged client
		pConnection->SetCharset(charset, fileCharset);
	}
	else
	{
		// TODO: eliminate cmdId?
		// just set the cmdId
		pConnection->cmdId = id;
	}
	return pConnection;
}

void P4BridgeServer::cancel_command(int cmdId)
{
	LOG_ENTRY();
	getConnection()->cancel_command();
}

bool P4BridgeServer::IsConnected()
{
	return pConnection && pConnection->IsConnected();
}

int P4BridgeServer::GetServerProtocols(P4ClientError **err)
{
	LOG_ENTRY();

	if (isUnicode >= 0)
	{
		// already read the protocols
		return 1;
	}

	LOG_LOC();
	// set to 0 for now so we don't call this again when running the help
	//   command to get the protocols
	isUnicode = 0;

	// running the 'help' command on the server is the only command that
	//   does not lock any tables on the server, so it has the least impact.

	P4Connection* pCon = getConnection();

	// abort if we can't proceed (need at least to get a value for server2)
	if (!run_command("help", 0, 1, NULL, 0))
	{
		LOG_DEBUG(4, "run help command failed");
		// store the error
		*err = pCon->getUi()->GetErrorResults();

		// clear the error pointer or it will get deleted when/if the connection is
		// closed due to the error
		pCon->getUi()->ClearErrorResults();

		// even if it failed, it may have gotten enough protocol information to proceed
		// this can happen in situations where the server is so locked down that even "help" requires a login
		// note that the GetProtocol() return pointer is only valid until the next GetProtocol call
		StrPtr* server2 = pCon->GetProtocol("server2");
		if (!server2 || server2->Length() == 0 || server2->Atoi() == 0)
		{
			disconnect();
			return 0;
		}
	}

	// Check server level
	{
		StrPtr *server2 = pCon->GetProtocol("server2");
		apiLevel = (server2) ? server2->Atoi() : 0;

		// Login/logout capable [2004.2 higher]
		if (apiLevel >= SERVER_SECURITY_PROTOCOL) {
			useLogin = 1;
		}
		else
		{
			useLogin = 0;
		}
	}

	// Supports new submit options [2006.2 higher]
	if ( apiLevel >= SERVER_EXTENDED_SUBMIT ) {
		supportsExtSubmit = 1;
	}
	else
	{
		supportsExtSubmit = 0;
	}

	// check the unicode setting
	{
		StrPtr *unicode = pCon->GetProtocol(P4Tag::v_unicode);
		if (unicode && unicode->Length() && unicode->Atoi())
		{
			isUnicode = 1;
		}
		else
		{
			isUnicode = 0;
		}
	}

	// LOG_DEBUG4(0, "api, useLogin, isUnicode, supportsExtSubmit: %d, %d, %d", apiLevel, useLogin, isUnicode, supportsExtSubmit);
	return 1;
}

/*******************************************************************************
 *
 * unicodeServer
 *
 * Does the connected server support unicode? If already determined, return the
 *  cached results, otherwise issue a help command and query the server to see
 *  if Unicode support is enabled.
 *
 ******************************************************************************/

int P4BridgeServer::unicodeServer(  )
{
	P4ClientError* err = NULL;
	GetServerProtocols(&err);
	DELETE_OBJECT(err);

	return isUnicode;
}

/*******************************************************************************
 *
 * APILevel
 *
 * The API level the connected server supports If already determined, return the
 *  cached results, otherwise issue a help command and query the server to see
 *  what protocols the server supports.
 *
 ******************************************************************************/

int P4BridgeServer::APILevel(  )
{
	P4ClientError* err = NULL;
	GetServerProtocols(&err);
	DELETE_OBJECT(err);

	return apiLevel;
}

/*******************************************************************************
 *
 * UseLogin
 *
 * Does the connected server require the login command be used? If already
 *  determined, return the cached results, otherwise issue a help command and
 *  query the server to see if Unicode support is enabled.
 *
 ******************************************************************************/

int P4BridgeServer::UseLogin()
{
	LOG_LOC();
	P4ClientError* err = NULL;
	GetServerProtocols(&err);
	DELETE_OBJECT(err);

	return useLogin;
}

//Does the connected sever support extended submit options (2006.2 higher)?
/*******************************************************************************
 *
 * SupportsExtSubmit
 *
 * Does the connected server support extended submit options (2006.2 higher)?
 *  If already determined, return the cached results, otherwise issue a help
 *  command and query the server to see if Unicode support is enabled.
 *
 ******************************************************************************/
int P4BridgeServer::SupportsExtSubmit()
{
	P4ClientError* err = NULL;
	GetServerProtocols(&err);
	DELETE_OBJECT(err);

	return supportsExtSubmit;
}

/*******************************************************************************
 *
 * SetConnection
 *
 * Set some or all of the parameters used for the connection.
 *
 ******************************************************************************/

void  P4BridgeServer::set_connection(const char* newPort,
									const char* newUser,
									const char* newPassword,
									const char* newClient)
{
	// close the connection to force reconnection with new value(s)
	LOG_ENTRY();
	close_connection();

	if (newPort)
	{
		this->p4port = newPort;
	}

	if (newUser)
	{
		this->user = newUser;
	}

	if (newPassword)
	{
		this->password = newPassword;
	}

	if (newClient)
	{
		this->client = newClient;
	}
}

/*******************************************************************************
 *
 * set_client
 *
 * Set the workspace used for the connection.
 *
 ******************************************************************************/

void P4BridgeServer::set_client( const char* newVal )
{
	// close the connection to force reconnection with new value(s)
	LOG_ENTRY();
	this->client = (newVal ? newVal : "");
	if (pConnection)
		pConnection->SetClient(this->client.c_str());
}

/*******************************************************************************
 *
 * set_user
 *
 * Set the user name used for the connection.
 *
 ******************************************************************************/

void P4BridgeServer::set_user( const char* newVal )
{
	// close the connection to force reconnection with new value(s)
	LOG_ENTRY();
	this->user = (newVal ? newVal : "");
	// close_connection();
	if (pConnection)
		pConnection->SetUser(this->user.c_str());
}


/*******************************************************************************
 *
 * set_port
 *
 * Set the port (hostname:portnumber) used for the connection.
 *
 ******************************************************************************/

void P4BridgeServer::set_port( const char* newVal )
{
	LOG_ENTRY();
	// close the connection to force reconnection with new value(s)
	close_connection();
	this->p4port = (newVal ? newVal : "");
}

/*******************************************************************************
 *
 * set_password
 *
 * Set the password used for the connection.
 *
 ******************************************************************************/

void P4BridgeServer::set_password( const char* newVal )
{
	// close the connection to force reconnection with new value(s)
	LOG_ENTRY();
	this->password = (newVal ? newVal : "");
	if (pConnection)
		pConnection->SetPassword(this->password.c_str());
}

/*******************************************************************************
 *
 * set_ticketFile
 *
 * Set the ticket file used for the connection.
 *
 ******************************************************************************/

void P4BridgeServer::set_ticketFile(const char* newVal)
{
	// close the connection to force reconnection with new value(s)
	LOG_ENTRY();
	this->ticketFile = (newVal ? newVal : "");
	if (pConnection)
		pConnection->SetTicketFile(this->ticketFile.c_str());
}

/*******************************************************************************
 *
 * set_programName
 *
 * Set the program name used for the connection.
 *
 ****************************************************if (pConnection)**************************/

void P4BridgeServer::set_programName( const char* newVal )
{
	pProgramName = (newVal ? newVal : "");
}

/*******************************************************************************
 *
 * set_programVer
 *
 * Set the program version used for the connection.
 *
 ******************************************************************************/

void P4BridgeServer::set_programVer( const char* newVal )
{
	pProgramVer = (newVal ? newVal : "");
}

/*******************************************************************************
 *
 * get_client
 *
 *  Get the workspace used for the connection.
 *
 ******************************************************************************/

string P4BridgeServer::get_client()
{
	LOG_ENTRY();
	return getConnection()->GetClient().Text();
}

/*******************************************************************************
 *
 * get_user
 *
 *  Get the user name used for the connection.
 *
 ******************************************************************************/

string P4BridgeServer::get_user()
{
	LOG_ENTRY();
	return getConnection()->GetUser().Text();
}

/*******************************************************************************
 *
 * get_port
 *
 *  Get the user port used for the connection.
 *
 ******************************************************************************/

string P4BridgeServer::get_port()
{
	LOG_ENTRY();
	return getConnection()->GetPort().Text();
}

/*******************************************************************************
 *
 * get_password
 *
 *  Get the password used for the connection.
 *
 ******************************************************************************/

string P4BridgeServer::get_password()
{
	LOG_ENTRY();
	return getConnection()->GetPassword().Text();
}

/*******************************************************************************
 *
 * get_ticketFile
 *
 *  Get the ticket file used for the connection.
 *
 ******************************************************************************/

string P4BridgeServer::get_ticketFile()
{
	LOG_ENTRY();
	return ticketFile;
}

/*******************************************************************************
 *
 * get_programName
 *
 *  Get the program name used for the connection.
 *
 ******************************************************************************/

string P4BridgeServer::get_programName()
{
	return pProgramName;
}

/*******************************************************************************
 *
 * get_programVer
 *
 *  Get the program version used for the connection.
 *
 ******************************************************************************/

string P4BridgeServer::get_programVer()
{
	return pProgramVer;
}

/*******************************************************************************
 *
 * get_config
 *
 *  Get the config file used for the connection, if any.
 *
 ******************************************************************************/

string P4BridgeServer::get_config_Int(const char * cwd)
{
	// NOTE: do not use _enviro, this is a hypothetical question about a directory
	Enviro enviroLocal;
	LOG_LOC();
	// update (not set) P4CONFIG to _enviro's.  This allows API users to use "Update"
	// to alter the P4CONFIG locally without setting in the system registry
	LOCK(&envLock);
	// if the P4CONFIG for the env is null, don't bother updating enviroLocal
	LOG_DEBUG1(4, "_enviro.Get(P4CONFIG) = %s", _enviro.Get("P4CONFIG"));
	const char* curConfig = _enviro.Get("P4CONFIG");
	if (curConfig != NULL)
		enviroLocal.Update("P4CONFIG", curConfig);
	// reload the configuration
	LOG_DEBUG1(4, "Set enviroLocal to %s", cwd);
	enviroLocal.Config(StrRef(cwd));

	StrBuf sb = enviroLocal.GetConfig();
	LOG_DEBUG1(4, "enviroLocal config is %s", sb.Text());

	if (sb == "noconfig")
	return sb.Text();

	const StrArray* ret = enviroLocal.GetConfigs();
	const StrBuf* sbp = ret->Get(0);
	LOG_DEBUG1(4, "enviroLocal config is %s", sbp->Text());
		return sbp->Text();
}

string P4BridgeServer::get_config(const char * cwd)
{
	LOG_ENTRY();
	return P4BridgeServer::get_config_Int(cwd);
}

/*******************************************************************************
 *
 * get_config
 *
 *  Get the config file used for the connection, if any.
 *
 ******************************************************************************/

string P4BridgeServer::get_config_Int()
{
	LOG_ENTRY();
	P4ClientError *err = NULL;
	if ( !connected( &err ) )
	{
		return "";
	}

	P4Connection* pCon = getConnection();
	// if this seems weird, it's probably because it is
	// pCon may actually be disconnected, as this class
	// considers "connected" as "i got the protocol data
	// but may not actually be talking to the server".
	// The config info is read when the Client gets created,
	// and apparently we want something more responsive
	LOG_DEBUG1(4, "pCon CWD: %s", pCon->GetCwd().Text());
	const StrPtr& ret = pCon->GetConfig();
	StrBuf sb = pCon->GetConfig(); ;// = ret;

	if (sb == "noconfig")
		return ret.Text();

	const StrArray* retA = pCon->GetConfigs();
	const StrBuf* sbp = retA->Get(0);
	LOG_DEBUG1(4, "pCon CWD: %s", sbp->Text());
	return sbp->Text();
}

string P4BridgeServer::get_config()
{
	return P4BridgeServer::get_config_Int();
}

/*******************************************************************************
 *
 *  HandleException
 *
 *  Handle any platform exceptions. The Microsoft Structured Exception Handler
 *      allows software to catch platform exceptions such as array overrun. The
 *      exception is logged, but the application will continue to run.
 *
 ******************************************************************************/

#ifdef _WIN32
int P4BridgeServer::HandleException(const char* fname, unsigned int line, const char* func, unsigned int c, struct _EXCEPTION_POINTERS *e, string** ppErrorString)
{
	if (!this->disposed) // hopefully didn't get called on an already deleted object
	{
		unsigned int code = c;
		struct _EXCEPTION_POINTERS *ep = e;

		// Log the exception
		const char * exType = "Unknown";

		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION:
			exType = "EXCEPTION_ACCESS_VIOLATION\r\n";
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			exType = "EXCEPTION_DATATYPE_MISALIGNMENT\r\n";
			break;
		case EXCEPTION_BREAKPOINT:
			exType = "EXCEPTION_BREAKPOINT\r\n";
			break;
		case EXCEPTION_SINGLE_STEP:
			exType = "EXCEPTION_SINGLE_STEP\r\n";
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			exType = "EXCEPTION_ARRAY_BOUNDS_EXCEEDED\r\n";
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			exType = "EXCEPTION_FLT_DENORMAL_OPERAND\r\n";
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			exType = "EXCEPTION_FLT_DIVIDE_BY_ZERO\r\n";
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			exType = "EXCEPTION_FLT_INEXACT_RESULT\r\n";
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			exType = "EXCEPTION_FLT_INVALID_OPERATION\r\n";
			break;
		case EXCEPTION_FLT_OVERFLOW:
			exType = "EXCEPTION_FLT_OVERFLOW\r\n";
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			exType = "EXCEPTION_FLT_STACK_CHECK\r\n";
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			exType = "EXCEPTION_FLT_UNDERFLOW\r\n";
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			exType = "EXCEPTION_INT_DIVIDE_BY_ZERO\r\n";
			break;
		case EXCEPTION_INT_OVERFLOW:
			exType = "EXCEPTION_INT_OVERFLOW\r\n";
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			exType = "EXCEPTION_PRIV_INSTRUCTION\r\n";
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			exType = "EXCEPTION_IN_PAGE_ERROR\r\n";
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			exType = "EXCEPTION_ILLEGAL_INSTRUCTION\r\n";
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			exType = "EXCEPTION_NONCONTINUABLE_EXCEPTION\r\n";
			break;
		case EXCEPTION_STACK_OVERFLOW:
			exType = "EXCEPTION_STACK_OVERFLOW\r\n";
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			exType = "EXCEPTION_INVALID_DISPOSITION\r\n";
			break;
		case EXCEPTION_GUARD_PAGE:
			exType = "EXCEPTION_GUARD_PAGE\r\n";
			break;
		case EXCEPTION_INVALID_HANDLE:
			exType = "EXCEPTION_INVALID_HANDLE\r\n";
			break;
		default:
			printf("UNKNOWN EXCEPTION\r\n");
			break;
		}

		std::stringstream ss;

		ss << fname << "(" << line << "): " << func << " : Exception Detected: ("
			"0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << code << ")" << exType;
		LOG_ERROR(ss.str().c_str());
		if (ppErrorString)
		{
			*ppErrorString = new string();
			**ppErrorString = ss.str().c_str();
		}
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

int P4BridgeServer::sHandleException(unsigned int c, struct _EXCEPTION_POINTERS *e)
{
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

const char* P4BridgeServer::Get_Int( const char *var )
{
	LOCK(&envLock);
	return _enviro.Get( var );
}

const char* P4BridgeServer::Get( const char *var )
{
	return P4BridgeServer::Get_Int( var );
}

void P4BridgeServer::Set_Int( const char *var, const char *value )
{
	LOCK(&envLock);

	// Enviro is a little weird, if you set a NULL value it deletes from the
	// registry but does not clear the symbol table value (it sort of does,
	// but the value is still there).
	Error e;
	_enviro.Set( var, value, &e );
	// workaround for P4-16150, also call Update to modify the cache
	_enviro.Update(var, value);

	if( e.Test() )
	{
		return;
	}
}

void P4BridgeServer::Set( const char *var, const char *value )
{
	__try
	{
		return P4BridgeServer::Set_Int( var, value );
	}
#ifdef _WIN32	
	__except (P4BridgeServer::sHandleException(GetExceptionCode(), GetExceptionInformation()))
	{
	}
#else
	catch (int e)
	{

	}
#endif
}

void P4BridgeServer::Update_Int( const char *var, const char *value )
{
	LOCK(&envLock);
	_enviro.Update( var, value);;
}

void P4BridgeServer::Update( const char *var, const char *value )
{
	__try
	{
		return P4BridgeServer::Update_Int( var, value );
	}
#ifdef _WIN32	
	__except (P4BridgeServer::sHandleException(GetExceptionCode(), GetExceptionInformation()))
	{
	}
#else
	catch (int e)
	{

	}
#endif
}

void P4BridgeServer::Reload_Int()
{
	LOCK(&envLock);
	_enviro.Reload();
}

void P4BridgeServer::Reload()
{
	__try
	{
		return P4BridgeServer::Reload_Int();
	}
#ifdef _WIN32	
	__except (P4BridgeServer::sHandleException(GetExceptionCode(), GetExceptionInformation()))
	{
	}
#else
	catch (int e)
	{
		
	}
#endif
}

void P4BridgeServer::SetProtocol_Int(const char *var, const char *value)
{
	// Note: this must be called before connecting or the server will ignore
	//       and we only do that when getConnection() is called
	LOG_ENTRY();
	// save for new connections
	extraProtocols[var] = value;
	// if one has already been created, set it there too (might not be connected, just the object)
	if (pConnection)
	{
		if (pConnection->IsConnected())
		{
			LOG_DEBUG2(4, "Trying to set %s=%s on and active connection, need to disconnect first", var, value);
		}
		pConnection->SetProtocol(var, value);
	}
}

void P4BridgeServer::SetProtocol(const char *var, const char *value)
{
	__try
	{
		return P4BridgeServer::SetProtocol_Int(var, value);
	}
#ifdef _WIN32	
	__except (P4BridgeServer::sHandleException(GetExceptionCode(), GetExceptionInformation()))
	{
	}
#else
	catch (int e)
	{
		
	}
#endif
}

/*******************************************************************************
 *
 *  CallTextResultsCallbackFn
 *
 *  Simple wrapper to call the callback function (if it has been set) within a
 *      SEH __try block to catch any platform exception. SEH __try blocks must
 *      be contained in simple functions or you will get Compiler Error C2712,
 *      "cannot use __try in functions that require object unwinding"
 *
 ******************************************************************************/

void P4BridgeServer::CallTextResultsCallbackFn(int cmdId, const char *data)
{
	string* pErrorString = NULL;
	__try
	{
		if ((cmdId > 0) && (pTextResultsCallbackFn != NULL))
		{
			(*pTextResultsCallbackFn)( cmdId, data );
		}
	}  
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		LOG_LOC();
		getConnection()->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );
		DELETE_OBJECT(pErrorString);
	}
#else
	catch (int e)
	{

	}
#endif


}

/*******************************************************************************
 *
 *  CallInfoResultsCallbackFn
 *
 *  Simple wrapper to call the callback function (if it has been set) within a
 *      SEH __try block to catch any platform exception. SEH __try blocks must
 *      be contained in simple functions or you will get Compiler Error C2712,
 *      "cannot use __try in functions that require object unwinding"
 *
 ******************************************************************************/

void P4BridgeServer::CallInfoResultsCallbackFn( int cmdId, int msgId, char level, const char *data )
{
	string* pErrorString = NULL;
	__try
	{
		if 	((cmdId > 0) && (pInfoResultsCallbackFn != NULL))
		{
			int nlevel = (int)(level - '0');
			(*pInfoResultsCallbackFn)( cmdId, msgId, nlevel, data );
		}
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		LOG_LOC();
		getConnection()->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );
		DELETE_OBJECT(pErrorString);
	}
#else
	catch (int e)
	{

	}
#endif
}

/*******************************************************************************
 *
 *  CallTaggedOutputCallbackFn
 *
 *  Simple wrapper to call the callback function (if it has been set) within a
 *      SEH __try block to catch any platform exception. SEH __try blocks must
 *      be contained in simple functions or you will get Compiler Error C2712,
 *      "cannot use __try in functions that require object unwinding"
 *
 ******************************************************************************/

void P4BridgeServer::CallTaggedOutputCallbackFn( int cmdId, int objId, const char *pKey, const char * pVal )
{
	string* pErrorString = NULL;
	__try
	{
		if ((cmdId > 0) && (pTaggedOutputCallbackFn != NULL))
		{
			(*pTaggedOutputCallbackFn)( cmdId, objId, pKey, pVal );
		}
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		LOG_LOC();
		getConnection(cmdId)->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );
		DELETE_OBJECT(pErrorString);
	}
#else
	catch (int e)
	{

	}
#endif
}

/*******************************************************************************
 *
 *  CallErrorCallbackFn
 *
 *  Simple wrapper to call the callback function (if it has been set) within a
 *      SEH __try block to catch any platform exception. SEH __try blocks must
 *      be contained in simple functions or you will get Compiler Error C2712,
 *      "cannot use __try in functions that require object unwinding"
 *
 ******************************************************************************/

void P4BridgeServer::CallErrorCallbackFn( int cmdId, int severity, int errorId, const char * errMsg )
{
	string* pErrorString = NULL;
	__try
	{
		if 	((cmdId > 0) && (pErrorCallbackFn != NULL))
		{
			(*pErrorCallbackFn)( cmdId, severity, errorId, errMsg );
		}
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		// could cause infinite recursion if we keep producing errors
		//  when reporting errors
		pErrorCallbackFn = NULL;
		LOG_LOC();
		getConnection()->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );
		DELETE_OBJECT(pErrorString);
	}
#else
	catch (int e)
	{

	}
#endif
}
/*******************************************************************************
 *
 *  CallErrorCallbackFn
 *
 *  Simple wrapper to call the callback function (if it has been set) within a
 *      SEH __try block to catch any platform exception. SEH __try blocks must
 *      be contained in simple functions or you will get Compiler Error C2712,
 *      "cannot use __try in functions that require object unwinding"
 *
 ******************************************************************************/

void P4BridgeServer::CallBinaryResultsCallbackFn( int cmdId, void * data, int length )
{
	string* pErrorString = NULL;
	__try
	{
		if ((cmdId > 0) && (pBinaryResultsCallbackFn))
		{
			(*pBinaryResultsCallbackFn)( cmdId, (void *) data, length );
		}
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		LOG_LOC();
		getConnection()->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );
		DELETE_OBJECT(pErrorString);
	}
#else
	catch (int e) 
	{

	}
#endif
}

// Set the call back function to receive the tagged output
void P4BridgeServer::SetTaggedOutputCallbackFn(IntTextTextCallbackFn* pNew)
{
	pTaggedOutputCallbackFn = pNew;
}

// Set the call back function to receive the error output
void P4BridgeServer::SetErrorCallbackFn(IntIntIntTextCallbackFn* pNew)
{
	pErrorCallbackFn = pNew;
}

void P4BridgeServer::Prompt( int cmdId, const StrPtr &msg, StrBuf &rsp,
			int noEcho, Error *e )
{
	string* pErrorString = NULL;

	__try
	{
		if ((cmdId > 0) && (pPromptCallbackFn))
		{
			char response[1024];

			(*pPromptCallbackFn)( cmdId, msg.Text(), response, sizeof(response), noEcho);

			rsp.Set(response);
		}
	}  
#ifdef _WIN32
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		LOG_LOC();
		getConnection()->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );

	}
#else
	catch (int e)
	{

	}
#endif
}

void P4BridgeServer::SetPromptCallbackFn( PromptCallbackFn * pNew)
{
	pPromptCallbackFn = pNew;
}

void P4BridgeServer::SetParallelTransferCallbackFn(ParallelTransferCallbackFn * pNew)
{
	pParallelTransferCallbackFn = pNew;
}

// Set the call back function to receive the information output
void P4BridgeServer::SetInfoResultsCallbackFn(IntIntIntTextCallbackFn* pNew)
{
	pInfoResultsCallbackFn = pNew;
}

// Set the call back function to receive the text output
void P4BridgeServer::SetTextResultsCallbackFn(TextCallbackFn* pNew)
{
	pTextResultsCallbackFn = pNew;
}

// Set the call back function to receive the binary output
void P4BridgeServer::SetBinaryResultsCallbackFn(BinaryCallbackFn* pNew)
{
	pBinaryResultsCallbackFn = pNew;
}

// Callbacks for handling interactive resolve
int	P4BridgeServer::Resolve( int cmdId, ClientMerge *m, Error *e )
{
	if (pResolveCallbackFn == NULL)
	{
		return CMS_SKIP;
	}
	P4ClientMerge *merger = new P4ClientMerge(m);
	int result = -1;

	result = Resolve_int( cmdId, merger );

	delete merger;

	if (result == -1)
	{
		LOG_LOC();
		return getConnection()->getUi()->ClientUser::Resolve( m, e );
	}
	return result;
}

int	P4BridgeServer::Resolve( int cmdId, ClientResolveA *r, int preview, Error *e )
{
	if (pResolveACallbackFn == NULL)
	{
		return CMS_SKIP;
	}

	P4ClientResolve *resolver = new P4ClientResolve(r, isUnicode);
	int result = -1;

	result = Resolve_int( cmdId, resolver, preview, e);

	delete resolver;

	if (result == -1)
	{
		return CMS_SKIP;
	}
	return result;
}

void P4BridgeServer::SetResolveCallbackFn(ResolveCallbackFn * pNew)
{
	pResolveCallbackFn = pNew;
}

void P4BridgeServer::SetResolveACallbackFn(ResolveACallbackFn * pNew)
{
	pResolveACallbackFn = pNew;
}

int P4BridgeServer::Resolve_int( int cmdId, P4ClientMerge *merger)
{
	int result = -1;
	string* pErrorString = NULL;
	__try
	{
		if ((cmdId > 0) && (pResolveCallbackFn != NULL))
		{
			result = (*pResolveCallbackFn)(cmdId, merger);
		}
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		LOG_LOC();
		getConnection()->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );
		DELETE_OBJECT(pErrorString);
	}
#else
	catch (int e)
	{

	}
#endif

	return result;
}

int P4BridgeServer::Resolve_int( int cmdId, P4ClientResolve *resolver, int preview, Error *e)
{
	int result = -1;
	string* pErrorString = NULL;

	__try
	{
		if ((cmdId > 0) && (pResolveACallbackFn != NULL))
		{
			result = (*pResolveACallbackFn)(cmdId, resolver, preview);
		}
	}
#ifdef _WIN32
	__except (HANDLE_EXCEPTION(&pErrorString))
	{
		LOG_LOC();
		getConnection()->getUi()->HandleError( E_FATAL, 0, pErrorString->c_str() );
		DELETE_OBJECT(pErrorString);
	}
#else
	catch (int e)
	{

	}
#endif


	return result;
}

int P4BridgeServer::IsIgnored_Int( const StrPtr &path )
{
	ClientApi client;
	Error e;
	client.SetCharset("utf8");
	Ignore* ignore = client.GetIgnore();
	const StrPtr ignoreFile = client.GetIgnoreFile();
	if(!ignore)
	{
		return 0;
	}

	return ignore->Reject( path, ignoreFile );
}

int P4BridgeServer::IsIgnored( const StrPtr &path )
{
	__try
	{
		return IsIgnored_Int(path);
	}
#ifdef _WIN32	
	__except (P4BridgeServer::sHandleException(GetExceptionCode(), GetExceptionInformation()))
	{
	}
#else
	catch (int e)
	{

	}
#endif
	return 0;
}

string P4BridgeServer::GetTicketFile()
{
	LOCK(&envLock);
    StrBuf ticketfile;
	char* c;
	HostEnv h;

	// ticketfile - where users login tickets are stashed
    if( c = _enviro.Get( "P4TICKETS" ) )
	{
		ticketfile.Set( c );
	}
	else
	{
		h.GetTicketFile( ticketfile, &_enviro );
	}
	return ticketfile.Text();
}

string P4BridgeServer::GetTicket(char* uri, char* user)
{
	LOCK(&envLock);
	StrBuf ticketfile;
	char* c;
	HostEnv h;

	// ticketfile - where users login tickets are stashed
    c = _enviro.Get( "P4TICKETS" );
	if (c)
	{
		ticketfile.Set( c );
	}
	else
	{
		h.GetTicketFile( ticketfile, &_enviro );
	}
    Ticket t(&ticketfile);
    StrBuf port(uri);
    StrBuf userStr(user);

	return t.GetTicket(port, userStr);
}

LogCallbackFn* P4BridgeServer::SetLogCallFn(LogCallbackFn *log_fn)
{
	LogCallbackFn* old = pLogFn;
	pLogFn = log_fn;
	return old;
}

int P4BridgeServer::DoTransferInternal(
	const char *cmd,
	std::vector<const char*> &argList,
	StrDictListIterator *varDictIterator,
	int threads,
	Error *e)
{
	LOG_ENTRY();
	__try
	{
		// TODO: Error* management, not clear if it's needed
		return pParallelTransferCallbackFn((int*)this, cmd, argList.data(), (int) argList.size(), (int*) varDictIterator, threads);
	}
#ifdef _WIN32
	__except (HANDLE_EXCEPTION_NOSTR())
	{
		LOG_ERROR("An exception occured handling a parallel operation");
		return 1;
	}
#else
	catch (int e)
	{
		LOG_ERROR("An exception occured handling a parallel operation");
		return 1;
	}
#endif
}

int P4BridgeServer::DoTransfer(
	ClientApi *client,
	ClientUser *ui,
	const char *cmd,
	StrArray &args,
	StrDict &pVars,
	int threads,
	Error *e)
{
	LOG_ENTRY();
	if (!pParallelTransferCallbackFn)
		return 1;	// we're not prepared to handle this

	// this function should not return until all of the threads have completed,
	// so local (stack) transforms of P4 objects to .Net objects is OK
	std::vector<const char*> argList;
	for (int i = 0; i < args.Count(); i++)
	{
		argList.push_back(args.Get(i)->Value());
	}

	// copy the pVars to a local StrDictList for the iterator
	StrDictList dList;
	dList.Data()->CopyVars(pVars);
	StrDictListIterator varDictIterator(&dList);

	return DoTransferInternal(cmd, argList, &varDictIterator, threads, e);
}

// Epic
void P4BridgeServer::SetConnectionHost(const char* hostname)
{
	if (pConnection)
	{
		pConnection->SetHost(hostname);
	}
}

// the transfer shim
ParallelTransfer::ParallelTransfer(P4BridgeServer* pServer) :
	p4base(Type()),
	pBridgeServer(pServer)
{
}

ParallelTransfer::~ParallelTransfer()
{
	LOG_LOC();
}

void ParallelTransfer::SetBridgeServer(P4BridgeServer* pServer)
{
	pBridgeServer = pServer;
}


int ParallelTransfer::Transfer(ClientApi* client,
	ClientUser *ui,
	const char *cmd,
	StrArray &args,
	StrDict &pVars,
	int threads,
	Error *e)
{
	LOG_ENTRY();
	// not bridge server to tell?  fail!
	return (!pBridgeServer) ? 1 : pBridgeServer->DoTransfer(client, ui, cmd, args, pVars, threads, e);
}
