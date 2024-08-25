// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalComputeShader.cpp: Metal RHI Compute Shader Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "Templates/MetalBaseShader.h"
#include "MetalComputeShader.h"
#include "MetalCommandBuffer.h"

//------------------------------------------------------------------------------

#pragma mark - Metal RHI Compute Shader Class


FMetalComputeShader::FMetalComputeShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
	: NumThreadsX(0)
	, NumThreadsY(0)
	, NumThreadsZ(0)
{
	Pipeline = nullptr;
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);

#if METAL_RHI_RAYTRACING
	RayTracingBindings = Header.RayTracing;
#endif // METAL_RHI_RAYTRACING

	NumThreadsX = FMath::Max((int32)Header.NumThreadsX, 1);
	NumThreadsY = FMath::Max((int32)Header.NumThreadsY, 1);
	NumThreadsZ = FMath::Max((int32)Header.NumThreadsZ, 1);
}

FMetalComputeShader::~FMetalComputeShader()
{
    if(Pipeline)
    {
        Pipeline = nullptr;
    }
}

FMetalShaderPipelinePtr FMetalComputeShader::GetPipeline()
{
	if (!Pipeline)
	{
        MTL_SCOPED_AUTORELEASE_POOL;
        
        MTLFunctionPtr Func = GetCompiledFunction();
		check(Func);

		NS::Error* Error;
		MTL::ComputePipelineDescriptor* Descriptor = MTL::ComputePipelineDescriptor::alloc()->init();
        check(Descriptor);
        
		Descriptor->setLabel(Func->name());
		Descriptor->setComputeFunction(Func.get());
        
		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesTextureBuffers))
		{
			Descriptor->setMaxTotalThreadsPerThreadgroup(NumThreadsX*NumThreadsY*NumThreadsZ);
		}

		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesPipelineBufferMutability))
		{
			MTL::PipelineBufferDescriptorArray* PipelineBuffers = Descriptor->buffers();

			uint32 ImmutableBuffers = Bindings.ConstantBuffers | Bindings.ArgumentBuffers;
			while(ImmutableBuffers)
			{
				uint32 Index = __builtin_ctz(ImmutableBuffers);
				ImmutableBuffers &= ~(1 << Index);

				if (Index < ML_MaxBuffers)
				{
					MTL::PipelineBufferDescriptor* PipelineBuffer = PipelineBuffers->object(Index);
					PipelineBuffer->setMutability(MTL::MutabilityImmutable);
				}
			}
			if (SideTableBinding > 0)
			{
				MTL::PipelineBufferDescriptor* PipelineBuffer = PipelineBuffers->object(SideTableBinding);
				PipelineBuffer->setMutability(MTL::MutabilityImmutable);
			}
		}

        MTLComputePipelineStatePtr Kernel;
		MTL::ComputePipelineReflection* Reflection = nullptr;

		METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewComputePipeline: %d_%d"), SourceLen, SourceCRC)));
#if METAL_DEBUG_OPTIONS
		if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
		{
			NS::Error* ComputeError = nullptr;
            MTL::ComputePipelineReflection* ComputeReflection = nullptr;
            
			NS::UInteger ComputeOption = MTL::PipelineOptionArgumentInfo | MTL::PipelineOptionBufferTypeInfo;
			Kernel = NS::TransferPtr(GetMetalDeviceContext().GetDevice()->newComputePipelineState(Descriptor, MTL::PipelineOption(ComputeOption), &ComputeReflection, &ComputeError));
			Error = ComputeError;
			Reflection = ComputeReflection;
		}
		else
#endif // METAL_DEBUG_OPTIONS
		{
			NS::Error* ComputeError;
			Kernel = NS::TransferPtr(GetMetalDeviceContext().GetDevice()->newComputePipelineState(Descriptor, MTL::PipelineOption(0), nullptr, &ComputeError));
			Error = ComputeError;
		}

		if (Kernel.get() == nullptr)
		{
			UE_LOG(LogRHI, Error, TEXT("*********** Error\n%s"), *NSStringToFString(GetSourceCode()));
			UE_LOG(LogRHI, Fatal, TEXT("Failed to create compute kernel: %s"), *NSStringToFString(Error->description()));
		}

		Pipeline = FMetalShaderPipelinePtr(new FMetalShaderPipeline);
		Pipeline->ComputePipelineState = Kernel;
#if METAL_DEBUG_OPTIONS
        if(Reflection)
        {
            Pipeline->ComputePipelineReflection = NS::RetainPtr(Reflection);
        }
        
		Pipeline->ComputeSource = GetSourceCode();
        Pipeline->ComputeSource->retain();
        
		if (Reflection)
		{
			Pipeline->ComputeDesc = NS::RetainPtr(Descriptor);
		}
#endif // METAL_DEBUG_OPTIONS
        Descriptor->release();
        
		METAL_DEBUG_OPTION(FMemory::Memzero(Pipeline->ResourceMask, sizeof(Pipeline->ResourceMask)));
	}
	check(Pipeline);

	return FMetalShaderPipelinePtr(Pipeline);
}

MTLFunctionPtr FMetalComputeShader::GetFunction()
{
	return GetCompiledFunction();
}

