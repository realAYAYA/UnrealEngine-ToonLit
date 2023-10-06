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

	TSet<FNetHandle> DirtyObjects;
	FNetBitArray AssignedHandleIndices;
	FNetBitArray Pollers;
	uint32 PollerCount = 0;
};

TSet<FNetHandle> FGlobalDirtyNetObjectTracker::FPimpl::EmptyDirtyObjects;

FGlobalDirtyNetObjectTracker::FPimpl* FGlobalDirtyNetObjectTracker::Instance = nullptr;

void FGlobalDirtyNetObjectTracker::MarkNetObjectStateDirty(FNetHandle NetHandle)
{
	if (Instance && Instance->PollerCount > 0)
	{
		Instance->DirtyObjects.Add(NetHandle);
	}
}

bool FGlobalDirtyNetObjectTracker::IsNetObjectStateDirty(FNetHandle NetHandle)
{
	if (Instance)
	{
		return Instance->DirtyObjects.Find(NetHandle) != nullptr;
	}

	return false;
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

	if (ensureAlwaysMsgf((HandleIndex < Instance->AssignedHandleIndices.GetNumBits()) && Instance->AssignedHandleIndices.GetBit(HandleIndex), TEXT("Destroying unknown poller with handle index %u"), HandleIndex))
	{
		Instance->AssignedHandleIndices.ClearBit(HandleIndex);

		const uint32 PollerCalled = Instance->Pollers.GetBit(HandleIndex);
		ensureAlwaysMsgf(PollerCalled == 0U, TEXT("Destroying poller that called GetDirtyNetObjects() but not ResetDirtyNetObjects()"));
		Instance->Pollers.ClearBit(HandleIndex);

		--Instance->PollerCount;
		if (Instance->PollerCount <= 0)
		{
			Instance->DirtyObjects.Reset();
		}
	}
}

const TSet<FNetHandle>& FGlobalDirtyNetObjectTracker::GetDirtyNetObjects(const FPollHandle& Handle)
{
	if (Instance && Handle.IsValid())
	{
		Instance->Pollers.SetBit(Handle.Index);
		return Instance->DirtyObjects;
	}

	return FPimpl::EmptyDirtyObjects;
}

void FGlobalDirtyNetObjectTracker::ResetDirtyNetObjects(const FPollHandle& Handle)
{
	if (Instance && Handle.IsValid())
	{
		Instance->Pollers.ClearBit(Handle.Index);
		if (Instance->Pollers.IsNoBitSet())
		{
			Instance->DirtyObjects.Reset();
		}
	}
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
	if (!ensureMsgf(Pollers.IsNoBitSet(), TEXT("FGlobalDirtyNetObjectTracker poller %u forgot to call ResetDirtNetObjects."), Pollers.FindFirstOne()))
	{
		Pollers.Reset();

		// DirtyObjects should already be reset if there are no pollers.
		DirtyObjects.Reset();
	}
}

}
