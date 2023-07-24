// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIContext.h"
#include "RHI.h"

RHI_API void RHISetComputeShaderBackwardsCompatible(IRHIComputeContext* InContext, FRHIComputeShader* InShader)
{
	TRefCountPtr<FRHIComputePipelineState> ComputePipelineState = RHICreateComputePipelineState(InShader);
	InContext->RHISetComputePipelineState(ComputePipelineState);
}

void IRHIComputeContext::RHISetComputeShader(FRHIComputeShader* ComputeShader)
{
	RHISetComputeShaderBackwardsCompatible(this, ComputeShader);
}

void IRHIComputeContext::StatsSetCategory(FRHIDrawStats* InStats, uint32 InCategoryID, uint32 InGPUIndex)
{
	Stats = &InStats->GetGPU(InGPUIndex).GetCategory(InCategoryID);
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
