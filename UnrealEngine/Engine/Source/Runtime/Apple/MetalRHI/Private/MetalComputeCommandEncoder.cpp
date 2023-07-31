// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalComputeCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalComputeCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalFence.h"
#include "MetalPipeline.h"

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
NS_ASSUME_NONNULL_BEGIN

#if METAL_DEBUG_OPTIONS
static NSString* GMetalDebugComputeShader = @"#include <metal_stdlib>\n"
@"using namespace metal;\n"
@"kernel void WriteCommandIndexCS(constant uint* Input [[ buffer(0) ]], device atomic_uint* Output [[ buffer(1) ]])\n"
@"{\n"
@"	atomic_store_explicit(Output, Input[0], memory_order_relaxed);\n"
@"}\n";

static id <MTLComputePipelineState> GetDebugComputeShaderState(id<MTLDevice> Device)
{
	static id<MTLComputePipelineState> State = nil;
	if (!State)
	{
		id<MTLLibrary> Lib = [Device newLibraryWithSource:GMetalDebugComputeShader options:nil error:nullptr];
		check(Lib);
		id<MTLFunction> Func = [Lib newFunctionWithName:@"WriteCommandIndexCS"];
		check(Func);
		State = [Device newComputePipelineStateWithFunction:Func error:nil];
		[Func release];
		[Lib release];
	}
	check(State);
	return State;
}
#endif

@interface FMetalDebugComputeCommandEncoder : FMetalDebugCommandEncoder
{
@public
	id<MTLComputeCommandEncoder> Inner;
	FMetalCommandBufferDebugging Buffer;
	FMetalShaderPipeline* Pipeline;

#pragma mark - Private Member Variables -
#if METAL_DEBUG_OPTIONS
	FMetalDebugShaderResourceMask ResourceMask;
    FMetalDebugBufferBindings ShaderBuffers;
    FMetalDebugTextureBindings ShaderTextures;
    FMetalDebugSamplerBindings ShaderSamplers;
#endif
}
/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithEncoder:(id<MTLComputeCommandEncoder>)Encoder andCommandBuffer:(FMetalCommandBufferDebugging const&)Buffer;
@end

@implementation FMetalDebugComputeCommandEncoder

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugComputeCommandEncoder)

-(id)initWithEncoder:(id<MTLComputeCommandEncoder>)Encoder andCommandBuffer:(FMetalCommandBufferDebugging const&)SourceBuffer
{
	id Self = [super init];
	if (Self)
	{
        Inner = Encoder;
		Buffer = SourceBuffer;
        Pipeline = nil;
	}
	return Self;
}

-(void)dealloc
{
	[Pipeline release];
	[super dealloc];
}

@end

#if METAL_DEBUG_OPTIONS
void FMetalComputeCommandEncoderDebugging::InsertDebugDispatch()
{
//	switch (Buffer->DebugLevel)
//	{
//		case EMetalDebugLevelConditionalSubmit:
//		case EMetalDebugLevelWaitForComplete:
//		case EMetalDebugLevelLogOperations:
//		case EMetalDebugLevelValidation:
//		{
//			uint32 const Index = Buffer->DebugCommands.Num();
//			[Inner setBytes:&Index length:sizeof(Index) atIndex:0];
//			[Inner setBuffer:Buffer->DebugInfoBuffer offset:0 atIndex:1];
//			[Inner setComputePipelineState:GetDebugComputeShaderState(Inner.device)];
//			
//			[Inner dispatchThreadgroups:mtlpp::Size const&Make(1, 1, 1) threadsPerThreadgroup:mtlpp::Size const&Make(1, 1, 1)];
//
//			if (Pipeline && Pipeline->ComputePipelineState)
//			{
//				[Inner setComputePipelineState:Pipeline->ComputePipelineState];
//			}
//			
//			if (ShaderBuffers.Buffers[0])
//			{
//				[Inner setBuffer:ShaderBuffers.Buffers[0] offset:ShaderBuffers.Offsets[0] atIndex:0];
//			}
//			else if (ShaderBuffers.Bytes[0])
//			{
//				[Inner setBytes:ShaderBuffers.Bytes[0] length:ShaderBuffers.Offsets[0] atIndex:0];
//			}
//			
//			if (ShaderBuffers.Buffers[1])
//			{
//				[Inner setBuffer:ShaderBuffers.Buffers[1] offset:ShaderBuffers.Offsets[1] atIndex:1];
//			}
//			else if (ShaderBuffers.Bytes[1])
//			{
//				[Inner setBytes:ShaderBuffers.Bytes[1] length:ShaderBuffers.Offsets[1] atIndex:1];
//			}
//		}
//		default:
//		{
//			break;
//		}
//	}
}
#endif

FMetalComputeCommandEncoderDebugging::FMetalComputeCommandEncoderDebugging()
{
	
}
FMetalComputeCommandEncoderDebugging::FMetalComputeCommandEncoderDebugging(mtlpp::ComputeCommandEncoder& Encoder, FMetalCommandBufferDebugging& Buffer)
: FMetalCommandEncoderDebugging((FMetalDebugCommandEncoder*)[[[FMetalDebugComputeCommandEncoder alloc] initWithEncoder:Encoder.GetPtr() andCommandBuffer:Buffer] autorelease])
{
	Buffer.BeginComputeCommandEncoder([NSString stringWithFormat:@"Compute: %@", Encoder.GetLabel().GetPtr()]);
	Encoder.SetAssociatedObject((void const*)&FMetalComputeCommandEncoderDebugging::Get, (FMetalCommandEncoderDebugging const&)*this);
}
FMetalComputeCommandEncoderDebugging::FMetalComputeCommandEncoderDebugging(FMetalDebugCommandEncoder* handle)
: FMetalCommandEncoderDebugging((FMetalDebugCommandEncoder*)handle)
{
	
}

FMetalComputeCommandEncoderDebugging FMetalComputeCommandEncoderDebugging::Get(mtlpp::ComputeCommandEncoder& Encoder)
{
	return Encoder.GetAssociatedObject<FMetalComputeCommandEncoderDebugging>((void const*)&FMetalComputeCommandEncoderDebugging::Get);
}

void FMetalComputeCommandEncoderDebugging::InsertDebugSignpost(ns::String const& Label)
{
	((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.InsertDebugSignpost(Label);
}
void FMetalComputeCommandEncoderDebugging::PushDebugGroup(ns::String const& Group)
{
    ((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.PushDebugGroup(Group);
}
void FMetalComputeCommandEncoderDebugging::PopDebugGroup()
{
    ((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.PopDebugGroup();
#if METAL_DEBUG_OPTIONS
	InsertDebugDispatch();
#endif
}

void FMetalComputeCommandEncoderDebugging::EndEncoder()
{
	((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.EndCommandEncoder();
}

void FMetalComputeCommandEncoderDebugging::DispatchThreadgroups(mtlpp::Size const& threadgroupsPerGrid, mtlpp::Size const& threadsPerThreadgroup)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.Dispatch([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		case EMetalDebugLevelValidation:
		{
			Validate();
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalComputeCommandEncoderDebugging::SetPipeline(FMetalShaderPipeline* Pipeline)
{
#if METAL_DEBUG_OPTIONS
	((FMetalDebugComputeCommandEncoder*)m_ptr)->Pipeline = [Pipeline retain];
	switch (((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.SetPipeline(Pipeline->ComputePipelineState.GetLabel());
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalComputeCommandEncoderDebugging::SetBytes(const void * bytes, NSUInteger length, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Buffers[index] = nil;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Bytes[index] = bytes;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Offsets[index] = length;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.BufferMask = bytes ? (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.BufferMask | (1 << (index))) : (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}
void FMetalComputeCommandEncoderDebugging::SetBuffer( FMetalBuffer const& buffer, NSUInteger offset, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Buffers[index] = buffer;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Bytes[index] = nil;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Offsets[index] = offset;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.BufferMask = buffer ? (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.BufferMask | (1 << (index))) : (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}
void FMetalComputeCommandEncoderDebugging::SetBufferOffset(NSUInteger offset, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Offsets[index] = offset;
			check(((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.BufferMask & (1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalComputeCommandEncoderDebugging::SetTexture( FMetalTexture const& texture, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderTextures.Textures[index] = texture;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.TextureMask = texture ? (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.TextureMask | (FMetalTextureMask(1) << (index))) : (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.TextureMask & ~(FMetalTextureMask(1) << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalComputeCommandEncoderDebugging::SetSamplerState( mtlpp::SamplerState const& sampler, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderSamplers.Samplers[index] = sampler;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.SamplerMask = sampler ? (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.SamplerMask | (1 << (index))) : (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalComputeCommandEncoderDebugging::SetSamplerState( mtlpp::SamplerState const& sampler, float lodMinClamp, float lodMaxClamp, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderSamplers.Samplers[index] = sampler;
			((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.SamplerMask = sampler ? (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.SamplerMask | (1 << (index))) : (((FMetalDebugComputeCommandEncoder*)m_ptr)->ResourceMask.SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalComputeCommandEncoderDebugging::DispatchThreadgroupsWithIndirectBuffer(FMetalBuffer const& indirectBuffer, NSUInteger indirectBufferOffset, mtlpp::Size const& threadsPerThreadgroup)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.Dispatch([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		case EMetalDebugLevelValidation:
		{
			Validate();
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalComputeCommandEncoderDebugging::Validate()
{
	bool bOK = true;
	
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugComputeCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			check(((FMetalDebugComputeCommandEncoder*)m_ptr)->Pipeline);
			
			MTLComputePipelineReflection* Reflection = ((FMetalDebugComputeCommandEncoder*)m_ptr)->Pipeline->ComputePipelineReflection;
			check(Reflection);
	
			NSArray<MTLArgument*>* Arguments = Reflection.arguments;
			for (uint32 i = 0; i < Arguments.count; i++)
			{
				MTLArgument* Arg = [Arguments objectAtIndex:i];
				check(Arg);
				switch(Arg.type)
				{
					case MTLArgumentTypeBuffer:
					{
						checkf(Arg.index < ML_MaxBuffers, TEXT("Metal buffer index exceeded!"));
						if ((((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Buffers[Arg.index] == nil && ((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderBuffers.Bytes[Arg.index] == nil))
						{
							UE_LOG(LogMetal, Warning, TEXT("Unbound buffer at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
							bOK = false;
						}
						break;
					}
					case MTLArgumentTypeThreadgroupMemory:
					{
						break;
					}
					case MTLArgumentTypeTexture:
					{
						checkf(Arg.index < ML_MaxTextures, TEXT("Metal texture index exceeded!"));
						if (((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderTextures.Textures[Arg.index] == nil)
						{
							UE_LOG(LogMetal, Warning, TEXT("Unbound texture at Metal index %u  which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
							bOK = false;
						}
						else if (((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderTextures.Textures[Arg.index].textureType != Arg.textureType)
						{
							UE_LOG(LogMetal, Warning, TEXT("Incorrect texture type bound at Metal index %u which will crash the driver: %s\n%s"), (uint32)Arg.index, *FString([Arg description]), *FString([((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderTextures.Textures[Arg.index] description]));
							bOK = false;
						}
						break;
					}
					case MTLArgumentTypeSampler:
					{
						checkf(Arg.index < ML_MaxSamplers, TEXT("Metal sampler index exceeded!"));
						if (((FMetalDebugComputeCommandEncoder*)m_ptr)->ShaderSamplers.Samplers[Arg.index] == nil)
						{
							UE_LOG(LogMetal, Warning, TEXT("Unbound sampler at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
							bOK = false;
						}
						break;
					}
					default:
						check(false);
						break;
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
	
    if (!bOK)
    {
        UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for compute shader:\n%s"), (((FMetalDebugComputeCommandEncoder*)m_ptr)->Pipeline && ((FMetalDebugComputeCommandEncoder*)m_ptr)->Pipeline->ComputeSource) ? *FString(((FMetalDebugComputeCommandEncoder*)m_ptr)->Pipeline->ComputeSource) : TEXT("nil"));
    }
#endif
}

NS_ASSUME_NONNULL_END
#endif
