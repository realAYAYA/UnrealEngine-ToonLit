// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GarbageCollectionInternal.h: Unreal realtime garbage collection internal helpers
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/GarbageCollection.h"
#include "UObject/GCScopeLock.h"

extern FGCCSyncObject* GGCSingleton;

/** Returns true if GC wants to run */
FORCEINLINE bool IsGarbageCollectionWaiting()
{
	return GGCSingleton->IsGCWaiting();
}