// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GarbageCollectionGlobals.h: Garbage Collection Global State Vars
=============================================================================*/

#pragma once

#include "UObject/ObjectMacros.h"

namespace UE::GC
{
	/** Current EInternalObjectFlags value representing a reachable object */
	extern COREUOBJECT_API EInternalObjectFlags GReachableObjectFlag;

	/** Current EInternalObjectFlags value representing an unreachable object */
	extern COREUOBJECT_API EInternalObjectFlags GUnreachableObjectFlag;

	/** Current EInternalObjectFlags value representing a maybe unreachable object */
	extern COREUOBJECT_API EInternalObjectFlags GMaybeUnreachableObjectFlag;

	/** true if incremental reachability analysis is in progress (global for faster access in low level structs and functions otherwise use IsIncrementalReachabilityAnalisysPending()) */
	extern COREUOBJECT_API bool GIsIncrementalReachabilityPending;
}
