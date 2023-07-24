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
 * Name		: p4base.h
 *
 * Author	: dbb
 *
 * Description	:  p4base is the base class for all classes which handles 
 *  (pointers)for instances of those classes are passed in and out if the DLL.
 *  It provides a mechanism to register the handles to those objects when they
 *  are created so they can be validated when passed in as parameters.
 *
 ******************************************************************************/
#include "stdafx.h"
#include "p4base.h"
#include "Lock.h"
#include "P4BridgeServer.h"

#include <sstream>

/*******************************************************************************
* Keep a doubly linked list of handles for each type to be tracked.
*******************************************************************************/
#ifdef _DEBUG
#ifdef _DEBUG_MEMORY

#include <tchar.h>

#endif
p4base** p4base::pFirstObject = NULL;
p4base** p4base::pLastObject = NULL;

int p4base::ItemCount = 0;
int p4base::ItemCounts[p4typesCount];

int p4base::TotalItems = 1;
int p4base::NextItemIds[p4typesCount];

#endif

int p4base::InitP4BaseLocker()
{
	Locker.InitCritSection();
	return 1;
}

// Used to lock access for multi threading
ILockable p4base::Locker = ILockable();

int p4base::P4BaseLockerInit = p4base::InitP4BaseLocker();

/*******************************************************************************
* Constructor
*
*   Add this object to the correct list, based on its type.
*
*   nType: The type of this object. It is passed from the derived objects 
*       constructer so it can be determined at run time. A call to the virtual
*       function GetType() does not work here in the base class constructor.
*
*******************************************************************************/
p4base::p4base(int ntype)
{
	// save the type for use in the destructor when we can no longer use the
	//  virtual function GetType().
	type =  ntype;

#ifdef _DEBUG
	LOCK(&Locker);

	// Check to see if we have allocated are array of lists yet. These will only
	//  be created when the first object is registered.
	if (!pFirstObject)
	{
		pFirstObject = new p4base*[p4typesCount];
		for (int i = 0; i < p4typesCount; i++)
		{
			pFirstObject[i] = NULL;
			ItemCounts[i] = 0;
			NextItemIds[i] = 0;
		}
	}
	if (!pLastObject)
	{
		pLastObject = new p4base*[p4typesCount];
		for (int i = 0; i < p4typesCount; i++)
		{
			pLastObject[i] = NULL;
		}
   }

	// Initialize the list pointers
	pNextItem = NULL;
	pPrevItem = NULL;

	// Add to the list of objects registered to be exported
	if(!pFirstObject[type])
	{
		// first object, initialize the list with this as the only element
		pFirstObject[type] = this;
		pLastObject[type] = this;
	}
	else
	{
		// add to the end of the list
		pLastObject[type]->pNextItem = this;
		pPrevItem = pLastObject[type];
		pLastObject[type] = this;
	}
	ItemCount++;
	ItemCounts[type]++;
	TotalItems++;
	ItemId = ++NextItemIds[type];

#ifdef _DEBUG_MEMORY
	LOG_DEBUG2(4, "Creating %s [0x%llu]", GetTypeStr(type), this);
#endif

#endif
}

/*******************************************************************************
* Destructor
*
*   Remove this object from the correct list, based on its type.
*
*******************************************************************************/
p4base::~p4base(void)
{
#ifdef _DEBUG
	LOCK(&Locker); 

#ifdef _DEBUG_MEMORY
	LOG_DEBUG2(4, "Deleting %s [0x%llu]", GetTypeStr(type), this);
#endif

	ItemCount--;

	ItemCounts[type]--;

	// Remove from the list
	if (!pPrevItem && !pNextItem)
	{
		// last object in the list, so NULL out the list head and tail pointers
		pFirstObject[type] = NULL;
		pLastObject[type] = NULL;
	}
	else if (!pPrevItem && pNextItem)
	{
		// first object in list, set the head to the next object in the list
		pFirstObject[type] = pNextItem;
		pNextItem->pPrevItem = NULL;
	}
	else if (pPrevItem && !pNextItem)
	{
		// last object, set the tail to the pervious object in the list
		pLastObject[type] = pPrevItem;
		pPrevItem->pNextItem = NULL;
	}
	else 
	{
		// in the middle of the list, so link the pointers for the previous 
		//  and next objects.
		pPrevItem->pNextItem = pNextItem;
		pNextItem->pPrevItem = pPrevItem;
	}
#endif
}

void p4base::Cleanup(void)
{
#ifdef _DEBUG
	if (pFirstObject)
	{
		delete[] pFirstObject;
	}
	if (pLastObject)
	{
		delete[] pLastObject;
   }
#endif
}

int p4base::ValidateHandle_Int( p4base* pObject, int type )
{
	if (!pObject)
		return 0;

	p4base* pCur = NULL;

	// Use Windows Structured Exception Handling to detect memory violations
	__try
	{
		if ((type < 0) || (type >=p4typesCount) || (type != pObject->Type()))
		{
			return 0; // invalid type
		}
#ifndef _DEBUG
		return 1;
	}
#ifdef _WIN32 	
	__except (1) //EXCEPTION_EXECUTE_HANDLER
	{
		// access violation, so definitely not valid.
		return 0;
	}
#else
	catch (int error) //EXCEPTION_EXECUTE_HANDLER
	{
		// access violation, so definitely not valid.
		return 0;
	}

#endif	
#else
		pCur = pFirstObject[type];
	}
	__except (1) //EXCEPTION_EXECUTE_HANDLER
	{
		// access violation, so definitely not valid.
		return 0;
	}
	while ( pCur != NULL)
	{
		if (pObject == pCur)  // pointers are the same
		{   
			return 1;
		}
		pCur = pCur->pNextItem;
	}
#endif
 
	return 0;
}

/*******************************************************************************
* ValidateHandle( p4base* pObject )
*
*   Static function to validate a handle
*
*   nType: The type of this object. It is passed from the derived objects 
*       constructer so it can be determined at run time. A call to the virtual
*       function GetType() does not work here in the base class constructor.
*
*******************************************************************************/
int p4base::ValidateHandle( p4base* pObject, int type )
{
	LOCK(&Locker);

	return ValidateHandle_Int( pObject, type );
}

#ifdef _DEBUG

const char* p4base::GetTypeStr(int type)
{
	switch (type)
	{
	case tP4BridgeServer:
		return "P4BridgeServer";
	case tP4BridgeClient:
		return "BridgeClient";
	case tStrDictListIterator:
		return "StrDictListIterator";
	case tStrDictList:
		return "StrDictList";
	case tKeyValuePair:
		return "KeyValuePair";
	case tP4ClientError:
		return "P4ClientError";
	case tP4MapApi:
		return "P4MapApi";
	case tP4P4FileSys:
		return "P4P4FileSys";
	case tP4ClientMerge:
		return "P4ClientMerge";
	case tP4ClientResolve:
		return "P4ClientResolve";
	case tP4ClientInfoMsg:
		return "P4ClientInfoMsg";
	case tParallelTransfer:
		return "ParallelTransfer";
	case p4typesCount:
		return "Error!p4typesCount";
#ifdef _DEBUG_MEMORY
	case tConnectionManager:
		return "ConnectionManager";
	case tP4Connection:
		return "P4Connection";
	//case tIdleConnection:
	//	return "IdleConnection";
#endif
	default:
		return "Error!UnknownType";
	}
}

#ifdef _DEBUG_MEMORY

static char* MemLogFile = "c:\\tmp\\p4bridge.memory.log.txt"; 

void  p4base::LogMemoryEvent(char * Event)
{
	LOG_DEBUG(4, Event);
}

void  p4base::PrintMemoryState(char *Title)
{
	printf("%s: Object Count Dump\r\n", Title);
		
	printf("All Item Types : %d Items Created, %d items remaining \r\n", GetTotalItemCount(), GetItemCount());

	for (int i = 0; i < p4typesCount; i++)
	{
		printf("Type:%s : %d Items Created, %d items remaining \r\n", GetTypeStr(i), NextItemIds[i],  GetItemCount(i));
	}
}

void  p4base::DumpMemoryState(char *Title)
{
	int bErrorFlag;
	DWORD dwBytesWritten = 0;

	HANDLE  hLogFile = CreateFile(  MemLogFile,GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);

	SetFilePointer(hLogFile,0,NULL,FILE_END);

	{
		std::stringstream ss;
		ss << Title << ": Object Count Dump\r\n";
		string line = ss.str();

		bErrorFlag = WriteFile(
			hLogFile,           // open file handle
			line.c_str(),			// start of data to write
			(DWORD) line.length(),				// number of bytes to write
			&dwBytesWritten,	// number of bytes that were written
			NULL);				// no overlapped structure
	}

	{
		std::stringstream ss;
		ss << "All Item Types : " << GetTotalItemCount() << "Items Created, " << GetItemCount() << " items remaining \r\n";
		string line = ss.str();

		bErrorFlag = WriteFile(
			hLogFile,           // open file handle
			line.c_str(),			// start of data to write
			(DWORD) line.length(),				// number of bytes to write
			&dwBytesWritten,	// number of bytes that were written
			NULL);				// no overlapped structure
	}

	for (int i = 0; i < p4typesCount; i++)
	{
		std::stringstream ss;
		ss << "Type:" << GetTypeStr(i) << " : " << NextItemIds[i] << " Items Created, " << GetItemCount(i) << " items remaining \r\n";
		string line = ss.str();

		bErrorFlag = WriteFile( 
					hLogFile,           // open file handle
					line.c_str(),			// start of data to write
					(DWORD) line.length(),				// number of bytes to write
					&dwBytesWritten,	// number of bytes that were written
					NULL);				// no overlapped structure
	}

	CloseHandle(hLogFile);
}
#else
void  p4base::LogMemoryEvent(char * Event) {}
void  p4base::PrintMemoryState(char *Title) {}
void  p4base::DumpMemoryState(char *Title) {}
#endif
#endif
