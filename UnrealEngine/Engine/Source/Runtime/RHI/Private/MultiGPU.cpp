// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.cpp: Multi-GPU support
=============================================================================*/

#include "MultiGPU.h"
#include "RHI.h"

#if WITH_SLI || WITH_MGPU
uint32 GNumAlternateFrameRenderingGroups = 1;
uint32 GNumExplicitGPUsForRendering = 1;
uint32 GVirtualMGPU = 0;
#endif

#if WITH_MGPU

RHI_API TArray<FRHIGPUMask, TFixedAllocator<MAX_NUM_GPUS>> AFRUtils::GroupMasks;
RHI_API TArray<FRHIGPUMask, TFixedAllocator<MAX_NUM_GPUS>> AFRUtils::SiblingMasks;

void AFRUtils::StaticInitialize()
{
	check(GroupMasks.Num() == 0 && SiblingMasks.Num() == 0);
	const uint32 NumGPUsPerGroup = GetNumGPUsPerGroup();

	// Set up group masks.
	for (uint32 GroupIndex = 0; GroupIndex < GNumAlternateFrameRenderingGroups; GroupIndex++)
	{
		FRHIGPUMask& GPUMask = GroupMasks.Emplace_GetRef(FRHIGPUMask::FromIndex(GroupIndex));
		for (uint32 IndexWithinGroup = 1; IndexWithinGroup < NumGPUsPerGroup; IndexWithinGroup++)
		{
			GPUMask |= FRHIGPUMask::FromIndex(IndexWithinGroup * GNumAlternateFrameRenderingGroups + GroupIndex);
		}
	}

	// Set up sibling masks.
	for (uint32 IndexWithinGroup = 0; IndexWithinGroup < NumGPUsPerGroup; IndexWithinGroup++)
	{
		FRHIGPUMask& GPUMask = SiblingMasks.Emplace_GetRef(FRHIGPUMask::FromIndex(IndexWithinGroup * GNumAlternateFrameRenderingGroups));
		for (uint32 GroupIndex = 1; GroupIndex < GNumAlternateFrameRenderingGroups; GroupIndex++)
		{
			GPUMask |= FRHIGPUMask::FromIndex(IndexWithinGroup * GNumAlternateFrameRenderingGroups + GroupIndex);
		}
	}
}

#endif // WITH_MGPU
