// Copyright Epic Games, Inc. All Rights Reserved.

#include "NUTGlobals.h"


/**
 * UNUTGlobals
 */

UNUTGlobals::UNUTGlobals(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EventWatcher(nullptr)
	, ServerPortOffset(0)
	, UnitTestNetDriverCount(0)
	, DumpRPCMatches()
	, UnitTestModules()
	, UnloadedModules()
{
}

