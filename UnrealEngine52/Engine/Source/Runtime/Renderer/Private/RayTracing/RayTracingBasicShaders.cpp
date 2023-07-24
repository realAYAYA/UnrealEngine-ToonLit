// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingBasicShaders.h"

#if RHI_RAYTRACING

#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"

IMPLEMENT_GLOBAL_SHADER( FBasicOcclusionMainRGS, "/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf", "OcclusionMainRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER( FBasicIntersectionMainRGS, "/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf", "IntersectionMainRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER( FBasicIntersectionMainCHS, "/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf", "IntersectionMainCHS", SF_RayHitGroup);

struct FBasicRayTracingPipeline
{
	FRayTracingPipelineState* PipelineState = nullptr;
	TShaderRef<FBasicOcclusionMainRGS> OcclusionRGS;
	TShaderRef<FBasicIntersectionMainRGS> IntersectionRGS;
};

/**
* Returns a ray tracing pipeline with FBasicOcclusionMainRGS, FBasicIntersectionMainRGS, FBasicIntersectionMainCHS and FDefaultPayloadMS.
* This can be used to perform basic "fixed function" occlusion and intersection ray tracing.
*/
FBasicRayTracingPipeline GetBasicRayTracingPipeline(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(FeatureLevel);

	FRayTracingPipelineStateInitializer PipelineInitializer;
	PipelineInitializer.bAllowHitGroupIndexing = false;

	auto OcclusionRGS = ShaderMap->GetShader<FBasicOcclusionMainRGS>();
	auto IntersectionRGS = ShaderMap->GetShader<FBasicIntersectionMainRGS>();

	FRHIRayTracingShader* RayGenShaderTable[] = { OcclusionRGS.GetRayTracingShader(), IntersectionRGS.GetRayTracingShader() };
	PipelineInitializer.SetRayGenShaderTable(RayGenShaderTable);

	auto ClosestHitShader = ShaderMap->GetShader<FBasicIntersectionMainCHS>();
	FRHIRayTracingShader* HitShaderTable[] = { ClosestHitShader.GetRayTracingShader() };
	PipelineInitializer.SetHitGroupTable(HitShaderTable);

	auto MissShader = ShaderMap->GetShader<FDefaultPayloadMS>();
	FRHIRayTracingShader* MissShaderTable[] = { MissShader.GetRayTracingShader() };
	PipelineInitializer.SetMissShaderTable(MissShaderTable);

	FBasicRayTracingPipeline Result;

	Result.PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, PipelineInitializer);
	Result.OcclusionRGS = OcclusionRGS;
	Result.IntersectionRGS = IntersectionRGS;

	return Result;
}

void DispatchBasicOcclusionRays(FRHICommandList& RHICmdList, FRHIRayTracingScene* Scene, FRHIShaderResourceView* SceneView, FRHIShaderResourceView* RayBufferView, FRHIUnorderedAccessView* ResultView, uint32 NumRays)
{
	FBasicRayTracingPipeline RayTracingPipeline = GetBasicRayTracingPipeline(RHICmdList, GMaxRHIFeatureLevel);

	RHICmdList.SetRayTracingHitGroup(Scene, 0, 0, 0, RayTracingPipeline.PipelineState, 0, 0, nullptr, 0, nullptr, 0);
	RHICmdList.SetRayTracingMissShader(Scene, 0, RayTracingPipeline.PipelineState, 0, 0, nullptr, 0);

	FBasicOcclusionMainRGS::FParameters OcclusionParameters;
	OcclusionParameters.TLAS = SceneView;
	OcclusionParameters.Rays = RayBufferView;
	OcclusionParameters.OcclusionOutput = ResultView;

	FRayTracingShaderBindingsWriter GlobalResources;
	SetShaderParameters(GlobalResources, RayTracingPipeline.OcclusionRGS, OcclusionParameters);
	RHICmdList.RayTraceDispatch(RayTracingPipeline.PipelineState, RayTracingPipeline.OcclusionRGS.GetRayTracingShader(), Scene, GlobalResources, NumRays, 1);
}

void DispatchBasicIntersectionRays(FRHICommandList& RHICmdList, FRHIRayTracingScene* Scene, FRHIShaderResourceView* SceneView, FRHIShaderResourceView* RayBufferView, FRHIUnorderedAccessView* ResultView, uint32 NumRays)
{
	FBasicRayTracingPipeline RayTracingPipeline = GetBasicRayTracingPipeline(RHICmdList, GMaxRHIFeatureLevel);

	RHICmdList.SetRayTracingHitGroup(Scene, 0, 0, 0, RayTracingPipeline.PipelineState, 0, 0, nullptr, 0, nullptr, 0);
	RHICmdList.SetRayTracingMissShader(Scene, 0, RayTracingPipeline.PipelineState, 0, 0, nullptr, 0);

	FBasicIntersectionMainRGS::FParameters OcclusionParameters;
	OcclusionParameters.TLAS = SceneView;
	OcclusionParameters.Rays = RayBufferView;
	OcclusionParameters.IntersectionOutput = ResultView;

	FRayTracingShaderBindingsWriter GlobalResources;
	SetShaderParameters(GlobalResources, RayTracingPipeline.IntersectionRGS, OcclusionParameters);
	RHICmdList.RayTraceDispatch(RayTracingPipeline.PipelineState, RayTracingPipeline.IntersectionRGS.GetRayTracingShader(), Scene, GlobalResources, NumRays, 1);
}

#endif // RHI_RAYTRACING
