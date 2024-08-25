// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMLazyInitialized.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"

namespace Verse
{

static TArray<void*> GLeakedObjectsForLazyInitialized;
static UE::FMutex GLeakedObjectsForLazyInitializedMutex;

COREUOBJECT_API void AddLeakedObjectForLazyInitialization(void* Object)
{
	using namespace UE;
	TUniqueLock Lock(GLeakedObjectsForLazyInitializedMutex);
	GLeakedObjectsForLazyInitialized.Push(Object);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)