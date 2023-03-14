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

//Forward ref
class ILockable;

/*******************************************************************************
 * These are the types of objects to track. To add a new type to track, add it
 * before the p4typesCount enumerator.
 ******************************************************************************/
enum p4types {
    tP4BridgeServer = 0,
    tP4BridgeClient,
    tStrDictListIterator,
    tStrDictList,
    tKeyValuePair,
    tP4ClientError,
	tP4MapApi,
	tP4P4FileSys,
	tP4ClientMerge,
	tP4ClientResolve,
	tP4ClientInfoMsg,
	tParallelTransfer,
#ifdef _DEBUG_MEMORY
	tP4Connection,
	tConnectionManager,
	p4typesCount
#else
	p4typesCount
#endif
};

/*******************************************************************************
 * class p4base: 
 *
 *      This is the base classes for all objects exported by the DLL. The 
 *  constructor registers the handle (pointer) of the object and the destructor
 *  unregisters it. The static method ValidateHandle() allows a handle passed
 *  into the DLL against the registry of handles that have been exported.
 ******************************************************************************/
class p4base /* abstract */
{
private:
    p4base() {};

public:
    p4base(int ntype);
    virtual ~p4base(void);

	static void Cleanup(void);

    // Validate a Handle (pointer) to verify that it points to a valid object
    //      that has not been deleted.
    static int ValidateHandle( p4base* pObject, int type );

    // Simple type identification for registering objects
    virtual int Type(void) = 0;

private:
	static ILockable Locker;
	static int InitP4BaseLocker();
	static int P4BaseLockerInit;

    // save the type for use in the destructor when we can no longer use the
    //  virtual function GetType().
    int type;

	static int ValidateHandle_Int( p4base* pObject, int type );

#ifdef _DEBUG

protected:
    // Maintain a list of objects for each type
    static p4base** pFirstObject;
    static p4base** pLastObject;

    // doubly linked list of objects of a given type
    p4base* pNextItem;
    p4base* pPrevItem;

	// Maintain a count of items in the list
	static int ItemCount;
	static int ItemCounts[p4typesCount];
	
	// Give each item a unique ID
	static int TotalItems;
	int ItemId;

	static int NextItemIds[p4typesCount];

public:
	static int GetItemCount() { return ItemCount;}
	static int GetItemCount(int type) { return ItemCounts[type]; }
	static int GetTotalItemCount() { return TotalItems;}
	static int GetTotalItemCount(int type) { return NextItemIds[type];}
	
	// Give each item a unique ID
	int GetItemId()  { return ItemId;}

	static const char* GetTypeStr(int type);

	static void PrintMemoryState(char *Title);
	static void DumpMemoryState(char *Title);
private:
	void LogMemoryEvent(char * Event);
#endif
};

// These macros validate the Handle passed into a routine and return the 
//   correct failure value for the return type of the function.
#define VALIDATE_HANDLE_P(Hdl, type) if(!p4base::ValidateHandle( (p4base*) Hdl, type )) return NULL;
#define VALIDATE_HANDLE_V(Hdl, type) if(!p4base::ValidateHandle( (p4base*) Hdl, type )) return ;
#define VALIDATE_HANDLE_I(Hdl, type) if(!p4base::ValidateHandle( (p4base*) Hdl, type )) return 0;
#define VALIDATE_HANDLE_B(Hdl, type) if(!p4base::ValidateHandle( (p4base*) Hdl, type )) return false;
#define VALIDATE_HANDLE(Hdl, type) p4base::ValidateHandle( (p4base*) Hdl, type )
#define VALIDATE_HANDLE_C(Hdl, type) if(!p4base::ValidateHandle( (p4base*) Hdl, type )) return '\0';
;
