// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

extern int32 GEmitRgpFrameMarkers;

// A fence that is manually signaled on the graphics pipe (all graphics pipes in mGPU setups).
// @todo: Remove this. Systems that rely on this fence should be converted to use sync points instead.
class FD3D12ManualFence final
{
	FD3D12Adapter* const Parent;
	struct FFencePair
	{
		TRefCountPtr<ID3D12Fence> Fence;
		class FD3D12CommandContext* Context;
	};

	TArray<FFencePair> FencePairs;

	FThreadSafeCounter NextFenceValue = 0;
	uint64 CompletedFenceValue = 0;

	FD3D12ManualFence(FD3D12ManualFence const&) = delete;
	FD3D12ManualFence(FD3D12ManualFence&&) = delete;

public:
	FD3D12ManualFence(FD3D12Adapter* InParent);

	// Returned the fence value which has been signaled by the GPU.
	// If bUpdateCachedFenceValue is false, only the cached value is returned.
	// Otherwise, the latest fence value is queried from the driver, and the cached value is updated.
	uint64 GetCompletedFenceValue(bool bUpdateCachedFenceValue);

	// Determines if the given fence value has been signaled on the GPU.
	bool IsFenceComplete(uint64 FenceValue, bool bUpdateCachedFenceValue)
	{
		return GetCompletedFenceValue(bUpdateCachedFenceValue) >= FenceValue;
	}

	// Returns the next value to be signaled.
	uint64 GetNextFenceToSignal() const
	{
		return NextFenceValue.GetValue() + 1;
	}

	// Should only be called by RHIAdvanceFrameFence
	void AdvanceFrame();
};
