// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalFence.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalContext.h"
#include "MetalProfiler.h"

uint32 FMetalFence::Release() const
{
	uint32 Refs = uint32(FPlatformAtomics::InterlockedDecrement(&NumRefs));
	if(Refs == 0)
	{
        FMetalFencePool::Get().ReleaseFence(const_cast<FMetalFence*>(this));
	}
	return Refs;
}

void FMetalFencePool::Initialise(MTL::Device* InDevice)
{
	Device = InDevice;
	for (int32 i = 0; i < FMetalFencePool::NumFences; i++)
	{
        FMetalFence* F = new FMetalFence;
        F->Set(Device->newFence());
        Lifo.Push(F);
	}
	Count = FMetalFencePool::NumFences;
    Allocated = 0;
}

FMetalFence* FMetalFencePool::AllocateFence()
{
	FMetalFence* Fence = Lifo.Pop();
	if (Fence)
	{
        INC_DWORD_STAT(STAT_MetalFenceCount);
        FPlatformAtomics::InterlockedDecrement(&Count);
        FPlatformAtomics::InterlockedIncrement(&Allocated);
	}
	check(Fence);
	Fence->Reset();
	return Fence;
}

void FMetalFencePool::ReleaseFence(FMetalFence* const InFence)
{
	if (InFence)
	{
        DEC_DWORD_STAT(STAT_MetalFenceCount);
        FPlatformAtomics::InterlockedDecrement(&Allocated);
		FPlatformAtomics::InterlockedIncrement(&Count);
		check(Count <= FMetalFencePool::NumFences);
		Lifo.Push(InFence);
	}
}
