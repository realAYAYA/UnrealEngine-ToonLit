#include "StdAfx.h"
#include "ClientManager.h"
#include "P4BridgeClient.h"

extern CRITICAL_SECTION CriticalSection; 
void MyEnterCriticalSection(CRITICAL_SECTION *CriticalSection);

ClientManager::ClientManager(void)
{
	static P4BridgeClient* pFirstItem;
	static P4BridgeClient* pLastItem;

	defaultUI = NULL;
	ExceptionError = NULL;
	pTextResultsCallbackFn = NULL;
	pInfoResultsCallbackFn = NULL;
	pTaggedOutputCallbackFn = NULL;
	pErrorCallbackFn = NULL;
	pBinaryResultsCallbackFn = NULL;
	pPromptCallbackFn = NULL;
	pResolveCallbackFn = NULL;
	pResolveACallbackFn = NULL;
}

ClientManager::~ClientManager(void)
{
	if (disposed != 0)
	{
		return;
	}
	// defaultUI is deleted when the uderlying list is deleted
	//if (defaultUI != NULL)
	//{
	//	delete defaultUI;
	//	defaultUI = NULL;
	//}
}

P4BridgeClient* ClientManager::CreateNewUI(int cmdId)
{
	MyEnterCriticalSection(&CriticalSection); 
	P4BridgeClient* newClient = new P4BridgeClient(this, cmdId);
	LeaveCriticalSection(&CriticalSection);
	return newClient;
}

P4BridgeClient* ClientManager::GetUI(int cmdId)
{
	MyEnterCriticalSection(&CriticalSection); 
	P4BridgeClient* ui = (P4BridgeClient*) Find(cmdId);
	if (ui == NULL)
	{
		ui = CreateNewUI(cmdId);
	}
	LeaveCriticalSection(&CriticalSection);
	return ui;
}

P4BridgeClient* ClientManager::GetDefaultUI()
{
	MyEnterCriticalSection(&CriticalSection); 
	if (defaultUI == NULL)
	{
		defaultUI = new P4BridgeClient(this,0);
	}
	LeaveCriticalSection(&CriticalSection);
	return defaultUI;
}

void ClientManager::ReleaseUI(int cmdId)
{
	Remove(cmdId);
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

int ClientManager::HandleException(unsigned int c, struct _EXCEPTION_POINTERS *e)
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
		printf("UNKOWN EXCEPTION\r\n");
		break;
	}
	if (ExceptionError != NULL)
		delete ExceptionError;
		
	ExceptionError = new StrBuf();

	ExceptionError->Append("Exception Detected in callback function: ");
	ExceptionError->Append(exType);

	return EXCEPTION_EXECUTE_HANDLER;
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

void ClientManager::CallTextResultsCallbackFn(int cmdId, const char *data)
{
	__try
	{
		if (pTextResultsCallbackFn != NULL)
		{
			(*pTextResultsCallbackFn)( cmdId, data );
		}
	}  __except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		this->GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
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

void ClientManager::CallInfoResultsCallbackFn( int cmdId, char level, const char *data )
{
	__try
	{
		if 	(pInfoResultsCallbackFn != NULL)
		{
			int nlevel = (int)(level - '0');
			(*pInfoResultsCallbackFn)( cmdId, nlevel, data );
		}
	}
	__except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
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

void ClientManager::CallTaggedOutputCallbackFn( int cmdId, int objId, const char *pKey, const char * pVal )
{
	__try
	{
		if (pTaggedOutputCallbackFn != NULL)
		{
			(*pTaggedOutputCallbackFn)( cmdId, objId, pKey, pVal );
		}
	}
	__except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
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

void ClientManager::CallErrorCallbackFn( int cmdId, int severity, int errorId, const char * errMsg )
{
	__try
	{
		if 	(pErrorCallbackFn != NULL)
		{
			(*pErrorCallbackFn)( cmdId, severity, errorId, errMsg );
		}
	}
	__except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		// could cause infinite recursion if we keep producing errors 
		//  when reporting errors
		pErrorCallbackFn = NULL;
		GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
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

void ClientManager::CallBinaryResultsCallbackFn( int cmdId, void * data, int length )
{
	__try
	{
		if (pBinaryResultsCallbackFn)
			(*pBinaryResultsCallbackFn)( cmdId, (void *) data, length );
	}
	__except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
}
// Set the call back function to receive the tagged output
void ClientManager::SetTaggedOutputCallbackFn(IntTextTextCallbackFn* pNew)
{
	pTaggedOutputCallbackFn = pNew;
}

// Set the call back function to receive the error output
void ClientManager::SetErrorCallbackFn(IntIntIntTextCallbackFn* pNew)
{
	pErrorCallbackFn = pNew;
}

void ClientManager::Prompt( int cmdId, const StrPtr &msg, StrBuf &rsp, 
			int noEcho, Error *e )
{
	__try
	{
		if (pPromptCallbackFn)
		{
			char response[1024];

			(*pPromptCallbackFn)( cmdId, msg.Text(), response, sizeof(response), noEcho);

			rsp.Set(response);
		}
	}  __except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
}

void ClientManager::SetPromptCallbackFn( PromptCallbackFn * pNew)
{
	pPromptCallbackFn = pNew;
}

// Set the call back function to receive the information output
void ClientManager::SetInfoResultsCallbackFn(IntIntTextCallbackFn* pNew)
{
	pInfoResultsCallbackFn = pNew;
}

// Set the call back function to receive the text output
void ClientManager::SetTextResultsCallbackFn(TextCallbackFn* pNew)
{
	pTextResultsCallbackFn = pNew;
}

// Set the call back function to receive the binary output
void ClientManager::SetBinaryResultsCallbackFn(BinaryCallbackFn* pNew)
{
	pBinaryResultsCallbackFn = pNew;
}

// Callbacks for handling interactive resolve
int	ClientManager::Resolve( int cmdId, ClientMerge *m, Error *e )
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
		return GetUI(cmdId)->ClientUser::Resolve( m, e );
	}
	return result;
}

int	ClientManager::Resolve( int cmdId, ClientResolveA *r, int preview, Error *e )
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

void ClientManager::SetResolveCallbackFn(ResolveCallbackFn * pNew)
{
	pResolveCallbackFn = pNew;
}

void ClientManager::SetResolveACallbackFn(ResolveACallbackFn * pNew)
{
	pResolveACallbackFn = pNew;
}

int ClientManager::Resolve_int( int cmdId, P4ClientMerge *merger)
{
	int result = -1;
	__try
	{
		 result = (*pResolveCallbackFn)(cmdId, merger);
	}  
	__except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
	return result;
}

int ClientManager::Resolve_int( int cmdId, P4ClientResolve *resolver, int preview, Error *e)
{
	int result = -1;
	__try
	{
		result = (*pResolveACallbackFn)(cmdId, resolver, preview);
	}  
	__except (HandleException(GetExceptionCode(), GetExceptionInformation()))
	{
		GetUI(cmdId)->HandleError( E_FATAL, 0, ExceptionError->Text());
	}
	return result;
}
