// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuiltInRayTracingShaders.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"

#if RHI_RAYTRACING

PRAGMA_DISABLE_DEPRECATION_WARNINGS
IMPLEMENT_GLOBAL_SHADER( FOcclusionMainRG,		"/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf",		"OcclusionMainRG",				SF_RayGen);
IMPLEMENT_GLOBAL_SHADER( FIntersectionMainRG,	"/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf",		"IntersectionMainRG",			SF_RayGen);
IMPLEMENT_SHADER_TYPE(, FIntersectionMainCHS,	TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("IntersectionMainCHS"),	SF_RayHitGroup);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

IMPLEMENT_SHADER_TYPE(, FDefaultMainCHS,		TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("DefaultMainCHS"),		SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(, FDefaultMainCHSOpaqueAHS, TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("closesthit=DefaultMainCHS anyhit=DefaultOpaqueAHS"), SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(, FDefaultPayloadMS,		TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("DefaultPayloadMS"),			SF_RayMiss);
IMPLEMENT_SHADER_TYPE(, FPackedMaterialClosestHitPayloadMS, TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("PackedMaterialClosestHitPayloadMS"), SF_RayMiss);

IMPLEMENT_GLOBAL_SHADER(FRayTracingDispatchDescCS, "/Engine/Private/RayTracing/RayTracingDispatchDesc.usf", "RayTracingDispatchDescCS", SF_Compute);

void FRayTracingDispatchDescCS::Dispatch(FRHICommandList& RHICmdList, 
	const void* DispatchDescInput, uint32 DispatchDescSize, uint32 DispatchDescDimensionsOffset,
	FRHIShaderResourceView* DispatchDimensionsSRV, uint32 DimensionsBufferOffset,
	FRHIUnorderedAccessView* DispatchDescOutputUAV)
{
	const uint32 DispatchDescSizeDwords = DispatchDescSize / 4;
	const uint32 DispatchDescDimensionsOffsetDwords = DispatchDescDimensionsOffset / 4;

	checkf(DimensionsBufferOffset % 4 == 0, TEXT("Dispatch dimensions buffer offset must be DWORD-aligned"));
	const uint32 DimensionsBufferOffsetDwords = DimensionsBufferOffset / 4;

	check(DispatchDescSizeDwords <= DispatchDescMaxSizeDwords);

	TShaderMapRef<FRayTracingDispatchDescCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ShaderRHI);

	static_assert(DispatchDescMaxSizeDwords % 4 == 0, "DispatchDescMaxSizeDwords must be a multiple of 4");
	static constexpr uint32 DispatchDescMaxSizeUint4s = DispatchDescMaxSizeDwords / 4;

	FUintVector4 DispatchDescData[DispatchDescMaxSizeUint4s] = {};
	FMemory::Memcpy(DispatchDescData, DispatchDescInput, DispatchDescSize);

	SetShaderValueArray(RHICmdList, ShaderRHI, ComputeShader->DispatchDescInputParam, DispatchDescData, DispatchDescMaxSizeUint4s);
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->DispatchDescSizeDwordsParam, DispatchDescSizeDwords);
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->DispatchDescDimensionsOffsetDwordsParam, DispatchDescDimensionsOffsetDwords);
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->DimensionsBufferOffsetDwordsParam, DimensionsBufferOffsetDwords);

	SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->DispatchDimensionsParam, DispatchDimensionsSRV);
	SetUAVParameter(RHICmdList, ShaderRHI, ComputeShader->DispatchDescOutputParam, DispatchDescOutputUAV);

	RHICmdList.DispatchComputeShader(1, 1, 1);

	SetSRVParameter(RHICmdList, ShaderRHI, ComputeShader->DispatchDimensionsParam, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, ComputeShader->DispatchDescOutputParam, nullptr);
}

#endif // RHI_RAYTRACING
