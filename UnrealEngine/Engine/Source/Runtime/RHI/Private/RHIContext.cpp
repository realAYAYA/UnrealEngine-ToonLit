// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIContext.h"
#include "DynamicRHI.h"
#include "RHI.h"
#include "RHIStats.h"

void IRHIComputeContext::StatsSetCategory(FRHIDrawStats* InStats, uint32 InCategoryID, uint32 InGPUIndex)
{
	Stats = &InStats->GetGPU(InGPUIndex).GetCategory(InCategoryID);
}

void RHIGenerateCrossGPUPreTransferFences(const TArrayView<const FTransferResourceParams> Params, TArray<FCrossGPUTransferFence*>& OutPreTransfer)
{
	// Generate destination GPU masks by source GPU
	uint32 DestGPUMasks[MAX_NUM_GPUS];
	for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; GPUIndex++)
	{
		DestGPUMasks[GPUIndex] = 0;
	}

	for (const FTransferResourceParams& Param : Params)
	{
		check(Param.SrcGPUIndex != Param.DestGPUIndex && Param.SrcGPUIndex < GNumExplicitGPUsForRendering&& Param.DestGPUIndex < GNumExplicitGPUsForRendering);
		DestGPUMasks[Param.SrcGPUIndex] |= 1u << Param.DestGPUIndex;
	}

	// Count total number of bits in all the masks, and allocate that number of fences
	uint32 NumFences = 0;
	for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; GPUIndex++)
	{
		NumFences += FMath::CountBits(DestGPUMasks[GPUIndex]);
	}
	OutPreTransfer.SetNumUninitialized(NumFences);

	// Allocate and initialize fence data
	uint32 FenceIndex = 0;
	for (uint32 SrcGPUIndex = 0; SrcGPUIndex < GNumExplicitGPUsForRendering; SrcGPUIndex++)
	{
		uint32 DestGPUMask = DestGPUMasks[SrcGPUIndex];
		for (uint32 DestGPUIndex = FMath::CountTrailingZeros(DestGPUMask); DestGPUMask; DestGPUIndex = FMath::CountTrailingZeros(DestGPUMask))
		{
			FCrossGPUTransferFence* TransferSyncPoint = RHICreateCrossGPUTransferFence();
			TransferSyncPoint->SignalGPUIndex = DestGPUIndex;
			TransferSyncPoint->WaitGPUIndex = SrcGPUIndex;

			OutPreTransfer[FenceIndex] = TransferSyncPoint;
			FenceIndex++;

			DestGPUMask &= ~(1u << DestGPUIndex);
		}
	}
}

void IRHICommandContextPSOFallback::SetGraphicsPipelineStateFromInitializer(const FGraphicsPipelineStateInitializer& PsoInit, uint32 StencilRef, bool bApplyAdditionalState)
{
	RHISetBoundShaderState(
		RHICreateBoundShaderState(
			PsoInit.BoundShaderState.VertexDeclarationRHI,
			PsoInit.BoundShaderState.VertexShaderRHI,
			PsoInit.BoundShaderState.PixelShaderRHI,
			PsoInit.BoundShaderState.GetGeometryShader()
		).GetReference()
	);

	RHISetDepthStencilState(PsoInit.DepthStencilState, StencilRef);
	RHISetRasterizerState(PsoInit.RasterizerState);
	RHISetBlendState(PsoInit.BlendState, FLinearColor(1.0f, 1.0f, 1.0f));
	if (GSupportsDepthBoundsTest)
	{
		RHIEnableDepthBoundsTest(PsoInit.bDepthBounds);
	}
}
