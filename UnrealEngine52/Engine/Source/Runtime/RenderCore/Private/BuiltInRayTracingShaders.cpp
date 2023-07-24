// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuiltInRayTracingShaders.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"
#include "RendererInterface.h"

#if RHI_RAYTRACING

#include "RayTracingPayloadType.h"

uint32 GetRaytracingMaterialPayloadSize()
{
	return Strata::IsStrataEnabled() ? Strata::GetRayTracingMaterialPayloadSizeInBytes() : 64u;
}

IMPLEMENT_RT_PAYLOAD_TYPE(ERayTracingPayloadType::Default, 24);
IMPLEMENT_RT_PAYLOAD_TYPE_FUNCTION(ERayTracingPayloadType::RayTracingMaterial, GetRaytracingMaterialPayloadSize);

IMPLEMENT_GLOBAL_SHADER( FDefaultPayloadMS, "/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf", "DefaultPayloadMS", SF_RayMiss);
IMPLEMENT_GLOBAL_SHADER( FPackedMaterialClosestHitPayloadMS, "/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf", "PackedMaterialClosestHitPayloadMS", SF_RayMiss);
IMPLEMENT_GLOBAL_SHADER(FRayTracingDispatchDescCS, "/Engine/Private/RayTracing/RayTracingDispatchDesc.usf", "RayTracingDispatchDescCS", SF_Compute);


ERayTracingPayloadType FDefaultPayloadMS::GetRayTracingPayloadType(const int32 PermutationId)
{
	return ERayTracingPayloadType::Default;
}

ERayTracingPayloadType FPackedMaterialClosestHitPayloadMS::GetRayTracingPayloadType(const int32 PermutationId)
{
	return ERayTracingPayloadType::RayTracingMaterial;
}


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
