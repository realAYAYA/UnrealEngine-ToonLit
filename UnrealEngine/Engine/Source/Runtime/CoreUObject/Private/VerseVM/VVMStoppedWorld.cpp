// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMStoppedWorld.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMContextImpl.h"
#include "VerseVM/VVMLog.h"

namespace Verse
{
FStoppedWorld::FStoppedWorld(FStoppedWorld&& Other)
	: Contexts(MoveTemp(Other.Contexts))
	, bHoldingStoppedWorldMutex(Other.bHoldingStoppedWorldMutex)
{
	V_DIE_UNLESS(Other.Contexts.IsEmpty());
	Other.bHoldingStoppedWorldMutex = false;
}

FStoppedWorld& FStoppedWorld::operator=(FStoppedWorld&& Other)
{
	V_DIE_UNLESS(Contexts.IsEmpty());
	V_DIE_IF(bHoldingStoppedWorldMutex);

	Contexts = MoveTemp(Other.Contexts);
	bHoldingStoppedWorldMutex = Other.bHoldingStoppedWorldMutex;

	V_DIE_UNLESS(Other.Contexts.IsEmpty());
	Other.bHoldingStoppedWorldMutex = false;

	return *this;
}

void FStoppedWorld::CancelStop()
{
	if (bHoldingStoppedWorldMutex)
	{
		for (FAccessContext Context : Contexts)
		{
			Context.GetImpl()->CancelStop();
		}
		FContextImpl::StoppedWorldMutex.Unlock();
		Contexts.Empty();
		bHoldingStoppedWorldMutex = false;
	}
	else
	{
		V_DIE_UNLESS(Contexts.IsEmpty());
	}
}

FStoppedWorld::~FStoppedWorld()
{
	V_DIE_UNLESS(Contexts.IsEmpty());
	V_DIE_IF(bHoldingStoppedWorldMutex);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)