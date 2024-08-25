// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "MetalFrameAllocator.h"

FUniformBufferRHIRef FMetalDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation)
{
	return new FMetalUniformBuffer(Contents, Layout, Usage, Validation);
}

void FMetalDynamicRHI::RHIUpdateUniformBuffer(FRHICommandListBase& RHICmdList, FRHIUniformBuffer* UniformBufferRHI, const void* Contents)
{
	FMetalUniformBuffer* UniformBuffer = ResourceCast(UniformBufferRHI);

	const void* SrcContents = Contents;

	if (RHICmdList.IsTopOfPipe())
	{
		const FRHIUniformBufferLayout& Layout = UniformBuffer->GetLayout();

		// Copy the contents memory region into the RHICmdList to allow deferred execution on the RHI thread.
		void* DstContents = RHICmdList.Alloc(Layout.ConstantBufferSize, alignof(FRHIResource*));
		FMemory::ParallelMemcpy(DstContents, Contents, Layout.ConstantBufferSize, EMemcpyCachePolicy::StoreUncached);

		SrcContents = DstContents;
	}

	RHICmdList.EnqueueLambda([UniformBuffer, SrcContents](FRHICommandListBase& RHICmdList)
	{
		UniformBuffer->Update(SrcContents);
	});
}

template<EMetalShaderStages Stage, typename RHIShaderType>
static void SetUniformBufferInternal(FMetalContext* Context, RHIShaderType* ShaderRHI, uint32 BufferIndex, FRHIUniformBuffer* UBRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;

    auto Shader = ResourceCast(ShaderRHI);
    Context->GetCurrentState().BindUniformBuffer(Stage, BufferIndex, UBRHI);
        
    auto& Bindings = Shader->Bindings;
    if((Bindings.ConstantBuffers) & (1 << BufferIndex))
    {
        FMetalUniformBuffer* UB = ResourceCast(UBRHI);
        UB->PrepareToBind();
#if METAL_USE_METAL_SHADER_CONVERTER
		if(IsMetalBindlessEnabled())
		{
			Context->GetCurrentState().IRBindUniformBuffer(Stage, BufferIndex, UB);
		}
		else
#endif
		{
			FMetalBufferPtr Buf = FMetalBufferPtr(new FMetalBuffer(UB->Backing));
			Context->GetCurrentState().SetShaderBuffer(Stage, Buf, nullptr, UB->Offset, UB->GetSize(), BufferIndex, MTL::ResourceUsageRead);
		}
    }
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FRHIGraphicsShader* Shader, uint32 BufferIndex, FRHIUniformBuffer* Buffer)
{
	switch (Shader->GetFrequency())
	{
		case SF_Vertex:
			SetUniformBufferInternal<EMetalShaderStages::Vertex, FRHIVertexShader>(Context, static_cast<FRHIVertexShader*>(Shader), BufferIndex, Buffer);
			break;

		case SF_Geometry:
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
            SetUniformBufferInternal<EMetalShaderStages::Geometry, FRHIGeometryShader>(Context, static_cast<FRHIGeometryShader*>(Shader), BufferIndex, Buffer);
#else
			NOT_SUPPORTED("RHISetShaderUniformBuffer-Geometry");
#endif
			break;

		case SF_Pixel:
			SetUniformBufferInternal<EMetalShaderStages::Pixel, FRHIPixelShader>(Context, static_cast<FRHIPixelShader*>(Shader), BufferIndex, Buffer);
			break;

#if PLATFORM_SUPPORTS_MESH_SHADERS
        case SF_Mesh:
            SetUniformBufferInternal<EMetalShaderStages::Mesh, FRHIMeshShader>(Context, static_cast<FRHIMeshShader*>(Shader), BufferIndex, Buffer);
            break;

        case SF_Amplification:
            SetUniformBufferInternal<EMetalShaderStages::Amplification, FRHIAmplificationShader>(Context, static_cast<FRHIAmplificationShader*>(Shader), BufferIndex, Buffer);
            break;
#endif

		default:
			checkf(0, TEXT("FRHIShader Type %d is invalid or unsupported!"), (int32)Shader->GetFrequency());
			NOT_SUPPORTED("RHIShaderStage");
			break;
	}
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FRHIComputeShader* ComputeShaderRHI, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	SetUniformBufferInternal<EMetalShaderStages::Compute, FRHIComputeShader>(Context, ComputeShaderRHI, BufferIndex, BufferRHI);
}
