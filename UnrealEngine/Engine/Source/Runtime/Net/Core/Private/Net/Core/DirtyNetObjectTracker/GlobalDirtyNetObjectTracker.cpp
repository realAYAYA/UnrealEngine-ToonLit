// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Core/DirtyNetObjectTracker/GlobalDirtyNetObjectTracker.h"
#include "Containers/Set.h"
#include "Misc/CoreDelegates.h"

namespace UE::Net
{

class FGlobalDirtyNetObjectTracker::FPimpl
{
private:
	FPimpl();
	~FPimpl();

	void ResetDirtyNetObjects();

private:
	friend FGlobalDirtyNetObjectTracker;

	static TSet<FNetHandle> EmptyDirtyObjects;

private:

	struct FPollerStatus
	{
		// Is this status tied to an active registered poller
		uint8 bIsActive:1;

		// Has this poller read the dirty list this frame yet.
		uint8 bHasGathered:1;

		FPollerStatus() : bIsActive(false), bHasGathered(false) {}

		void ClearStatus()
		{
			*this = FPollerStatus();
		}
	};

private:

	TSet<FNetHandle> DirtyObjects;

	FNetBitArray AssignedHandleIndices;
	FNetBitArray Pollers;

	TArray<FPollerStatus> PollerStatuses;

	uint32 PollerCount = 0;

	/** When true detect and prevent illegal changes to the dirty object list. */
	bool bLockDirtyList = false;
};

TSet<FNetHandle> FGlobalDirtyNetObjectTracker::FPimpl::EmptyDirtyObjects;

FGlobalDirtyNetObjectTracker::FPimpl* FGlobalDirtyNetObjectTracker::Instance = nullptr;

void FGlobalDirtyNetObjectTracker::MarkNetObjectStateDirty(FNetHandle NetHandle)
{
	if (Instance && Instance->PollerCount > 0)
	{
		if (ensureMsgf(!Instance->bLockDirtyList, TEXT("MarkNetObjectStateDirty was called while the dirty list was set to read-only.")))
		{
			Instance->DirtyObjects.Add(NetHandle);
		}
	}
}

FGlobalDirtyNetObjectTracker::FPollHandle FGlobalDirtyNetObjectTracker::CreatePoller()
{
	if (Instance)
	{
		if (Instance->PollerCount >= Instance->AssignedHandleIndices.GetNumBits())
		{
			Instance->AssignedHandleIndices.SetNumBits(Instance->PollerCount + 1U);
			Instance->Pollers.SetNumBits(Instance->PollerCount + 1U);
		}

		const uint32 HandleIndex = Instance->AssignedHandleIndices.FindFirstZero();
		if (!ensure(HandleIndex != FNetBitArrayBase::InvalidIndex))
		{
			return FPollHandle();
		}

		Instance->AssignedHandleIndices.SetBit(HandleIndex);
		++Instance->PollerCount;

		Instance->PollerStatuses.SetNum(Instance->PollerCount, EAllowShrinking::No);
		Instance->PollerStatuses[HandleIndex].bIsActive = true;

		return FPollHandle(HandleIndex);
	}

	return FPollHandle();
}

void FGlobalDirtyNetObjectTracker::DestroyPoller(uint32 HandleIndex)
{
	if (HandleIndex == FPollHandle::InvalidIndex)
	{
		return;
	}

	if (ensureMsgf((HandleIndex < Instance->AssignedHandleIndices.GetNumBits()) && Instance->AssignedHandleIndices.GetBit(HandleIndex), TEXT("Destroying unknown poller with handle index %u"), HandleIndex))
	{
		Instance->AssignedHandleIndices.ClearBit(HandleIndex);

		const uint32 PollerCalled = Instance->Pollers.GetBit(HandleIndex);
		ensureMsgf(PollerCalled == 0U, TEXT("Destroying poller that called GetDirtyNetObjects() but not ResetDirtyNetObjects()"));
		Instance->Pollers.ClearBit(HandleIndex);

		Instance->PollerStatuses[HandleIndex].ClearStatus();

		--Instance->PollerCount;
		if (Instance->PollerCount <= 0)
		{
			Instance->DirtyObjects.Reset();
			Instance->bLockDirtyList = false;
		}
	}
}

const TSet<FNetHandle>& FGlobalDirtyNetObjectTracker::GetDirtyNetObjects(const FPollHandle& Handle)
{
	if (Instance && Handle.IsValid())
	{
		check(Instance->PollerStatuses[Handle.Index].bIsActive);

		Instance->Pollers.SetBit(Handle.Index);
		Instance->PollerStatuses[Handle.Index].bHasGathered = true;
		return Instance->DirtyObjects;
	}

	return FPimpl::EmptyDirtyObjects;
}

void FGlobalDirtyNetObjectTracker::LockDirtyListUntilReset(const FPollHandle& Handle)
{
	if (Instance && Handle.IsValid())
	{
		check(Instance->PollerStatuses[Handle.Index].bIsActive);

		// From here prevent new dirty objects until the list is reset.
		Instance->bLockDirtyList = true;
	}
}

void FGlobalDirtyNetObjectTracker::ResetDirtyNetObjects(const FPollHandle& Handle)
{
	if (Instance && Handle.IsValid())
	{
		check(Instance->PollerStatuses[Handle.Index].bIsActive);

		Instance->Pollers.ClearBit(Handle.Index);

		if (Instance->Pollers.IsNoBitSet())
		{

#if DO_CHECK
			bool bAllPollersGathered = true;
			for (const FPimpl::FPollerStatus& PollerStatus : Instance->PollerStatuses)
			{
				bAllPollersGathered &= !PollerStatus.bIsActive || PollerStatus.bHasGathered;
			}
			ensureMsgf(bAllPollersGathered, TEXT("Not all pollers gathered the dirty list before the list got reset. Those pollers will never know those objects were dirty."));
#endif

			Instance->bLockDirtyList = false;
			Instance->DirtyObjects.Reset();
		}
	}
}

bool FGlobalDirtyNetObjectTracker::ResetDirtyNetObjectsIfSinglePoller(const FPollHandle& Handle)
{
	if (Instance && Handle.IsValid())
	{
		check(Instance->PollerStatuses[Handle.Index].bIsActive);

		// Are we the only poller registered to read and reset the list
		if (Instance->AssignedHandleIndices.CountSetBits() == 1)
		{
			check(Instance->Pollers.IsBitSet(Handle.Index));

			Instance->bLockDirtyList = false;
			Instance->DirtyObjects.Reset();

			Instance->Pollers.ClearBit(Handle.Index);

			return true;
		}
	}

	return false;
}

void FGlobalDirtyNetObjectTracker::Init()
{
	checkf(Instance == nullptr, TEXT("%s"), TEXT("Only one FGlobalDirtyNetObjectTracker instance may exist."));
	Instance = new FGlobalDirtyNetObjectTracker::FPimpl();
}

void FGlobalDirtyNetObjectTracker::Deinit()
{
	delete Instance;
	Instance = nullptr;
}

FGlobalDirtyNetObjectTracker::FPimpl::FPimpl()
{
#if WITH_ENGINE
	FCoreDelegates::OnEndFrame.AddRaw(this, &FPimpl::ResetDirtyNetObjects);
#endif
}

FGlobalDirtyNetObjectTracker::FPimpl::~FPimpl()
{
#if WITH_ENGINE
	FCoreDelegates::OnEndFrame.RemoveAll(this);
#endif
}

void FGlobalDirtyNetObjectTracker::FPimpl::ResetDirtyNetObjects()
{
	if (!ensureMsgf(Pollers.IsNoBitSet(), TEXT("FGlobalDirtyNetObjectTracker poller %u forgot to call ResetDirtyNetObjects."), Pollers.FindFirstOne()))
	{
		Pollers.Reset();

		// DirtyObjects should already be reset if there are no pollers.
		DirtyObjects.Reset();

		bLockDirtyList = false;
	}
}

}
