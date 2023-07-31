// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalDynamicRHI.cpp: Metal Dynamic RHI Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "MetalRHIRenderQuery.h"
#include "MetalRHIStagingBuffer.h"
#include "MetalShaderTypes.h"
#include "MetalVertexDeclaration.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalComputePipelineState.h"
#include "MetalTransitionData.h"


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Vertex Declaration Methods -


FVertexDeclarationRHIRef FMetalDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	@autoreleasepool {
		uint32 Key = FCrc::MemCrc32(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));

		// look up an existing declaration
		FVertexDeclarationRHIRef* VertexDeclarationRefPtr = VertexDeclarationCache.Find(Key);
		if (VertexDeclarationRefPtr == NULL)
		{
			// create and add to the cache if it doesn't exist.
			VertexDeclarationRefPtr = &VertexDeclarationCache.Add(Key, new FMetalVertexDeclaration(Elements));
		}

		return *VertexDeclarationRefPtr;
	} // autoreleasepool
}


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Pipeline State Methods -


FGraphicsPipelineStateRHIRef FMetalDynamicRHI::RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	@autoreleasepool {
		FMetalGraphicsPipelineState* State = new FMetalGraphicsPipelineState(Initializer);

		if(!State->Compile())
		{
			// Compilation failures are propagated up to the caller.
			State->Delete();
			return nullptr;
		}

		State->VertexDeclaration = ResourceCast(Initializer.BoundShaderState.VertexDeclarationRHI);
		State->VertexShader = ResourceCast(Initializer.BoundShaderState.VertexShaderRHI);
		State->PixelShader = ResourceCast(Initializer.BoundShaderState.PixelShaderRHI);
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		State->GeometryShader = ResourceCast(Initializer.BoundShaderState.GetGeometryShader());
#endif // PLATFORM_SUPPORTS_GEOMETRY_SHADERS

		State->DepthStencilState = ResourceCast(Initializer.DepthStencilState);
		State->RasterizerState = ResourceCast(Initializer.RasterizerState);

		return State;
	} // autoreleasepool
}

TRefCountPtr<FRHIComputePipelineState> FMetalDynamicRHI::RHICreateComputePipelineState(FRHIComputeShader* ComputeShader)
{
	@autoreleasepool {
		return new FMetalComputePipelineState(ResourceCast(ComputeShader));
	} // autoreleasepool
}


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Staging Buffer Methods -


FStagingBufferRHIRef FMetalDynamicRHI::RHICreateStagingBuffer()
{
	return new FMetalRHIStagingBuffer();
}

void* FMetalDynamicRHI::RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI)
{
	FMetalRHIStagingBuffer* Buffer = ResourceCast(StagingBuffer);
	return Buffer->Lock(Offset, SizeRHI);
}

void FMetalDynamicRHI::RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer)
{
	FMetalRHIStagingBuffer* Buffer = ResourceCast(StagingBuffer);
	Buffer->Unlock();
}


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Resource Transition Methods -


void FMetalDynamicRHI::RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo)
{
	// Construct the data in-place on the transition instance
	new (Transition->GetPrivateData<FMetalTransitionData>()) FMetalTransitionData(CreateInfo.SrcPipelines, CreateInfo.DstPipelines, CreateInfo.Flags, CreateInfo.TransitionInfos);
}

void FMetalDynamicRHI::RHIReleaseTransition(FRHITransition* Transition)
{
	// Destruct the private data object of the transition instance.
	Transition->GetPrivateData<FMetalTransitionData>()->~FMetalTransitionData();
}


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Render Query Methods -


FRenderQueryRHIRef FMetalDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	@autoreleasepool {
		FRenderQueryRHIRef Query = new FMetalRHIRenderQuery(QueryType);
		return Query;
	}
}

FRenderQueryRHIRef FMetalDynamicRHI::RHICreateRenderQuery_RenderThread(class FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType)
{
	@autoreleasepool {
		return GDynamicRHI->RHICreateRenderQuery(QueryType);
	}
}

bool FMetalDynamicRHI::RHIGetRenderQueryResult(FRHIRenderQuery* QueryRHI, uint64& OutNumPixels, bool bWait, uint32 GPUIndex)
{
	@autoreleasepool {
		check(IsInRenderingThread());
		FMetalRHIRenderQuery* Query = ResourceCast(QueryRHI);
		return Query->GetResult(OutNumPixels, bWait, GPUIndex);
	}
}
