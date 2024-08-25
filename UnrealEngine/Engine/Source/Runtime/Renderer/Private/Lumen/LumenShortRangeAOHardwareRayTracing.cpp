// Copyright Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "SceneTextureParameters.h"
#include "IndirectLightRendering.h"
#include "LumenRadianceCache.h"
#include "LumenScreenProbeGather.h"

#if RHI_RAYTRACING

#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"

#endif // RHI_RAYTRACING

static TAutoConsoleVariable<int32> CVarLumenShortRangeAOHardwareRayTracing(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing"),
	0,
	TEXT("0. Screen space tracing for the full resolution Bent Normal (directional occlusion).")
	TEXT("1. Enable hardware ray tracing of the full resolution Bent Normal (directional occlusion). (Default)\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenShortRangeAOHardwareRayTracingNormalBias(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing.NormalBias"),
	.1f,
	TEXT("Bias for HWRT Bent Normal to avoid self intersection"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace Lumen
{
	bool UseHardwareRayTracedShortRangeAO(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenShortRangeAOHardwareRayTracing.GetValueOnAnyThread() != 0);
#else
		return false;
#endif
	}
}

#if RHI_RAYTRACING

class FLumenShortRangeAOHardwareRayTracing : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenShortRangeAOHardwareRayTracing)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenShortRangeAOHardwareRayTracing, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWScreenBentNormal)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
		SHADER_PARAMETER(uint32, NumRays)
		SHADER_PARAMETER(float, NormalBias)
		SHADER_PARAMETER(float, MaxScreenTraceFraction)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, HairStrandsVoxel)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
	END_SHADER_PARAMETER_STRUCT()

	class FHairStrandsVoxel : SHADER_PERMUTATION_BOOL("USE_HAIRSTRANDS_VOXEL");
	using FPermutationDomain = TShaderPermutationDomain<FHairStrandsVoxel>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_CLOSEST_HIT_SHADER"), 0);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_ANY_HIT_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_MISS_SHADER"), 0);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_COHERENT_RAYS"), 1);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::RayTracingMaterial;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenShortRangeAOHardwareRayTracing, "/Engine/Private/Lumen/LumenShortRangeAOHardwareRayTracing.usf", "LumenShortRangeAOHardwareRayTracing", SF_RayGen);

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingShortRangeAO(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedShortRangeAO(*View.Family))
	{
		for (int32 HairOcclusion = 0; HairOcclusion < 2; HairOcclusion++)
		{
			FLumenShortRangeAOHardwareRayTracing::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenShortRangeAOHardwareRayTracing::FHairStrandsVoxel>(HairOcclusion == 0);
			TShaderRef<FLumenShortRangeAOHardwareRayTracing> RayGenerationShader = View.ShaderMap->GetShader<FLumenShortRangeAOHardwareRayTracing>(PermutationVector);
			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

#endif // RHI_RAYTRACING

void RenderHardwareRayTracingShortRangeAO(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextureParameters& SceneTextures,
	const FBlueNoise& BlueNoise,
	float MaxScreenTraceFraction,
	const FViewInfo& View,
	FRDGTextureRef ScreenBentNormal,
	uint32 NumPixelRays)
#if RHI_RAYTRACING
{
	extern int32 GLumenShortRangeAOHairStrandsVoxelTrace;
	const bool bNeedTraceHairVoxel = HairStrands::HasViewHairStrandsVoxelData(View) && GLumenShortRangeAOHairStrandsVoxelTrace > 0;

	{
		FLumenShortRangeAOHardwareRayTracing::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenShortRangeAOHardwareRayTracing::FParameters>();
		PassParameters->RWScreenBentNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenBentNormal));
		PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->SceneTextures = SceneTextures;
		PassParameters->BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);
		PassParameters->MaxScreenTraceFraction = MaxScreenTraceFraction;
		PassParameters->NumRays = NumPixelRays;
		PassParameters->NormalBias = CVarLumenShortRangeAOHardwareRayTracingNormalBias.GetValueOnRenderThread();
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

		if (bNeedTraceHairVoxel)
		{
			PassParameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
		}

		FLumenShortRangeAOHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set< FLumenShortRangeAOHardwareRayTracing::FHairStrandsVoxel>(bNeedTraceHairVoxel);
		TShaderMapRef<FLumenShortRangeAOHardwareRayTracing> RayGenerationShader(View.ShaderMap, PermutationVector);

		ClearUnusedGraphResources(RayGenerationShader, PassParameters);

		FIntPoint Resolution(View.ViewRect.Width(), View.ViewRect.Height());

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShortRangeAO_HWRT(Rays=%u)", NumPixelRays),
			PassParameters,
			ERDGPassFlags::Compute,
			[&View, RayGenerationShader, PassParameters, Resolution](FRHIRayTracingCommandList& RHICmdList)
			{
				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);

				FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

				bool bBentNormalEnableMaterials = false;

				if (bBentNormalEnableMaterials)
				{
					RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, Resolution.X, Resolution.Y);
				}
				else
				{
					FRayTracingPipelineStateInitializer Initializer;

					Initializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(ERayTracingPayloadType::RayTracingMaterial);

					FRHIRayTracingShader* RayGenShaderTable[] = { RayGenerationShader.GetRayTracingShader() };
					Initializer.SetRayGenShaderTable(RayGenShaderTable);

					FRHIRayTracingShader* HitGroupTable[] = { GetRayTracingDefaultOpaqueShader(View.ShaderMap) };
					Initializer.SetHitGroupTable(HitGroupTable);
					Initializer.bAllowHitGroupIndexing = false; // Use the same hit shader for all geometry in the scene by disabling SBT indexing.

					FRHIRayTracingShader* MissGroupTable[] = { GetRayTracingDefaultMissShader(View.ShaderMap) };
					Initializer.SetMissShaderTable(MissGroupTable);

					FRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);
					RHICmdList.SetRayTracingMissShader(RayTracingSceneRHI, 0, Pipeline, 0 /* ShaderIndexInPipeline */, 0, nullptr, 0);
					RHICmdList.RayTraceDispatch(Pipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, Resolution.X, Resolution.Y);
				}
			});
	}
}
#else // RHI_RAYTRACING
{
	unimplemented();
}
#endif // RHI_RAYTRACING
