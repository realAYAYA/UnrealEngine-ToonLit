// Copyright Epic Games, Inc. All Rights Reserved.

#include "Templates/Function.h"
#include "ID3D12DynamicRHI.h"

namespace D3D12RHI
{
	// Use these functions with care!

	// Returns ID3D12GraphicsCommandList* and ID3D12CommandQueue*
	UE_DEPRECATED(5.1, "ID3D12DynamicRHI should be used to acquire D3D12 command lists and command queues")
	D3D12RHI_API void GetGfxCommandListAndQueue(FRHICommandList& RHICmdList, void*& OutGfxCmdList, void*& OutCommandQueue);
}
