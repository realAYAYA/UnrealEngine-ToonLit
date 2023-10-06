// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalBlitCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalBlitCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalFence.h"

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
extern int32 GMetalRuntimeDebugLevel;

@interface FMetalDebugBlitCommandEncoder : FMetalDebugCommandEncoder
{
	@public
	id<MTLBlitCommandEncoder> Inner;
	FMetalCommandBufferDebugging Buffer;
}
/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithEncoder:(id<MTLBlitCommandEncoder>)Encoder andCommandBuffer:(FMetalCommandBufferDebugging const&)Buffer;

@end

@implementation FMetalDebugBlitCommandEncoder

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugBlitCommandEncoder)

-(id)initWithEncoder:(id<MTLBlitCommandEncoder>)Encoder andCommandBuffer:(FMetalCommandBufferDebugging const&)SourceBuffer
{
	id Self = [super init];
	if (Self)
	{
        Inner = Encoder;
		Buffer = SourceBuffer;
	}
	return Self;
}

-(void)dealloc
{
	[super dealloc];
}

@end

FMetalBlitCommandEncoderDebugging::FMetalBlitCommandEncoderDebugging()
{
	
}
FMetalBlitCommandEncoderDebugging::FMetalBlitCommandEncoderDebugging(mtlpp::BlitCommandEncoder& Encoder, FMetalCommandBufferDebugging& Buffer)
: FMetalCommandEncoderDebugging((FMetalDebugCommandEncoder*)[[[FMetalDebugBlitCommandEncoder alloc] initWithEncoder:Encoder.GetPtr() andCommandBuffer:Buffer] autorelease])
{
	Buffer.BeginBlitCommandEncoder([NSString stringWithFormat:@"Blit: %@", Encoder.GetLabel().GetPtr()]);
	Encoder.SetAssociatedObject((void const*)&FMetalBlitCommandEncoderDebugging::Get, (FMetalCommandEncoderDebugging const&)*this);
}
FMetalBlitCommandEncoderDebugging::FMetalBlitCommandEncoderDebugging(FMetalDebugCommandEncoder* handle)
: FMetalCommandEncoderDebugging((FMetalDebugCommandEncoder*)handle)
{
	
}

FMetalBlitCommandEncoderDebugging FMetalBlitCommandEncoderDebugging::Get(mtlpp::BlitCommandEncoder& Encoder)
{
	return Encoder.GetAssociatedObject<FMetalBlitCommandEncoderDebugging>((void const*)&FMetalBlitCommandEncoderDebugging::Get);
}

void FMetalBlitCommandEncoderDebugging::InsertDebugSignpost(ns::String const& Label)
{
	((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.InsertDebugSignpost(Label);
}
void FMetalBlitCommandEncoderDebugging::PushDebugGroup(ns::String const& Group)
{
	((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.PushDebugGroup(Group);
}
void FMetalBlitCommandEncoderDebugging::PopDebugGroup()
{
	((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.PopDebugGroup();
}

void FMetalBlitCommandEncoderDebugging::EndEncoder()
{
	((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.EndCommandEncoder();
}

#if PLATFORM_MAC
void FMetalBlitCommandEncoderDebugging::Synchronize(mtlpp::Resource const& resource)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::Synchronize(FMetalTexture const& texture, NSUInteger slice, NSUInteger level)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}
#endif

void FMetalBlitCommandEncoderDebugging::Copy(FMetalTexture const& sourceTexture, NSUInteger sourceSlice, NSUInteger sourceLevel, mtlpp::Origin const& sourceOrigin, mtlpp::Size const& sourceSize, FMetalTexture const& destinationTexture, NSUInteger destinationSlice, NSUInteger destinationLevel, mtlpp::Origin const& destinationOrigin)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::Copy(FMetalBuffer const& sourceBuffer, NSUInteger sourceOffset, NSUInteger sourceBytesPerRow, NSUInteger sourceBytesPerImage, mtlpp::Size const& sourceSize, FMetalTexture const& destinationTexture, NSUInteger destinationSlice, NSUInteger destinationLevel, mtlpp::Origin const& destinationOrigin)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::Copy(FMetalBuffer const& sourceBuffer, NSUInteger sourceOffset, NSUInteger sourceBytesPerRow, NSUInteger sourceBytesPerImage, mtlpp::Size const& sourceSize, FMetalTexture const& destinationTexture, NSUInteger destinationSlice, NSUInteger destinationLevel, mtlpp::Origin const& destinationOrigin, mtlpp::BlitOption options)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::Copy(FMetalTexture const& sourceTexture, NSUInteger sourceSlice, NSUInteger sourceLevel, mtlpp::Origin const& sourceOrigin, mtlpp::Size const& sourceSize, FMetalBuffer const& destinationBuffer, NSUInteger destinationOffset, NSUInteger destinationBytesPerRow, NSUInteger destinationBytesPerImage)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::Copy(FMetalTexture const& sourceTexture, NSUInteger sourceSlice, NSUInteger sourceLevel, mtlpp::Origin const& sourceOrigin, mtlpp::Size const& sourceSize, FMetalBuffer const& destinationBuffer, NSUInteger destinationOffset, NSUInteger destinationBytesPerRow, NSUInteger destinationBytesPerImage, mtlpp::BlitOption options)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::GenerateMipmaps(FMetalTexture const& texture)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::Fill(FMetalBuffer const& buffer, ns::Range const& range, uint8_t value)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

void FMetalBlitCommandEncoderDebugging::Copy(FMetalBuffer const& sourceBuffer, NSUInteger sourceOffset, FMetalBuffer const& destinationBuffer, NSUInteger destinationOffset, NSUInteger size)
{
#if METAL_DEBUG_OPTIONS
	switch(((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.GetPtr()->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			((FMetalDebugBlitCommandEncoder*)m_ptr)->Buffer.Blit([NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]);
		}
		default:
		{
			break;
		}
	}
#endif
}

#endif
