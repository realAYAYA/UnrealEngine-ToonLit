// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMCollectionCycleRequest.h"
#include "Async/UniqueLock.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMLog.h"

namespace Verse
{

bool FCollectionCycleRequest::IsDone() const
{
	return FHeap::CompletedCycleVersion >= RequestedCycleVersion;
}

void FCollectionCycleRequest::Wait(FIOContext Context) const
{
	using namespace UE;

	if (FHeap::NormalizeWithoutThreadingAtCollectionStart())
	{
		while (FHeap::CompletedCycleVersion < RequestedCycleVersion)
		{
			FHeap::RunCollectionCycle(Context);
		}
	}
	else
	{
		TUniqueLock Lock(FHeap::Mutex);
		UE_LOG(LogVerseGC, Verbose, TEXT("Waiting for cycle %llu to finish"), RequestedCycleVersion);
		FHeap::CheckCycleTriggerInvariants();
		while (FHeap::CompletedCycleVersion < RequestedCycleVersion)
		{
			FHeap::ConditionVariable.Wait(FHeap::Mutex);
		}
		FHeap::CheckCycleTriggerInvariants();
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)