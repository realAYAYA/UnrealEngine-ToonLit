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
    MTL_SCOPED_AUTORELEASE_POOL;
    
    uint32 Key = FCrc::MemCrc32(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));

    // look up an existing declaration
    FVertexDeclarationRHIRef* VertexDeclarationRefPtr = VertexDeclarationCache.Find(Key);
    if (VertexDeclarationRefPtr == NULL)
    {
        // create and add to the cache if it doesn't exist.
        VertexDeclarationRefPtr = &VertexDeclarationCache.Add(Key, new FMetalVertexDeclaration(Elements));
    }

    return *VertexDeclarationRefPtr;
}


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Pipeline State Methods -


FGraphicsPipelineStateRHIRef FMetalDynamicRHI::RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
    FMetalGraphicsPipelineState* State = new FMetalGraphicsPipelineState(Initializer);

#if METAL_USE_METAL_SHADER_CONVERTER
	
	if(IsMetalBindlessEnabled())
	{
		FMetalVertexShader* VertexShader = ResourceCast(Initializer.BoundShaderState.VertexShaderRHI);
		
		if (VertexShader != nullptr)
		{
			FMetalVertexDeclaration* VertexDeclaration = ResourceCast(Initializer.BoundShaderState.VertexDeclarationRHI);
			
			IRShaderReflection* VertexReflection = IRShaderReflectionCreate();
			IRMetalLibBinary* StageInMetalLib = IRMetalLibBinaryCreate();
			
			const FString& SerializedJSON = VertexShader->Bindings.IRConverterReflectionJSON;
			IRShaderReflectionDeserialize(TCHAR_TO_ANSI(*SerializedJSON), VertexReflection);
			
			bool bStageInCreationSuccessful = IRMetalLibSynthesizeStageInFunction(CompilerInstance,
																				  VertexReflection,
																				  &VertexDeclaration->InputDescriptor,
																				  StageInMetalLib);
			check(bStageInCreationSuccessful)
			
			// Store bytecode for lib/stagein function creation.
			size_t MetallibSize = IRMetalLibGetBytecodeSize(StageInMetalLib);
			State->StageInFunctionBytecode.SetNum(MetallibSize);
			size_t WrittenBytes = IRMetalLibGetBytecode(StageInMetalLib, reinterpret_cast<uint8_t*>(State->StageInFunctionBytecode.GetData()));
			check(MetallibSize == WrittenBytes);
			
			IRMetalLibBinaryDestroy(StageInMetalLib);
			IRShaderReflectionDestroy(VertexReflection);
		}
	}
#endif

    if(!State->Compile())
    {
        // Compilation failures are propagated up to the caller.
        State->Delete();
        return nullptr;
    }

    State->VertexDeclaration = ResourceCast(Initializer.BoundShaderState.VertexDeclarationRHI);
#if PLATFORM_SUPPORTS_MESH_SHADERS
    State->MeshShader = ResourceCast(Initializer.BoundShaderState.GetMeshShader());
    State->AmplificationShader = ResourceCast(Initializer.BoundShaderState.GetAmplificationShader());
#endif
    State->VertexShader = ResourceCast(Initializer.BoundShaderState.VertexShaderRHI);
    State->PixelShader = ResourceCast(Initializer.BoundShaderState.PixelShaderRHI);
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    State->GeometryShader = ResourceCast(Initializer.BoundShaderState.GetGeometryShader());
#endif // PLATFORM_SUPPORTS_GEOMETRY_SHADERS

    State->DepthStencilState = ResourceCast(Initializer.DepthStencilState);
    State->RasterizerState = ResourceCast(Initializer.RasterizerState);

    return State;
}

TRefCountPtr<FRHIComputePipelineState> FMetalDynamicRHI::RHICreateComputePipelineState(FRHIComputeShader* ComputeShader)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    return new FMetalComputePipelineState(ResourceCast(ComputeShader));
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
    MTL_SCOPED_AUTORELEASE_POOL;
    
    FRenderQueryRHIRef Query = new FMetalRHIRenderQuery(QueryType);
	return Query;
}

bool FMetalDynamicRHI::RHIGetRenderQueryResult(FRHIRenderQuery* QueryRHI, uint64& OutNumPixels, bool bWait, uint32 GPUIndex)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	check(IsInRenderingThread());
	FMetalRHIRenderQuery* Query = ResourceCast(QueryRHI);
	return Query->GetResult(OutNumPixels, bWait, GPUIndex);
}
