// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalDebugCommandEncoder.h"

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
NS_ASSUME_NONNULL_BEGIN

@class FMetalDebugCommandBuffer;
@class FMetalDebugBlitCommandEncoder;

class FMetalBlitCommandEncoderDebugging : public FMetalCommandEncoderDebugging
{
public:
	FMetalBlitCommandEncoderDebugging();
	FMetalBlitCommandEncoderDebugging(mtlpp::BlitCommandEncoder& Encoder, FMetalCommandBufferDebugging& Buffer);
	FMetalBlitCommandEncoderDebugging(FMetalDebugCommandEncoder* handle);
	
	static FMetalBlitCommandEncoderDebugging Get(mtlpp::BlitCommandEncoder& Buffer);
	
	void InsertDebugSignpost(ns::String const& Label);
	void PushDebugGroup(ns::String const& Group);
	void PopDebugGroup();
	
	void EndEncoder();
	
#if PLATFORM_MAC
	void Synchronize(mtlpp::Resource const& resource);
	
	void Synchronize(FMetalTexture const& texture, NSUInteger slice, NSUInteger level);
#endif
	
	void Copy(FMetalTexture const& sourceTexture, NSUInteger sourceSlice, NSUInteger sourceLevel, mtlpp::Origin const& sourceOrigin, mtlpp::Size const& sourceSize, FMetalTexture const& destinationTexture, NSUInteger destinationSlice, NSUInteger destinationLevel, mtlpp::Origin const& destinationOrigin);
	
	void Copy(FMetalBuffer const& sourceBuffer, NSUInteger sourceOffset, NSUInteger sourceBytesPerRow, NSUInteger sourceBytesPerImage, mtlpp::Size const& sourceSize, FMetalTexture const& destinationTexture, NSUInteger destinationSlice, NSUInteger destinationLevel, mtlpp::Origin const& destinationOrigin);
	
	void Copy(FMetalBuffer const& sourceBuffer, NSUInteger sourceOffset, NSUInteger sourceBytesPerRow, NSUInteger sourceBytesPerImage, mtlpp::Size const& sourceSize, FMetalTexture const& destinationTexture, NSUInteger destinationSlice, NSUInteger destinationLevel, mtlpp::Origin const& destinationOrigin, mtlpp::BlitOption options);
	
	void Copy(FMetalTexture const& sourceTexture, NSUInteger sourceSlice, NSUInteger sourceLevel, mtlpp::Origin const& sourceOrigin, mtlpp::Size const& sourceSize, FMetalBuffer const& destinationBuffer, NSUInteger destinationOffset, NSUInteger destinationBytesPerRow, NSUInteger destinationBytesPerImage);
	
	void Copy(FMetalTexture const& sourceTexture, NSUInteger sourceSlice, NSUInteger sourceLevel, mtlpp::Origin const& sourceOrigin, mtlpp::Size const& sourceSize, FMetalBuffer const& destinationBuffer, NSUInteger destinationOffset, NSUInteger destinationBytesPerRow, NSUInteger destinationBytesPerImage, mtlpp::BlitOption options);
	
	void GenerateMipmaps(FMetalTexture const& texture);
	
	void Fill(FMetalBuffer const& buffer, ns::Range const& range, uint8_t value);
	
	void Copy(FMetalBuffer const& sourceBuffer, NSUInteger sourceOffset, FMetalBuffer const& destinationBuffer, NSUInteger destinationOffset, NSUInteger size);
};

NS_ASSUME_NONNULL_END
#endif
