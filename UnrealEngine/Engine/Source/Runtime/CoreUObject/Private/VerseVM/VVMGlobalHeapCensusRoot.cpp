// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMGlobalHeapCensusRoot.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMNeverDestroyed.h"

namespace Verse
{

FGlobalHeapCensusRoot::FGlobalHeapCensusRoot()
{
	using namespace UE;
	TUniqueLock Lock(FHeap::GlobalCensusRootMutex);
	V_DIE_UNLESS(FHeap::bIsInitialized);
	FHeap::GlobalCensusRoots->Push(this);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)