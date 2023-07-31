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
 * Name		: p4bridge-api.cc
 *
 * Author	: dbb
 *
 * Description	: A "Flat C" interface for the Perforce API. Used to provide 
 * 		  simple access for C#.NET using P/Invoke and dllimport.
 *
 ******************************************************************************/

#include "stdafx.h"
#include "ticket.h"
#include "P4BridgeServer.h"
#ifdef _DEBUG_VLD
#include <vld.h> 
#endif

#ifndef _WIN32
#include <csignal>
#endif

// EPIC: This global variable is not thread safe and should really be associated with the server connection
// Don't have time to fix this properly, so getting commented out and will see "unknown error connecting" in the meantime

// If there is a connection error, keep it so the client can fetch it later
// P4ClientError * connectionError = NULL;

// #define STACKTRACE  // I don't know if this will work on a 32 bit build

#ifdef STACKTRACE
#include <WinBase.h>
#include <DbgHelp.h>

#define STACK_SIZE 20

void printStackTrace()
{
    char *result = "";
    unsigned int   i;
    void          *stack[STACK_SIZE];
    unsigned short frames;
    SYMBOL_INFO   *symbol;
    HANDLE         process;
    process = GetCurrentProcess();
    SymInitialize( process, NULL, TRUE );
    frames               = CaptureStackBackTrace( 0, STACK_SIZE, stack, NULL );
    symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
    symbol->MaxNameLen   = 255;
    symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
    for( i = 0; i < frames; i++ )
    {
        SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );
        char * symbol_name = "unknown symbol";
		if (strlen(symbol->Name) > 0)
		{
			symbol_name = symbol->Name;
		}

        P4BridgeServer::LogMessage(0, __FILE__,__LINE__,"%3d %s %I64x\n", frames - i -1, symbol_name, symbol->Address);
    }
    free( symbol );
}

#endif

#define HANDLE_EXCEPTION() HandleException(__FILE__, __LINE__, __FUNCTION__, GetExceptionCode(), GetExceptionInformation())

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
int HandleException(const char* fname, unsigned int line, const char* func, unsigned int c, struct _EXCEPTION_POINTERS *e)
{
	unsigned int code = c;
	struct _EXCEPTION_POINTERS *ep = e;

	// Log the exception
	char * exType = "Unknown";

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
	//case EXCEPTION_POSSIBLE_DEADLOCK:
	//    exType = "EXCEPTION_POSSIBLE_DEADLOCK\r\n");
	//    break;
	default:
		exType = "UNKNOWN EXCEPTION\r\n";
		break;
	}

	P4BridgeServer::LogMessage(0, fname, line, "Exception Detected (0x%08x) in bridge function %s: %s", code, func, exType);

#ifdef STACKTRACE
	printStackTrace();
#endif

	return EXCEPTION_EXECUTE_HANDLER;
} 
#else
#endif

extern "C"
{
	P4BRIDGE_API void ClearConnectionError()
	{
		__try
		{
			// EPIC:  commented out, see note at declaration
			// 
			// free old error string, if any.
			//if (connectionError)
			//{
			//	delete connectionError;
			//	connectionError = NULL;
			//}
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
		return;
	}
}

/******************************************************************************
 * Helper function to (re)connect to the server and determine if it is Unicode 
 * enabled.
******************************************************************************/
int ServerConnect(P4BridgeServer* pServer)
{
	__try
	{
		// free old error string, if any.
		ClearConnectionError();

		// EPIC: see note at global connectionError declaration
		P4ClientError* connectionError = NULL;

		if( !pServer->connected( &connectionError ) )
		{
			if (connectionError)
			{
				delete connectionError;
			}
			// Abort if the connect did not succeed
			return 0;
		}

		return 1;
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION())
	{
		return 0;
	}
#else
	// EPIC: to do handle exception
	catch (int e) {
		return 0;
	}	
#endif	
}

/******************************************************************************
 * Helper function to (re)connect to the server and determine if it is Unicode 
 * enabled.
******************************************************************************/
int ServerConnectTrust(P4BridgeServer* pServer, char* trust_flag, char* fingerprint)
{
	__try
	{
		// dereference old error string, if any. It's not 'our' string, so we can't
		//  free it.
		ClearConnectionError();

		// EPIC: see note at global connectionError declaration
		P4ClientError* connectionError = NULL;

		// Connect to the server and see if the api returns an error. 
		if( !pServer->connect_and_trust( &connectionError, trust_flag, fingerprint ) )
		{
			if (connectionError)
			{
				delete connectionError;
			}

			// Abort if the connect did not succeed
			return 0;
		}

	
		if ( pServer )
		{
			// Check if the server is Unicode enabled
			pServer->unicodeServer(  );
		}
		return 1;
	}
#ifdef _WIN32	
	__except (HANDLE_EXCEPTION())
	{
		return 0;
	}
#else
	// EPIC: to do handle exception
	catch (int e) {
		return 0;
	}	
#endif	
}

/******************************************************************************
 * 'Flat' C interface for the dll. This interface will be imported into C# 
 *    using P/Invoke 
******************************************************************************/
extern "C" 
{
	P4BRIDGE_API void SetLogFunction(
		LogCallbackFn *log_fn)
	{
		P4BridgeServer::SetLogCallFn(log_fn);
	}

	/**************************************************************************
	*
	* P4BridgeServer functions
	*
	*    These are the functions that use a P4BridgeServer* to access an object 
	*      created in the dll.
	*
	**************************************************************************/

	P4BridgeServer* Connect_Int(	const char *server, 
													const char *user, 
													const char *pass,
													const char *ws_client,
													LogCallbackFn *log_fn)
	{
		// EPIC: This is broken, we set a single static logging function (which happens to be thread safe)
		// LogCallbackFn *orig = P4BridgeServer::SetLogCallFn(log_fn);

		//Connect to the server
		P4BridgeServer* pServer = new P4BridgeServer(   server, 
														user, 
														pass,
														ws_client);

		// EPIC: see above comment
		// P4BridgeServer::SetLogCallFn(orig);

		if (ServerConnect( pServer ) )
		{
			return pServer;
		}

		delete pServer;
		return NULL;
	}

	/**************************************************************************
	*
	*  Connect: Connect to the a Perforce server and create a new 
	*    P4BridgeServer to access the server. 
	*
	*   Returns: Pointer to the new P4BridgeServer, NULL if there is an error.
	*
	*  NOTE: Call CloseConnection() on the returned pointer to free the object
	*
	**************************************************************************/

	P4BRIDGE_API P4BridgeServer* Connect(	const char *server, 
													const char *user, 
													const char *pass,
													const char *ws_client,
													LogCallbackFn *log_fn)
	{
		LOG_ENTRY();
		__try
		{
			// EPIC: see note at global connectionError declaration
			//connectionError = NULL;
			return Connect_Int(	server, user, pass, ws_client, log_fn);
		}
	#ifdef _WIN32	
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
	#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
	#endif		
	}

	P4BridgeServer* TrustedConnect_Int(	const char *server, 
															const char *user, 
															const char *pass,
															const char *ws_client,
															char *trust_flag,
															char *fingerprint,
															LogCallbackFn *log_fn)
	{
		//Connect to the server
		P4BridgeServer* pServer = new P4BridgeServer(   server, 
														user, 
														pass,
														ws_client);

		// EPIC, we use a static thread-safe logging callback 
		// LogCallbackFn* orig = P4BridgeServer::SetLogCallFn(log_fn); 
		
		bool ok = ServerConnectTrust( pServer, trust_flag, fingerprint ) != 0;
		
		// EPIC
		//P4BridgeServer::SetLogCallFn(orig);

		if (ok)
		{
			return pServer;
		}
		// delete pServer?
		delete pServer;
		return NULL;
	}

	/**************************************************************************
	*
	*  TrustedConnect: Connect to the a Perforce server and create a new 
	*    P4BridgeServer to access the server, and establish (or reestablish)
	*	 a trust relationship based on the servers certificate fingerprint. 
	*
	*   Returns: Pointer to the new P4BridgeServer, NULL if there is an error.
	*
	*  NOTE: Call CloseConnection() on the returned pointer to free the object
	*
	**************************************************************************/

	P4BRIDGE_API P4BridgeServer* TrustedConnect(	const char *server, 
															const char *user, 
															const char *pass,
															const char *ws_client,
															char *trust_flag,
															char *fingerprint,
															LogCallbackFn *log_fn)
	{
		__try
		{
			// EPIC: see note at global connectionError declaration
			//connectionError = NULL;
			return TrustedConnect_Int( server, user, pass, ws_client, trust_flag, fingerprint, log_fn);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BridgeServer* _connection_from_path(const char* cwd)
	{
		// create an un-connected p4bridgeserver based on a path
		// this is handy if you just want to get the connection info 
		// from a directory path
		ClientApi tempApi;
		tempApi.SetCwd(cwd);

		return new P4BridgeServer(
			tempApi.GetPort().Text(),
			tempApi.GetUser().Text(),
			tempApi.GetPassword().Text(),
			tempApi.GetClient().Text());
	}
	P4BRIDGE_API P4BridgeServer* ConnectionFromPath(const char* cwd)
	{
		__try
		{
			return _connection_from_path(cwd);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}
	/**************************************************************************
	*
	*  GetConnectionError: Returns the text of a the error message 
	*   generated by the connection attempt, if any.
	*
	**************************************************************************/

	P4BRIDGE_API P4ClientError * GetConnectionError( void )
	{
		__try
		{
			// EPIC: see note at global connectionError declaration
			return NULL;//connectionError;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  CloseConnection: Closes the connection and deletes the P4BridgeServer 
	*		object.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	**************************************************************************/
	P4BRIDGE_API int ReleaseConnection( P4BridgeServer* pServer )
	{
		LOG_ENTRY();
		if (!pServer) 
		{
			return 1;
		}
		__try
		{
			// if the handle is invalid or freeing it causes an exception, 
			// just consider it closed so return success
			if (!VALIDATE_HANDLE(pServer, tP4BridgeServer))
			{
				if (pServer) 
				{
					delete pServer;
				}
				return 1;
			}
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 1;
		}
#endif		

		__try
		{
			pServer->SetTaggedOutputCallbackFn(NULL);
			pServer->SetErrorCallbackFn(NULL);
			pServer->SetInfoResultsCallbackFn(NULL);
			pServer->SetTextResultsCallbackFn(NULL);
			pServer->SetBinaryResultsCallbackFn(NULL);
			pServer->SetPromptCallbackFn(NULL);
			pServer->SetParallelTransferCallbackFn(NULL);
			pServer->SetResolveCallbackFn(NULL);
			pServer->SetResolveACallbackFn(NULL);

			LOG_LOC();
			int ret = pServer->close_connection();
			delete pServer;
			return ret;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  Disconnect: Disconnect from the server after running one or more 
	*	commands.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	**************************************************************************/
	P4BRIDGE_API int Disconnect( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_B(pServer, tP4BridgeServer)
			return pServer->disconnect();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  IsUnicode: Check if the server supports Unicode 
	*
	*  Note: Is set during connection so is valid immediately after Connect()
	*    successfully completes.
	*
	**************************************************************************/

	P4BRIDGE_API int IsUnicode( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_B(pServer, tP4BridgeServer)
			return pServer->unicodeServer();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  APILevel: Get the API level supported by the server 
	*
	*  Note: Is set during connection so is valid immediately after Connect()
	*    successfully completes.
	*
	**************************************************************************/

	P4BRIDGE_API int APILevel( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_B(pServer, tP4BridgeServer)
			return pServer->APILevel();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  UseLogin: Check if the server requires the Login command be used 
	*
	*  Note: Is set during connection so is valid immediately after Connect()
	*    successfully completes.
	*
	**************************************************************************/

	P4BRIDGE_API int UseLogin( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_B(pServer, tP4BridgeServer)
			return pServer->UseLogin();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SupportsExtSubmit: Check if the server support extended submit options 
	*   (2006.2 higher)?  
	*
	*  Note: Is set during connection so is valid immediately after Connect()
	*    successfully completes.
	*
	**************************************************************************/

	P4BRIDGE_API int SupportsExtSubmit( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_B(pServer, tP4BridgeServer)
			return pServer->SupportsExtSubmit();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  UrlLaunched: Check if the ClientUser::HandleUrl method has been
	*   called
	*
	*  Note: P4BridgeClient::HandleUrl is the override that sets a bool
	*   and calls ClientUser::HandleUrl.
	*
	**************************************************************************/
	
	P4BRIDGE_API int UrlLaunched()
	{
		return handleUrl;
	}

	/**************************************************************************
	*
	*  IsUnicode: Check if the server supports Unicode 
	*
	*    pServer:      Pointer to the P4BridgeServer 
	*
	*    pCharSet:     String name for the character set to use for command 
	*                    data passed to/from the server.
	*
	*    pFileCharSet: String name for the character set to use for the 
	*                    contents of type Unicode file when stored in the 
	*                    a file on the client's disk.
	*
	*  Note: Needs to be called before any command which takes parameters is 
	*    called.
	*
	**************************************************************************/
	
	const char* _SetCharacterSet(P4BridgeServer* pServer,
		const char * pCharSet,
		const char * pFileCharSet)
	{
		return Utils::AllocString(pServer->set_charset(pCharSet, pFileCharSet));
	}

	P4BRIDGE_API const char * SetCharacterSet(   P4BridgeServer* pServer, 
													const char * pCharSet, 
													const char * pFileCharSet )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
 			return _SetCharacterSet(pServer, pCharSet, pFileCharSet);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  set_connection: Set the connection parameters.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*    newPort:		New port
	*    newUser:		New workspace
	*    newPassword:	New password
	*    newClient:		New workspace
	*
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void set_connection(P4BridgeServer* pServer,
		const char* newPort,
		const char* newUser,
		const char* newPassword,
		const char* newClient)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			return pServer->set_connection(newPort, newUser, newPassword, newClient);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  set_client: Set the client workspace.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*    pNew: New workspace
	*
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void set_client(P4BridgeServer* pServer, const char* workspace)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			return pServer->set_client(workspace);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_client: Get the name of the current client workspace.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char* _get_client(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_client());
	}

	P4BRIDGE_API const char * get_client(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_client(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_user: Get the user name for the current connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*  Return: Pointer access the data.
	*
	**************************************************************************/

	const char* _get_user(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_user());
	}

	P4BRIDGE_API const char * get_user(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_user(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  set_user: Set the user name for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*    newValue: The new value
	*
	*  Return: Pointer access the data.
	*
	**************************************************************************/

	P4BRIDGE_API void set_user(P4BridgeServer* pServer, char * newValue)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
				pServer->set_user(newValue);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_port: Get the port for the current connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char* _get_port(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_port());
	}

	P4BRIDGE_API const char * get_port(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_port(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  set_port: Set the port for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*    newValue: The new value
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API void set_port(P4BridgeServer* pServer, char * newValue)

	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
				pServer->set_port(newValue);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_password: Get the password for the current connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char* _get_password(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_password());
	}

	P4BRIDGE_API const char * get_password(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_password(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  set_password: Set the password for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*    newValue: The new value
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API void set_password(P4BridgeServer* pServer, char * newValue)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
				pServer->set_password(newValue);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
    *
    *  set_ticketFile: Set the ticket file.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*    ticketFile: New ticket file
	*
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void set_ticketFile(P4BridgeServer* pServer, const char* ticketFile)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
				return pServer->set_ticketFile(ticketFile);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_ticketFile: Get the name of the current ticket file.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char* _get_TicketFile(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_ticketFile());
	}

	P4BRIDGE_API const char * get_ticketFile(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_TicketFile(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
*
*  get_ticket: Get the ticket for the current connection, if any.
*
*    pServer: Pointer to the P4BridgeServer
*
*  Return: Pointer to access the data.
*
**************************************************************************/

	const char* _get_ticket(char* path, char* port, char* user)
	{
		LOG_ENTRY();
		StrPtr pathStr = StrRef(path);
		Ticket ticket(&pathStr);
		StrPtr portStr = StrRef(port);
		StrPtr userStr = StrRef(user);

		return Utils::AllocString(ticket.GetTicket(portStr, userStr));
	}

	P4BRIDGE_API const char * get_ticket(char* path, char* port, char* user)
	{
		LOG_ENTRY();
		__try
		{
			return _get_ticket(path, port, user);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	const char* get_cwd_int(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_cwd());
	}

	/**************************************************************************
	*
	*  GetCwd: Gets the current working directory for the P4BridgeServer.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	**************************************************************************/
	P4BRIDGE_API const char * get_cwd(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return get_cwd_int(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetCwd: Sets the current working directory for the P4BridgeServer.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*    new_val: Path to the new current working directory
	*
	**************************************************************************/

	P4BRIDGE_API void set_cwd(P4BridgeServer* pServer,
		const char * new_val)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->set_cwd((const char *)new_val);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_programName: Get the program name to use for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char* _get_programName(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_programName());
	}

	P4BRIDGE_API const char * get_programName(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_programName(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  set_programName: Set the program name to use for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*    newValue: The new value
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API void set_programName(P4BridgeServer* pServer, char * newValue)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
				pServer->set_programName(newValue);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_programVer: Get the program version to use for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char * _get_programVer(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_programVer());
	}

	P4BRIDGE_API const char * get_programVer( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_programVer(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  set_programVer: Set the program version to use for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    newValue: The new value
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API void set_programVer( P4BridgeServer* pServer, char * newValue )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->set_programVer(newValue);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_charset: Get the character to use for the connection.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char * _get_charset(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_charset());
	}

	P4BRIDGE_API const char * get_charset( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_charset(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  get_config: Get the config file for the current connection, if any.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	const char * _get_config(P4BridgeServer* pServer)
	{
		return Utils::AllocString(pServer->get_config());
	}

	P4BRIDGE_API const char * get_config( P4BridgeServer* pServer )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			return _get_config(pServer);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	const char* _get_config_cwd(char* cwd)
	{
		LOG_ENTRY();
		return Utils::AllocString(P4BridgeServer::get_config(cwd));
	}

	P4BRIDGE_API const char * get_config_cwd( char* cwd )
	{
		LOG_ENTRY();
		__try
		{
			return _get_config_cwd(cwd);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	const char* _Get(const char *var)
	{
		return Utils::AllocString(P4BridgeServer::Get(var));
	}

	P4BRIDGE_API const char* Get( const char *var )
	{
		__try
		{
			return _Get( var );
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API void Set( const char *var, const char *val )
	{
		__try
		{
			return P4BridgeServer::Set( var, val );
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	P4BRIDGE_API void Update( const char *var, const char *val )
	{
		__try
		{
			return P4BridgeServer::Update( var, val );
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	P4BRIDGE_API void ReloadEnviro()
	{
		__try
		{
			return P4BridgeServer::Reload();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetTicketFile: Get the path to the file where user tickets are stored.
	*
	*  Return: Path to the ticket file, NULL if not known or error
	**************************************************************************/
	const char* _GetTicketFile()
	{
		return Utils::AllocString(P4BridgeServer::GetTicketFile());
	}

	P4BRIDGE_API const char* GetTicketFile(  )
	{
		__try
		{
			return _GetTicketFile();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetExistingTicket: Get the existing ticket for a connection, if any.
	*
	*  Return: The ticket, NULL if no ticket in file or error
	**************************************************************************/
	const char* _GetTicket(char* port, char* user)
	{
		return Utils::AllocString(P4BridgeServer::GetTicket(port, user));
	}

	P4BRIDGE_API const char* GetTicket( char* port, char* user )
	{
		__try
		{
			return _GetTicket(port, user);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/*
		Raw SetProtocol - must be called on a disconnected pServer to be effective, or on a pServer that you reconnect on
	*/
	P4BRIDGE_API void SetProtocol(P4BridgeServer* pServer, const char* key, const char* val)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer);
			pServer->SetProtocol(key, val);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  RunCommand: Run a command using the P4BridgeServer.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    cmd: Command name, i.e 'fstst'. These are always in ASCII, regardless
	*           of whether or not the server is Unicode enabled.
	*
	*    tagged: If non zero, run the command using tagged protocol 
	*
	*    args: list of arguments. For non Unicode servers, these are ASCII
	*            encode strings. For Unicode servers they should be encoded in
	*            using the encoding specified in a previous call to 
	*            SetCharacterSet().
	*
	*    argc: count of arguments
	*
	*  Return: Zero if there was an error running the command
	**************************************************************************/

	P4BRIDGE_API int RunCommand( P4BridgeServer* pServer, 
										  const char *cmd, 
										  int cmdId,
										  int tagged, 
										  char **args, 
										  int argc )
	{
		__try
		{
			VALIDATE_HANDLE_I(pServer, tP4BridgeServer)
			// make sure we're connected to the server
			if (0 == ServerConnect( pServer ))
			{
				return 0;
			}
			LOG_DEBUG2(4, "Running command [%d] %s", cmdId, cmd);
			return pServer->run_command(cmd, cmdId, tagged, args, argc);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  CancelCommand: Cancel a running command
	*
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void CancelCommand( P4BridgeServer* pServer, int cmdId ) 
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->cancel_command(cmdId);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	P4BRIDGE_API int IsConnected(P4BridgeServer* pServer)
	{
		__try
		{
			VALIDATE_HANDLE_I(pServer, tP4BridgeServer)
			return pServer->IsConnected();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetTaggedOutputCallbackFn: Set the tagged output callback fn.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: New function pointer 
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetTaggedOutputCallbackFn( P4BridgeServer* pServer, IntTextTextCallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetTaggedOutputCallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetTaggedOutputCount: Get a count of the number of entries in the tagged 
	*							output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: count of the number of entries in the tagged output..
	*
	*  NOTE: Call Release() on the returned pointer to free the object
	*
	**************************************************************************/

	P4BRIDGE_API int GetTaggedOutputCount( P4BridgeServer* pServer, int cmdId )
	{
		__try
		{
			VALIDATE_HANDLE_I(pServer, tP4BridgeServer);
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  -1;
			return pUi->GetTaggedOutputCount();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetTaggedOutput: Get a StrDictListIterator to iterate through
	*                            the tagged output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to a new StrDictListIterator to access the data.
	*
	*  NOTE: Call Release() on the returned pointer to free the object
	*
	**************************************************************************/

	P4BRIDGE_API StrDictListIterator * GetTaggedOutput( P4BridgeServer* pServer, int cmdId )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  NULL;
			return pUi->GetTaggedOutput();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetErrorCallbackFn: Set the error output callback fn.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: New function pointer 
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetErrorCallbackFn( P4BridgeServer* pServer, IntIntIntTextCallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetErrorCallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetErrorResults: Get the error output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to the data.
	*
	**************************************************************************/

	P4BRIDGE_API P4ClientError * GetErrorResults( P4BridgeServer * pServer, int cmdId)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  NULL;
			return pUi->GetErrorResults();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetInfoResultsCallbackFn: Set the info output callback fn.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: New function pointer 
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetInfoResultsCallbackFn( P4BridgeServer* pServer, IntIntIntTextCallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetInfoResultsCallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetInfoResultsCount: Get the count of the number of the info output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Count of number of entries in the info out.
	*
	**************************************************************************/

	P4BRIDGE_API int GetInfoResultsCount( P4BridgeServer* pServer, int cmdId)
	{
		__try
		{
			VALIDATE_HANDLE_B(pServer, tP4BridgeServer)
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  -1;
			return pUi->GetInfoResultsCount();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetInfoResults: Get the info output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API P4ClientInfoMsg * GetInfoResults( P4BridgeServer* pServer, int cmdId)
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, (int) tP4BridgeServer)
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  NULL;
			if (!pUi->GetInfoResults())
				return  NULL;
			return pUi->GetInfoResults();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetTextResultsCallbackFn: Set the text output callback fn.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: New function pointer 
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetTextResultsCallbackFn( P4BridgeServer* pServer, TextCallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetTextResultsCallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{			
		}
#else
		catch (int e)
		{
			// EPIC: handle exception			
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetTextResults: Get the text output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to the data.
	*
	**************************************************************************/

	P4BRIDGE_API const char * GetTextResults( P4BridgeServer* pServer, int cmdId )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer)
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  NULL;
			if (!pUi->GetTextResults())
				return  NULL;
			return pUi->GetTextResults();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetBinaryResultsCallbackFn: Set the callback for binary output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: The new callback function pointer
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetBinaryResultsCallbackFn( P4BridgeServer* pServer, BinaryCallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetBinaryResultsCallbackFn(pNew);
		}
		#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		

	}

	/**************************************************************************
	*
	*  GetBinaryResultsCount: Get the size in bytes of the binary output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Byte count for the data.
	*
	**************************************************************************/

	P4BRIDGE_API size_t GetBinaryResultsCount(  P4BridgeServer* pServer, int cmdId) 
	{ 
		__try
		{
			VALIDATE_HANDLE_I(pServer, tP4BridgeServer)
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  0;
			return pUi->GetBinaryResultsCount( );
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetBinaryResults: Get the binary output.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to the data.
	*
	**************************************************************************/

	P4BRIDGE_API const unsigned char* GetBinaryResults( P4BridgeServer* pServer, int cmdId )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  NULL;
			return pUi->GetBinaryResults( );
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetDataSet: Set the Data Set in the UI (P4Client).
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    data: String Pointer to the data
	*    
	*  Return: Pointer to char * for the data.
	*
	**************************************************************************/

	P4BRIDGE_API void SetDataSet( P4BridgeServer* pServer, int cmdId,
										   const char * data )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			P4BridgeClient* pUi = pServer->get_ui(cmdId);
			if (!pUi)
				return;
			return pUi->SetDataSet(data);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetDataSet: Get the Data Set in the UI (P4Client).
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to a new char * to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API  char * GetDataSet( P4BridgeServer* pServer, int cmdId )
	{
		__try
		{
			VALIDATE_HANDLE_P(pServer, tP4BridgeServer);
			P4BridgeClient* pUi = pServer->find_ui(cmdId);
			if (!pUi)
				return  NULL;
			return pUi->GetDataSet()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetPromptCallbackFn: Set the callback for replying to a server prompt.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: The new callback function pointer
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetPromptCallbackFn( P4BridgeServer* pServer, 
													PromptCallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetPromptCallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetParallelTransferCallbackFn: Set the callback for replying to a server prompt.
	*
	*    pServer: Pointer to the P4BridgeServer
	*
	*    pNew: The new callback function pointer
	*
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetParallelTransferCallbackFn(P4BridgeServer* pServer,
		ParallelTransferCallbackFn* pNew)
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetParallelTransferCallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	*
	*  IsIgnored: Test to see if a particular file is ignored.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: local path of the file
	*    
	*  Return: non zero if file is ignored
	**************************************************************************/

	P4BRIDGE_API int IsIgnored( const char *pPath )
	{
		__try
		{
			StrPtr Str = StrRef(pPath);
			return P4BridgeServer::IsIgnored(Str);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
		return 0;
	}

	/**************************************************************************
	*   class StrDictListIterator
	* 
	* A StrDictList is a list of items defined by StrDictionaries. Each
	*   StrDictionary can be considered a collection of entries, key:value 
	*   pairs of string data. StrDictListIterator allows you to walk this
	*   list of lists.
	*
	* itemList---->item1----->item2-....->itemN
	*              ->entry1   ->entry1    ->entry1
	*              ->entry2   ->entry2    ->entry2
	*                ...        ...         ...              
	*              ->entryX   ->entryY    ->entryZ
	*
	* Basic Usage:
	*   StrDictListIterator * pItem;
	*   while (pItem = pIterator-GetNextItem()
	*   {
	*       KeyValuePair * = pEntry;
	*       while (pEntry = pIterator-GetNextEntry()
	*          // do something with the key:value pair, pEntry
	*   }
	*
	*  NOTE: The iterate as currently implemented, can only iterate through the
	*    data once, as there is no method to rest it.
	*
	**************************************************************************/

	/**************************************************************************
	*
	*  GetNextEntry: Get the next Item in the list. Returns the first item
	*      on the first call.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API StrDictList* GetNextItem( StrDictListIterator* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tStrDictListIterator)
			return pObj->GetNextItem();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetNextEntry: Get the next Entry for the current item. Returns the first 
	*      entry for the item on the first call to.
	*
	*    pObj: Pointer to the iterator. 
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API KeyValuePair * GetNextEntry( StrDictListIterator* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tStrDictListIterator)
			return pObj->GetNextEntry();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  Release: Delete an object allocated in the bridge's heap.
	*
	*    pObj: Pointer to the iterator. 
	*    
	*  Return: None.
	*
	**************************************************************************/

	P4BRIDGE_API void Release( void* pObj )
	{
		__try
		{
			// make sure to cast to a p4base object first, otherwise
			// the destructor will not be called and 'bad things will happen'
			p4base* pBase = static_cast<p4base*>(pObj);
			delete pBase;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
		}
#endif		
	}

	/**************************************************************************
	*
	*  Release: Delete an array allocated in the bridge's heap.
	*
	*    pObj: Pointer to the iterator. 
	*    
	*  Return: None.
	*
	**************************************************************************/

	P4BRIDGE_API void ReleaseString( void* pObj )
	{
		__try
		{
			Utils::ReleaseString(pObj);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}

	/**************************************************************************
	* class KeyValuePair
	**************************************************************************/
	
	/**************************************************************************
	*
	*  GetKey: Get the key.
	*
	*    pObj: Pointer to the KeyValuePair. 
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API const char * GetKey( KeyValuePair* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tKeyValuePair)
			return pObj->key.c_str();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}
	
	/**************************************************************************
	*
	*  GetValue: Get the value.
	*
	*    pObj: Pointer to the KeyValuePair. 
	*    
	*  Return: Pointer to access the data.
	*
	**************************************************************************/

	P4BRIDGE_API const char *  GetValue( KeyValuePair* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tKeyValuePair)
			return pObj->value.c_str();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	 *  P4ClientError
	 *************************************************************************/

	/**************************************************************************
	*
	*  GetSeverity: Get the severity.
	*
	*    pObj: Pointer to the P4ClientError. 
	*    
	*  Return: Severity of the Error.
	*
	**************************************************************************/

	P4BRIDGE_API const int Severity( P4ClientError* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientError)
			return pObj->Severity;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  ErrorCode: Get the Error Code.
	*
	*    pObj: Pointer to the P4ClientError. 
	*    
	*  Return: Unique ErrorCode of the Error.
	*
	**************************************************************************/

	P4BRIDGE_API const int ErrorCode( P4ClientError* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientError)
			return pObj->ErrorCode;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetMessage: Get the error message.
	*
	*    pObj: Pointer to the P4ClientError. 
	*    
	*  Return: Error Message.
	*
	**************************************************************************/

	P4BRIDGE_API const char * Message( P4ClientError* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientError)
			return pObj->Message.c_str();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}
	
	/**************************************************************************
	*
	*  GetNext: Get the next error message.
	*
	*    pObj: Pointer to the P4ClientError. 
	*    
	*  Return: Pointer to the next error message.
	*
	**************************************************************************/

	P4BRIDGE_API P4ClientError * Next( P4ClientError * pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientError)
			return pObj->Next;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}


	/**************************************************************************
	 *  P4ClientInfoMsg
	 *************************************************************************/

	/**************************************************************************
	*
	*  GetLevel: Get the message level.
	*
	*    pObj: Pointer to the P4ClientInfoMsg. 
	*    
	*  Return: Message level char from 0->9.
	*
	**************************************************************************/

	P4BRIDGE_API const char MessageLevel( P4ClientInfoMsg* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_C(pObj, tP4ClientInfoMsg)
			return pObj->Level;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  ErrorCode: Get the Message Code.
	*
	*    pObj: Pointer to the P4ClientInfoMsg. 
	*    
	*  Return: Unique Code of the Message.
	*
	**************************************************************************/

	P4BRIDGE_API const int InfoMsgCode( P4ClientInfoMsg* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientInfoMsg)
			return pObj->MsgCode;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	/**************************************************************************
	*
	*  GetMessage: Get the info message.
	*
	*    pObj: Pointer to the P4ClientInfoMsg. 
	*    
	*  Return: Error Message.
	*
	**************************************************************************/

	P4BRIDGE_API const char * InfoMessage( P4ClientInfoMsg* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientInfoMsg)
			return pObj->Message.c_str();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}
	
	/**************************************************************************
	*
	*  GetNext: Get the next message.
	*
	*    pObj: Pointer to the P4ClientInfoMsg. 
	*    
	*  Return: Pointer to the next message.
	*
	**************************************************************************/

	P4BRIDGE_API P4ClientInfoMsg * NextInfoMsg( P4ClientInfoMsg * pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientInfoMsg)
			return pObj->Next;
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	 *
	 *  P4ClientMerge
	 *
	 *  This simple class is a ClientMerge object.
	 *
	 *************************************************************************/

	P4BRIDGE_API int CM_AutoResolve( P4ClientMerge* pObj, MergeForce forceMerge )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return (int) pObj->AutoResolve(forceMerge);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API int CM_Resolve( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return (int) pObj->Resolve();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API int CM_DetectResolve( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return (int) pObj->DetectResolve();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API int CM_IsAcceptable( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return pObj->IsAcceptable();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API char *CM_GetBaseFile( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientMerge)
			if (!pObj->GetBaseFile())
				return NULL;
			return pObj->GetBaseFile()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CM_GetYourFile( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientMerge)
			if (!pObj->GetYourFile())
				return NULL;
			return pObj->GetYourFile()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CM_GetTheirFile( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientMerge)
			if (!pObj->GetTheirFile())
				return NULL;
			return pObj->GetTheirFile()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CM_GetResultFile( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientMerge)
			if (!pObj->GetResultFile())
				return NULL;
			return pObj->GetResultFile()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}


	P4BRIDGE_API int	CM_GetYourChunks( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return pObj->GetYourChunks();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API int	CM_GetTheirChunks( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return pObj->GetTheirChunks();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API int	CM_GetBothChunks( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return pObj->GetBothChunks();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API int	CM_GetConflictChunks( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientMerge);
			return pObj->GetConflictChunks();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return -1;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return -1;
		}
#endif		
	}

	P4BRIDGE_API char *CM_GetMergeDigest( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientMerge)
			if (!pObj->GetBaseFile())
				return NULL;
			return pObj->GetMergeDigest()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CM_GetYourDigest( P4ClientMerge* pObj )
	{
		__try
		{
			if (!pObj->GetBaseFile())
				return NULL;
			return pObj->GetYourDigest()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CM_GetTheirDigest( P4ClientMerge* pObj )
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientMerge)
			if (!pObj->GetBaseFile())
				return NULL;
			return pObj->GetTheirDigest()->Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API P4ClientError *CM_GetLastClientMergeError(P4ClientMerge* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientMerge)
			return pObj->GetLastError();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

/*******************************************************************************
 *
 *  P4ClientResolve
 *
 *  This simple class is a wrapper for ClientResolve object.
 *
 ******************************************************************************/

	P4BRIDGE_API int CR_AutoResolve( P4ClientResolve* pObj, MergeForce force )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientResolve);
			return (int) pObj->AutoResolve(force);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	P4BRIDGE_API int CR_Resolve( P4ClientResolve* pObj, int preview, Error *e )
	{
		__try
		{
			VALIDATE_HANDLE_I(pObj, tP4ClientResolve);
			return (int) pObj->Resolve(preview);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return 0;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return 0;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetType(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve);
			return pObj->GetType().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetMergeAction(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetMergeAction().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetYoursAction(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetYoursAction().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetTheirAction(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetTheirAction().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	// For the CLI interface, probably not of interest to others

	P4BRIDGE_API char *CR_GetMergePrompt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetMergePrompt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetYoursPrompt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetYoursPrompt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetTheirPrompt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetTheirPrompt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetMergeOpt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetMergeOpt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetYoursOpt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetYoursOpt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetTheirOpt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetTheirOpt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetSkipOpt(P4ClientResolve* pObj) 
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetSkipOpt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetHelpOpt(P4ClientResolve* pObj) 
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetHelpOpt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetAutoOpt(P4ClientResolve* pObj) 
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetAutoOpt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetPrompt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetPrompt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetTypePrompt(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetTypePrompt().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetUsageError(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetUsageError().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	P4BRIDGE_API char *CR_GetHelp(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetHelp().Text();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}
	
	P4BRIDGE_API P4ClientError *CR_GetLastError(P4ClientResolve* pObj)
	{
		__try
		{
			VALIDATE_HANDLE_P(pObj, tP4ClientResolve)
			return pObj->GetLastError();
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			return NULL;
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			return NULL;
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetResolveCallbackFn: Set the callback for replying to a resolve 
	*		callback.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: The new callback function pointer
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetResolveCallbackFn(	P4BridgeServer* pServer, 
														ResolveCallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetResolveCallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
			
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
			
		}
#endif		
	}

	/**************************************************************************
	*
	*  SetResolveACallbackFn: Set the callback for replying to a resolve 
	*		callback.
	*
	*    pServer: Pointer to the P4BridgeServer 
	*
	*    pNew: The new callback function pointer
	*    
	*  Return: None
	**************************************************************************/

	P4BRIDGE_API void SetResolveACallbackFn(	P4BridgeServer* pServer, 
														ResolveACallbackFn* pNew )
	{
		__try
		{
			VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetResolveACallbackFn(pNew);
		}
#ifdef _WIN32		
		__except (HANDLE_EXCEPTION())
		{
		}
#else
		catch (int e)
		{
			// EPIC: handle exception
		}
#endif		
	}




#if defined(_DEBUG)
	P4BRIDGE_API int GetAllocObjCount()		{ return p4typesCount;  }
	P4BRIDGE_API int GetAllocObj(int type)		{ return p4base::GetItemCount(type); }
	P4BRIDGE_API const char* GetAllocObjName(int type) {	return p4base::GetTypeStr(type);	}
	P4BRIDGE_API long GetStringAllocs()		{ return Utils::AllocCount(); }
	P4BRIDGE_API long GetStringReleases()		{ return Utils::FreeCount(); }
#else
	P4BRIDGE_API int GetAllocObjCount()		{ return 0; }
	P4BRIDGE_API int GetAllocObj(int type)		{ return 0; }
	P4BRIDGE_API const char* GetAllocObjName(int type) { return "only available in _DEBUG builds"; }
	P4BRIDGE_API long GetStringAllocs()		{ return 0; }
	P4BRIDGE_API long GetStringReleases()		{ return 0; }
#endif


// EPIC
P4BRIDGE_API void SetConnectionHost(P4BridgeServer* pServer, const char* hostname)
{
	__try
	{
		VALIDATE_HANDLE_V(pServer, tP4BridgeServer)
			pServer->SetConnectionHost(hostname);

	}
#ifdef _WIN32		
	__except (HANDLE_EXCEPTION())
	{

	}
#else
	catch (int e)
	{
		// EPIC: handle exception

	}
#endif		
}

P4BRIDGE_API void DebugCrash()
{
#ifdef _WIN32
	* ((int*)NULL) = 123;
	exit(3);
#else
	* ((int*)NULL) = 123;  
	raise(SIGABRT); 
#endif
}

}

