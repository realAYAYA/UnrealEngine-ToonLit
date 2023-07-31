// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRenderCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalRenderCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalFence.h"
#include "MetalPipeline.h"

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
NS_ASSUME_NONNULL_BEGIN

@interface FMetalDebugRenderCommandEncoder : FMetalDebugCommandEncoder
{
@public
#pragma mark - Private Member Variables -
#if METAL_DEBUG_OPTIONS
	FMetalDebugShaderResourceMask ResourceMask[EMetalShaderRenderNum];
	FMetalDebugBufferBindings ShaderBuffers[EMetalShaderRenderNum];
	FMetalDebugTextureBindings ShaderTextures[EMetalShaderRenderNum];
	FMetalDebugSamplerBindings ShaderSamplers[EMetalShaderRenderNum];
	id<MTLRenderPipelineState> DebugState;
#endif
	MTLRenderPassDescriptor* RenderPassDesc;
	id<MTLRenderCommandEncoder> Inner;
	FMetalCommandBufferDebugging Buffer;
	FMetalShaderPipeline* Pipeline;
}

/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithEncoder:(id<MTLRenderCommandEncoder>)Encoder fromDescriptor:(MTLRenderPassDescriptor*)RenderPassDesc andCommandBuffer:(FMetalCommandBufferDebugging const&)Buffer;

@end

@interface FMetalDebugParallelRenderCommandEncoder : FApplePlatformObject
{
@public
	TArray<FMetalRenderCommandEncoderDebugging> RenderEncoders;
	MTLRenderPassDescriptor* RenderPassDesc;
	id<MTLParallelRenderCommandEncoder> Inner;
	FMetalCommandBufferDebugging Buffer;
}
-(id)initWithEncoder:(id<MTLParallelRenderCommandEncoder>)Encoder fromDescriptor:(MTLRenderPassDescriptor*)RenderPassDesc andCommandBuffer:(FMetalCommandBufferDebugging const&)Buffer;
@end


#if METAL_DEBUG_OPTIONS
static NSString* GMetalDebugVertexShader = @"#include <metal_stdlib>\n"
@"using namespace metal;\n"
@"struct VertexInput\n"
@"{\n"
@"};\n"
@"vertex void WriteCommandIndexVS(VertexInput StageIn [[stage_in]], constant uint* Input [[ buffer(0) ]], device uint* Output  [[ buffer(1) ]])\n"
@"{\n"
@"	Output[0] = Input[0];\n"
@"}\n";

static id <MTLRenderPipelineState> GetDebugVertexShaderState(id<MTLDevice> Device, MTLRenderPassDescriptor* PassDesc)
{
	static id<MTLFunction> Func = nil;
	static FCriticalSection Mutex;
	static NSMutableDictionary* Dict = [NSMutableDictionary new];
	if(!Func)
	{
		id<MTLLibrary> Lib = [Device newLibraryWithSource:GMetalDebugVertexShader options:nil error:nullptr];
		check(Lib);
		Func = [Lib newFunctionWithName:@"WriteCommandIndexVS"];
		check(Func);
		[Lib release];
	}
	
	FScopeLock Lock(&Mutex);
	id<MTLRenderPipelineState> State = [Dict objectForKey:PassDesc];
	if (!State)
	{
		MTLRenderPipelineDescriptor* Desc = [MTLRenderPipelineDescriptor new];
		
		Desc.vertexFunction = Func;
		
		if (PassDesc.depthAttachment)
		{
			Desc.depthAttachmentPixelFormat = PassDesc.depthAttachment.texture.pixelFormat;
		}
		if (PassDesc.stencilAttachment)
		{
			Desc.stencilAttachmentPixelFormat = PassDesc.stencilAttachment.texture.pixelFormat;
		}
		if (PassDesc.colorAttachments)
		{
			for (NSUInteger i = 0; i < 8; i++)
			{
				MTLRenderPassColorAttachmentDescriptor* CD = [PassDesc.colorAttachments objectAtIndexedSubscript:i];
				if (CD.texture.pixelFormat != MTLPixelFormatInvalid)
				{
					MTLRenderPipelineColorAttachmentDescriptor* CD0 = [[MTLRenderPipelineColorAttachmentDescriptor new] autorelease];
					CD0.pixelFormat = CD.texture.pixelFormat;
					[Desc.colorAttachments setObject:CD0 atIndexedSubscript:i];
				}
			}
		}
		Desc.rasterizationEnabled = false;
		
		State = [[Device newRenderPipelineStateWithDescriptor:Desc error:nil] autorelease];
		check(State);
		
		[Dict setObject:State forKey:PassDesc];
		
		[Desc release];
	}
	check(State);
	return State;
}
#endif

@implementation FMetalDebugParallelRenderCommandEncoder

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugParallelRenderCommandEncoder)

-(id)initWithEncoder:(id<MTLParallelRenderCommandEncoder>)Encoder fromDescriptor:(MTLRenderPassDescriptor*)Desc andCommandBuffer:(FMetalCommandBufferDebugging const&)SourceBuffer
{
	id Self = [super init];
	if (Self)
	{
        Inner = Encoder;
		Buffer = SourceBuffer;
		RenderPassDesc = [Desc retain];
	}
	return Self;
}

-(void)dealloc
{
	[RenderPassDesc release];
	[super dealloc];
}

@end

@implementation FMetalDebugRenderCommandEncoder

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugRenderCommandEncoder)

-(id)initWithEncoder:(id<MTLRenderCommandEncoder>)Encoder fromDescriptor:(MTLRenderPassDescriptor*)Desc andCommandBuffer:(FMetalCommandBufferDebugging const&)SourceBuffer
{
	id Self = [super init];
	if (Self)
	{
        Inner = Encoder;
		Buffer = SourceBuffer;
		RenderPassDesc = [Desc retain];
#if METAL_DEBUG_OPTIONS
		DebugState = Buffer.GetPtr()->DebugLevel >= EMetalDebugLevelValidation ? [GetDebugVertexShaderState(Buffer.GetPtr()->InnerBuffer.device, Desc) retain] : nil;
#endif
        Pipeline = nil;
	}
	return Self;
}

-(void)dealloc
{
	[RenderPassDesc release];
#if METAL_DEBUG_OPTIONS
	[DebugState release];
#endif
	[Pipeline release];
	[super dealloc];
}

@end

NS_ASSUME_NONNULL_END

void FMetalRenderCommandEncoderDebugging::InsertDebugDraw()
{
#if METAL_DEBUG_OPTIONS
//	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
//	{
//		case EMetalDebugLevelConditionalSubmit:
//		case EMetalDebugLevelWaitForComplete:
//		case EMetalDebugLevelLogOperations:
//		case EMetalDebugLevelValidation:
//		{
//			uint32 const Index = Buffer->DebugCommands.Num();
//#if PLATFORM_MAC
//			[Inner textureBarrier];
//#endif
//			[Inner setVertexBytes:&Index length:sizeof(Index) atIndex:0];
//			[Inner setVertexBuffer:Buffer->DebugInfoBuffer offset:0 atIndex:1];
//			[Inner setRenderPipelineState:DebugState];
//			[Inner drawPrimitives:mtlpp::PrimitiveTypePoint vertexStart:0 vertexCount:1];
//#if PLATFORM_MAC
//			[Inner textureBarrier];
//#endif
//			if (Pipeline && Pipeline->RenderPipelineState)
//			{
//				[Inner setRenderPipelineState:Pipeline->RenderPipelineState];
//			}
//			
//			if (ShaderBuffers[EMetalShaderVertex].Buffers[0])
//			{
//				[Inner setVertexBuffer:ShaderBuffers[EMetalShaderVertex].Buffers[0] offset:ShaderBuffers[EMetalShaderVertex].Offsets[0] atIndex:0];
//			}
//			else if (ShaderBuffers[EMetalShaderVertex].Bytes[0])
//			{
//				[Inner setVertexBytes:ShaderBuffers[EMetalShaderVertex].Bytes[0] length:ShaderBuffers[EMetalShaderVertex].Offsets[0] atIndex:0];
//			}
//			
//			if (ShaderBuffers[EMetalShaderVertex].Buffers[1])
//			{
//				[Inner setVertexBuffer:ShaderBuffers[EMetalShaderVertex].Buffers[1] offset:ShaderBuffers[EMetalShaderVertex].Offsets[1] atIndex:1];
//			}
//			else if (ShaderBuffers[EMetalShaderVertex].Bytes[1])
//			{
//				[Inner setVertexBytes:ShaderBuffers[EMetalShaderVertex].Bytes[1] length:ShaderBuffers[EMetalShaderVertex].Offsets[1] atIndex:1];
//			}
//		}
//		default:
//		{
//			break;
//		}
//	}
#endif
}

FMetalRenderCommandEncoderDebugging::FMetalRenderCommandEncoderDebugging()
{
	
}
FMetalRenderCommandEncoderDebugging::FMetalRenderCommandEncoderDebugging(mtlpp::RenderCommandEncoder& Encoder, mtlpp::RenderPassDescriptor const& Desc, FMetalCommandBufferDebugging& Buffer)
: FMetalCommandEncoderDebugging((FMetalDebugCommandEncoder*)[[[FMetalDebugRenderCommandEncoder alloc] initWithEncoder:Encoder.GetPtr() fromDescriptor:Desc.GetPtr() andCommandBuffer:Buffer] autorelease])
{
	Buffer.BeginRenderCommandEncoder([NSString stringWithFormat:@"Render: %@", Encoder.GetLabel().GetPtr()], Desc);
	Encoder.SetAssociatedObject((void const*)&FMetalRenderCommandEncoderDebugging::Get, (FMetalCommandEncoderDebugging const&)*this);
}
FMetalRenderCommandEncoderDebugging::FMetalRenderCommandEncoderDebugging(FMetalDebugCommandEncoder* handle)
: FMetalCommandEncoderDebugging(handle)
{
	
}

FMetalRenderCommandEncoderDebugging FMetalRenderCommandEncoderDebugging::Get(mtlpp::RenderCommandEncoder& Encoder)
{
	return Encoder.GetAssociatedObject<FMetalRenderCommandEncoderDebugging>((void const*)&FMetalRenderCommandEncoderDebugging::Get);
}

void FMetalRenderCommandEncoderDebugging::SetPipeline(FMetalShaderPipeline* Pipeline)
{
#if METAL_DEBUG_OPTIONS
	((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline = [Pipeline retain];
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.SetPipeline(Pipeline->RenderPipelineState.GetLabel());
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalRenderCommandEncoderDebugging::SetBytes(EMetalShaderFrequency Freq, const void * bytes, NSUInteger length, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Freq].Buffers[index] = nil;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Freq].Bytes[index] = bytes;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Freq].Offsets[index] = length;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].BufferMask = bytes ? (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].BufferMask | (1 << (index))) : (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}
void FMetalRenderCommandEncoderDebugging::SetBuffer(EMetalShaderFrequency Freq,  FMetalBuffer const& buffer, NSUInteger offset, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Freq].Buffers[index] = buffer;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Freq].Bytes[index] = nil;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Freq].Offsets[index] = offset;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].BufferMask = buffer ? (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].BufferMask | (1 << (index))) : (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}
void FMetalRenderCommandEncoderDebugging::SetBufferOffset(EMetalShaderFrequency Freq, NSUInteger offset, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Freq].Offsets[index] = offset;
			check(((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].BufferMask & (1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalRenderCommandEncoderDebugging::SetTexture(EMetalShaderFrequency Freq,  FMetalTexture const& texture, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderTextures[Freq].Textures[index] = texture;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].TextureMask = texture ? (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].TextureMask | (FMetalTextureMask(1) << index)) : (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].TextureMask & ~(FMetalTextureMask(1) << index));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalRenderCommandEncoderDebugging::SetSamplerState(EMetalShaderFrequency Freq,  mtlpp::SamplerState const& sampler, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderSamplers[Freq].Samplers[index] = sampler;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].SamplerMask = sampler ? (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].SamplerMask | (1 << (index))) : (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalRenderCommandEncoderDebugging::SetSamplerState(EMetalShaderFrequency Freq,  mtlpp::SamplerState const& sampler, float lodMinClamp, float lodMaxClamp, NSUInteger index)
{
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderSamplers[Freq].Samplers[index] = sampler;
			((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].SamplerMask = sampler ? (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].SamplerMask | (1 << (index))) : (((FMetalDebugRenderCommandEncoder*)m_ptr)->ResourceMask[Freq].SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalRenderCommandEncoderDebugging::SetDepthStencilState( mtlpp::DepthStencilState const& depthStencilState)
{
}

void FMetalRenderCommandEncoderDebugging::Draw(mtlpp::PrimitiveType primitiveType, NSUInteger vertexStart, NSUInteger vertexCount, NSUInteger instanceCount)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
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

void FMetalRenderCommandEncoderDebugging::Draw(mtlpp::PrimitiveType primitiveType, NSUInteger vertexStart, NSUInteger vertexCount)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
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

void FMetalRenderCommandEncoderDebugging::DrawIndexed(mtlpp::PrimitiveType primitiveType, NSUInteger indexCount, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset, NSUInteger instanceCount)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
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

void FMetalRenderCommandEncoderDebugging::DrawIndexed(mtlpp::PrimitiveType primitiveType, NSUInteger indexCount, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
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

void FMetalRenderCommandEncoderDebugging::Draw(mtlpp::PrimitiveType primitiveType, NSUInteger vertexStart, NSUInteger vertexCount, NSUInteger instanceCount, NSUInteger baseInstance)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s:%u,%u,%u,%u,%u", __PRETTY_FUNCTION__, (uint32)primitiveType, (uint32)vertexStart, (uint32)vertexCount, (uint32)instanceCount, (uint32)baseInstance]);
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

void FMetalRenderCommandEncoderDebugging::DrawIndexed(mtlpp::PrimitiveType primitiveType, NSUInteger indexCount, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset, NSUInteger instanceCount, NSInteger baseVertex, NSUInteger baseInstance)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s:%u,%u,%u,%u,%u,%u,%u", __PRETTY_FUNCTION__, (uint32)primitiveType, (uint32)indexCount, (uint32)indexType, (uint32)indexBufferOffset, (uint32)instanceCount, (uint32)baseVertex, (uint32)baseInstance]);
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

void FMetalRenderCommandEncoderDebugging::Draw(mtlpp::PrimitiveType primitiveType, FMetalBuffer const& indirectBuffer, NSUInteger indirectBufferOffset)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
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

void FMetalRenderCommandEncoderDebugging::DrawIndexed(mtlpp::PrimitiveType primitiveType, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset, FMetalBuffer const& indirectBuffer, NSUInteger indirectBufferOffset)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.Draw([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
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

/** Validates the pipeline/binding state */
bool FMetalRenderCommandEncoderDebugging::ValidateFunctionBindings(EMetalShaderFrequency Frequency)
{
	bool bOK = true;
#if METAL_DEBUG_OPTIONS
	switch (((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			check(((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline);
			
			MTLRenderPipelineReflection* Reflection = ((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline->RenderPipelineReflection;
			check(Reflection);
			
			NSArray<MTLArgument*>* Arguments = nil;
			switch(Frequency)
			{
				case EMetalShaderVertex:
				{
					Arguments = Reflection.vertexArguments;
					break;
				}
				case EMetalShaderFragment:
				{
					Arguments = Reflection.fragmentArguments;
					break;
				}
				default:
					check(false);
					break;
			}
			
			for (uint32 i = 0; i < Arguments.count; i++)
			{
				MTLArgument* Arg = [Arguments objectAtIndex:i];
				check(Arg);
				switch(Arg.type)
				{
					case MTLArgumentTypeBuffer:
					{
						checkf(Arg.index < ML_MaxBuffers, TEXT("Metal buffer index exceeded!"));
						if ((((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Frequency].Buffers[Arg.index] == nil && ((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderBuffers[Frequency].Bytes[Arg.index] == nil))
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Unbound buffer at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
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
						if (((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderTextures[Frequency].Textures[Arg.index] == nil)
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Unbound texture at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
						}
						else if (((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderTextures[Frequency].Textures[Arg.index].textureType != Arg.textureType)
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Incorrect texture type bound at Metal index %u which will crash the driver: %s\n%s"), (uint32)Arg.index, *FString([Arg description]), *FString([((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderTextures[Frequency].Textures[Arg.index] description]));
						}
						break;
					}
					case MTLArgumentTypeSampler:
					{
						checkf(Arg.index < ML_MaxSamplers, TEXT("Metal sampler index exceeded!"));
						if (((FMetalDebugRenderCommandEncoder*)m_ptr)->ShaderSamplers[Frequency].Samplers[Arg.index] == nil)
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Unbound sampler at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
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
#endif
	return bOK;
}
void FMetalRenderCommandEncoderDebugging::Validate()
{
#if METAL_DEBUG_OPTIONS
	bool bOK = ValidateFunctionBindings(EMetalShaderVertex);
	if (!bOK)
	{
		UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for vertex shader:\n%s"), ((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline && ((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline->VertexSource ? *FString(((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline->VertexSource) : TEXT("nil"));
	}
	
	bOK = ValidateFunctionBindings(EMetalShaderFragment);
	if (!bOK)
	{
		UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for fragment shader:\n%s"), ((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline && ((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline->FragmentSource ? *FString(((FMetalDebugRenderCommandEncoder*)m_ptr)->Pipeline->FragmentSource) : TEXT("nil"));
	}
#endif
}

void FMetalRenderCommandEncoderDebugging::InsertDebugSignpost(ns::String const& Label)
{
	((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.InsertDebugSignpost(Label);
}
void FMetalRenderCommandEncoderDebugging::PushDebugGroup(ns::String const& Group)
{
    ((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.PushDebugGroup(Group);
}
void FMetalRenderCommandEncoderDebugging::PopDebugGroup()
{
    ((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.PopDebugGroup();
#if METAL_DEBUG_OPTIONS
	InsertDebugDraw();
#endif
}

void FMetalRenderCommandEncoderDebugging::EndEncoder()
{
	((FMetalDebugRenderCommandEncoder*)m_ptr)->Buffer.EndCommandEncoder();
}

FMetalParallelRenderCommandEncoderDebugging::FMetalParallelRenderCommandEncoderDebugging()
{

}

FMetalParallelRenderCommandEncoderDebugging::FMetalParallelRenderCommandEncoderDebugging(mtlpp::ParallelRenderCommandEncoder& Encoder, mtlpp::RenderPassDescriptor const& Desc, FMetalCommandBufferDebugging& Buffer)
: ns::Object<FMetalDebugParallelRenderCommandEncoder*>([[FMetalDebugParallelRenderCommandEncoder alloc] initWithEncoder:Encoder.GetPtr() fromDescriptor:Desc.GetPtr() andCommandBuffer:Buffer], ns::Ownership::Assign)
{
	Buffer.BeginRenderCommandEncoder([NSString stringWithFormat:@"ParallelRender: %@", Encoder.GetLabel().GetPtr()], Desc);
	Encoder.SetAssociatedObject((void const*)&FMetalParallelRenderCommandEncoderDebugging::Get, *this);
}

FMetalParallelRenderCommandEncoderDebugging::FMetalParallelRenderCommandEncoderDebugging(FMetalDebugParallelRenderCommandEncoder* handle)
: ns::Object<FMetalDebugParallelRenderCommandEncoder*>(handle)
{

}

FMetalDebugParallelRenderCommandEncoder* FMetalParallelRenderCommandEncoderDebugging::Get(mtlpp::ParallelRenderCommandEncoder& Buffer)
{
	return Buffer.GetAssociatedObject<FMetalParallelRenderCommandEncoderDebugging>((void const*)&FMetalParallelRenderCommandEncoderDebugging::Get);
}

FMetalRenderCommandEncoderDebugging FMetalParallelRenderCommandEncoderDebugging::GetRenderCommandEncoderDebugger(mtlpp::RenderCommandEncoder& Encoder)
{
	mtlpp::RenderPassDescriptor Desc(m_ptr->RenderPassDesc);
	FMetalCommandBufferDebugging IndirectBuffer([[FMetalDebugCommandBuffer alloc] initWithCommandBuffer:m_ptr->Buffer.GetPtr()->InnerBuffer]);
	FMetalRenderCommandEncoderDebugging EncoderDebugging(Encoder, Desc, IndirectBuffer);
	m_ptr->RenderEncoders.Add(EncoderDebugging);
	return EncoderDebugging;
}

void FMetalParallelRenderCommandEncoderDebugging::InsertDebugSignpost(ns::String const& Label)
{
	m_ptr->Buffer.InsertDebugSignpost(Label);
}

void FMetalParallelRenderCommandEncoderDebugging::PushDebugGroup(ns::String const& Group)
{
    m_ptr->Buffer.PushDebugGroup(Group);
}

void FMetalParallelRenderCommandEncoderDebugging::PopDebugGroup()
{
    m_ptr->Buffer.PopDebugGroup();
}

void FMetalParallelRenderCommandEncoderDebugging::EndEncoder()
{
	FMetalDebugCommandBuffer* CommandBuffer = m_ptr->Buffer.GetPtr();
	for (FMetalRenderCommandEncoderDebugging& EncoderDebug : m_ptr->RenderEncoders)
	{
		FMetalDebugCommandBuffer* CmdBuffer = ((FMetalDebugRenderCommandEncoder*)EncoderDebug.GetPtr())->Buffer.GetPtr();
		[CommandBuffer->DebugGroup addObjectsFromArray:CmdBuffer->DebugGroup];
		CommandBuffer->DebugCommands.Append(CmdBuffer->DebugCommands);
	}
	m_ptr->Buffer.EndCommandEncoder();
}

#endif

