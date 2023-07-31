// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalDebugCommandEncoder.h"

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
NS_ASSUME_NONNULL_BEGIN

@class FMetalDebugComputeCommandEncoder;
@class FMetalShaderPipeline;


class FMetalComputeCommandEncoderDebugging : public FMetalCommandEncoderDebugging
{
	void InsertDebugDispatch();
	void Validate();
public:
	FMetalComputeCommandEncoderDebugging();
	FMetalComputeCommandEncoderDebugging(mtlpp::ComputeCommandEncoder& Encoder, FMetalCommandBufferDebugging& Buffer);
	FMetalComputeCommandEncoderDebugging(FMetalDebugCommandEncoder* handle);
	
	static FMetalComputeCommandEncoderDebugging Get(mtlpp::ComputeCommandEncoder& Buffer);
	
	void InsertDebugSignpost(ns::String const& Label);
	void PushDebugGroup(ns::String const& Group);
	void PopDebugGroup();
	void EndEncoder();
	void DispatchThreadgroups(mtlpp::Size const& threadgroupsPerGrid, mtlpp::Size const& threadsPerThreadgroup);
	void SetPipeline(FMetalShaderPipeline* Pipeline);
	void SetBytes(const void * bytes, NSUInteger length, NSUInteger index);
	void SetBuffer( FMetalBuffer const& buffer, NSUInteger offset, NSUInteger index);
	void SetBufferOffset(NSUInteger offset, NSUInteger index);
	void SetTexture( FMetalTexture const& texture, NSUInteger index);
	void SetSamplerState( mtlpp::SamplerState const& sampler, NSUInteger index);
	void SetSamplerState( mtlpp::SamplerState const& sampler, float lodMinClamp, float lodMaxClamp, NSUInteger index);
	
	void DispatchThreadgroupsWithIndirectBuffer(FMetalBuffer const& indirectBuffer, NSUInteger indirectBufferOffset, mtlpp::Size const& threadsPerThreadgroup);
};

NS_ASSUME_NONNULL_END
#endif
