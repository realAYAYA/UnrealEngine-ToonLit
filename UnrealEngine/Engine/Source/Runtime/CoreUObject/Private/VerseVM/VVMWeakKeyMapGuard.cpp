// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMWeakKeyMapGuard.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMNeverDestroyed.h"

namespace Verse
{

bool FWeakKeyMapGuard::ConductCensus()
{
	V_DIE_UNLESS(TryGet());
	TryGet()->ConductCensus();
	if (TryGet()->IsEmpty())
	{
		delete TryGet();
		GetRef() = nullptr;
		return true;
	}
	else
	{
		FHeap::ReportMarkedNativeBytes(TryGet()->GetAllocatedSize());
		return false;
	}
}

FWeakKeyMap* FWeakKeyMapGuard::GetSlow()
{
	using namespace UE;
	GetRef() = new FWeakKeyMap();
	{
		TUniqueLock Lock(FHeap::WeakKeyMapsMutex);
		FHeap::WeakKeyMapsByHeader->Push(Guard.GetHeader());
	}
	return GetRef();
}

} // namespace Verse
#endif // WITH_VERSE_VM
