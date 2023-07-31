// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#if METAL_DEBUG_OPTIONS
#include "MetalDebugCommandEncoder.h"
#endif

#include "ShaderPipelineCache.h"

enum EMetalPipelineHashBits
{
	NumBits_RenderTargetFormat = 6, //(x8=48),
	NumBits_DepthFormat = 3, //(x1=3),
	NumBits_StencilFormat = 3, //(x1=3),
	NumBits_SampleCount = 3, //(x1=3),

	NumBits_BlendState = 7, //(x8=56),
	NumBits_PrimitiveTopology = 2, //(x1=2)
	NumBits_AlphaToCoverage = 1, //(x1=1)
};

enum EMetalPipelineHashOffsets
{
	Offset_BlendState0 = 0,
	Offset_BlendState1 = Offset_BlendState0 + NumBits_BlendState,
	Offset_BlendState2 = Offset_BlendState1 + NumBits_BlendState,
	Offset_BlendState3 = Offset_BlendState2 + NumBits_BlendState,
	Offset_BlendState4 = Offset_BlendState3 + NumBits_BlendState,
	Offset_BlendState5 = Offset_BlendState4 + NumBits_BlendState,
	Offset_BlendState6 = Offset_BlendState5 + NumBits_BlendState,
	Offset_BlendState7 = Offset_BlendState6 + NumBits_BlendState,
	Offset_PrimitiveTopology = Offset_BlendState7 + NumBits_BlendState,
	Offset_RasterEnd = Offset_PrimitiveTopology + NumBits_PrimitiveTopology,

	Offset_RenderTargetFormat0 = 64,
	Offset_RenderTargetFormat1 = Offset_RenderTargetFormat0 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat2 = Offset_RenderTargetFormat1 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat3 = Offset_RenderTargetFormat2 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat4 = Offset_RenderTargetFormat3 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat5 = Offset_RenderTargetFormat4 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat6 = Offset_RenderTargetFormat5 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat7 = Offset_RenderTargetFormat6 + NumBits_RenderTargetFormat,
	Offset_DepthFormat = Offset_RenderTargetFormat7 + NumBits_RenderTargetFormat,
	Offset_StencilFormat = Offset_DepthFormat + NumBits_DepthFormat,
	Offset_SampleCount = Offset_StencilFormat + NumBits_StencilFormat,
	Offset_AlphaToCoverage = Offset_SampleCount + NumBits_SampleCount,
	Offset_End = Offset_AlphaToCoverage + NumBits_AlphaToCoverage
};

class FMetalPipelineStateCacheManager
{
public:
	FMetalPipelineStateCacheManager();
	~FMetalPipelineStateCacheManager();
	
private:
	FDelegateHandle OnShaderPipelineCachePreOpenDelegate;
	FDelegateHandle OnShaderPipelineCacheOpenedDelegate;
	FDelegateHandle OnShaderPipelineCachePrecompilationCompleteDelegate;
	
	/** Delegate handlers to track the ShaderPipelineCache precompile. */
	void OnShaderPipelineCachePreOpen(FString const& Name, EShaderPlatform Platform, bool& bReady);
	void OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);
	void OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);
};

@interface FMetalShaderPipeline : FApplePlatformObject
{
@public
	mtlpp::RenderPipelineState RenderPipelineState;
	mtlpp::ComputePipelineState ComputePipelineState;
	mtlpp::RenderPipelineState StreamPipelineState;
	mtlpp::RenderPipelineState DebugPipelineState;
	TArray<uint32> BufferDataSizes[EMetalShaderStagesNum];
	TMap<uint8, uint8> TextureTypes[EMetalShaderStagesNum];
	FMetalDebugShaderResourceMask ResourceMask[EMetalShaderStagesNum];
	mtlpp::RenderPipelineReflection RenderPipelineReflection;
	mtlpp::RenderPipelineReflection StreamPipelineReflection;
	mtlpp::ComputePipelineReflection ComputePipelineReflection;
#if METAL_DEBUG_OPTIONS
	ns::String VertexSource;
	ns::String FragmentSource;
	ns::String ComputeSource;
	mtlpp::RenderPipelineDescriptor RenderDesc;
	mtlpp::RenderPipelineDescriptor StreamDesc;
	mtlpp::ComputePipelineDescriptor ComputeDesc;
#endif
}
- (instancetype)init;
- (void)initResourceMask;
- (void)initResourceMask:(EMetalShaderFrequency)Frequency;
@end
