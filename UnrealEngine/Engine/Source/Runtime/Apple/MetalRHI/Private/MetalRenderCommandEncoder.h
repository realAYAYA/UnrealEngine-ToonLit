// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalDebugCommandEncoder.h"

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
NS_ASSUME_NONNULL_BEGIN

@class FMetalShaderPipeline;
@class FMetalDebugRenderCommandEncoder;
@class FMetalDebugParallelRenderCommandEncoder;

class FMetalRenderCommandEncoderDebugging : public FMetalCommandEncoderDebugging
{
	void InsertDebugDraw();
public:
	FMetalRenderCommandEncoderDebugging();
	FMetalRenderCommandEncoderDebugging(mtlpp::RenderCommandEncoder& Encoder, mtlpp::RenderPassDescriptor const& Desc, FMetalCommandBufferDebugging& Buffer);
	FMetalRenderCommandEncoderDebugging(FMetalDebugCommandEncoder* handle);
	
	static FMetalRenderCommandEncoderDebugging Get(mtlpp::RenderCommandEncoder& Buffer);
	
	void SetPipeline(FMetalShaderPipeline* Pipeline);
	
	void SetBytes(EMetalShaderFrequency Freq, const void * bytes, NSUInteger length, NSUInteger index);
	void SetBuffer(EMetalShaderFrequency Freq,  FMetalBuffer const& buffer, NSUInteger offset, NSUInteger index);
	void SetBufferOffset(EMetalShaderFrequency Freq, NSUInteger offset, NSUInteger index);
	void SetTexture(EMetalShaderFrequency Freq,  FMetalTexture const& texture, NSUInteger index);
	void SetSamplerState(EMetalShaderFrequency Freq,  mtlpp::SamplerState const& sampler, NSUInteger index);
	void SetSamplerState(EMetalShaderFrequency Freq,  mtlpp::SamplerState const& sampler, float lodMinClamp, float lodMaxClamp, NSUInteger index);
	void SetDepthStencilState( mtlpp::DepthStencilState const& depthStencilState);
	
	void Draw(mtlpp::PrimitiveType primitiveType, NSUInteger vertexStart, NSUInteger vertexCount, NSUInteger instanceCount);
	
	void Draw(mtlpp::PrimitiveType primitiveType, NSUInteger vertexStart, NSUInteger vertexCount);
	
	void DrawIndexed(mtlpp::PrimitiveType primitiveType, NSUInteger indexCount, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset, NSUInteger instanceCount);
	
	void DrawIndexed(mtlpp::PrimitiveType primitiveType, NSUInteger indexCount, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset);
	
	void Draw(mtlpp::PrimitiveType primitiveType, NSUInteger vertexStart, NSUInteger vertexCount, NSUInteger instanceCount, NSUInteger baseInstance);
	
	void DrawIndexed(mtlpp::PrimitiveType primitiveType, NSUInteger indexCount, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset, NSUInteger instanceCount, NSInteger baseVertex, NSUInteger baseInstance);
	
	void Draw(mtlpp::PrimitiveType primitiveType, FMetalBuffer const& indirectBuffer, NSUInteger indirectBufferOffset);
	
	void DrawIndexed(mtlpp::PrimitiveType primitiveType, mtlpp::IndexType indexType, FMetalBuffer const& indexBuffer, NSUInteger indexBufferOffset, FMetalBuffer const& indirectBuffer, NSUInteger indirectBufferOffset);
	
	/** Validates the pipeline/binding state */
	bool ValidateFunctionBindings(EMetalShaderFrequency Frequency);
	void Validate();
	
	void InsertDebugSignpost(ns::String const& Label);
	void PushDebugGroup(ns::String const& Group);
	void PopDebugGroup();
	
	void EndEncoder();
};

class FMetalParallelRenderCommandEncoderDebugging : public ns::Object<FMetalDebugParallelRenderCommandEncoder*>
{
public:
	FMetalParallelRenderCommandEncoderDebugging();
	FMetalParallelRenderCommandEncoderDebugging(mtlpp::ParallelRenderCommandEncoder& Encoder, mtlpp::RenderPassDescriptor const& Desc, FMetalCommandBufferDebugging& Buffer);
	FMetalParallelRenderCommandEncoderDebugging(FMetalDebugParallelRenderCommandEncoder* handle);

	static FMetalDebugParallelRenderCommandEncoder* Get(mtlpp::ParallelRenderCommandEncoder& Buffer);

	FMetalRenderCommandEncoderDebugging GetRenderCommandEncoderDebugger(mtlpp::RenderCommandEncoder& Encoder);

	void InsertDebugSignpost(ns::String const& Label);
	void PushDebugGroup(ns::String const& Group);
	void PopDebugGroup();
	
	void EndEncoder();
};

NS_ASSUME_NONNULL_END
#endif
