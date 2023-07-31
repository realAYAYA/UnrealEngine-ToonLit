// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "SceneTextureParameters.h"
#include "IndirectLightRendering.h"

#include "RayTracing/RayTracingLighting.h"

// Actual screen-probe requirements..
#include "LumenRadianceCache.h"
#include "LumenScreenProbeGather.h"

namespace Lumen
{
	struct FHardwareRayTracingPermutationSettings
	{
		EHardwareRayTracingLightingMode LightingMode;
		bool bUseMinimalPayload;
		bool bUseDeferredMaterial;
	};

	// Struct definitions much match those in LumenHardwareRayTracingCommon.ush 
	struct FHitGroupRootConstants
	{
		uint32 BaseInstanceIndex;
		uint32 UserData;
	};

	enum class ERayTracingShaderDispatchSize
	{
		DispatchSize1D = 0,
		DispatchSize2D = 1,
	};

	enum class ERayTracingShaderDispatchType
	{
		RayGen = 0,
		Inline = 1
	};
}

class FLumenHardwareRayTracingShaderBase : public FGlobalShader
{
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FSharedParameters, )
		// Scene includes
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_SRV(StructuredBuffer, RayTracingSceneMetadata)

		// Lighting structures
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRaytracingLightDataPacked, LightDataPacked)

		// Surface cache
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)

		// Inline data
		SHADER_PARAMETER_SRV(StructuredBuffer<Lumen::FHitGroupRootConstants>, HitGroupData)
	END_SHADER_PARAMETER_STRUCT()

	FLumenHardwareRayTracingShaderBase() = default;
	FLumenHardwareRayTracingShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	static constexpr const Lumen::ERayTracingShaderDispatchSize DispatchSize = Lumen::ERayTracingShaderDispatchSize::DispatchSize2D;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ESurfaceCacheSampling SurfaceCacheSampling, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SURFACE_CACHE_FEEDBACK"), SurfaceCacheSampling == Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback ? 0 : 1);
		OutEnvironment.SetDefine(TEXT("SURFACE_CACHE_HIGH_RES_PAGES"), SurfaceCacheSampling == Lumen::ESurfaceCacheSampling::HighResPages ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("LUMEN_HARDWARE_RAYTRACING"), 1);

		// GPU Scene definitions
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		// Inline
		const bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;
		if (bInlineRayTracing)
		{
			OutEnvironment.SetDefine(TEXT("LUMEN_HARDWARE_INLINE_RAYTRACING"), 1);
			OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
			OutEnvironment.CompilerFlags.Add(CFLAG_InlineRayTracing);
		}
	}

	static void ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchSize Size, FShaderCompilerEnvironment& OutEnvironment)
	{
		if (DispatchSize == Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)
		{
			OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DISPATCH_1D"), 1);
		}
	}

	static FIntPoint GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ERayTracingShaderDispatchSize ShaderDispatchSize)
	{
		// Current inline ray tracing implementation requires 1:1 mapping between thread groups and waves and only supports wave32 mode.
		const bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;
		if (bInlineRayTracing)
		{
			switch (ShaderDispatchSize)
			{
			case Lumen::ERayTracingShaderDispatchSize::DispatchSize2D: return FIntPoint(8, 4);
			case Lumen::ERayTracingShaderDispatchSize::DispatchSize1D: return FIntPoint(32, 1);
			default:
				checkNoEntry();
			}
		}

		return FIntPoint(1, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		const bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;
		if (bInlineRayTracing)
		{
			return IsRayTracingEnabledForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform) && RHISupportsRayTracing(Parameters.Platform) && RHISupportsInlineRayTracing(Parameters.Platform);
		}
		else
		{
			return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
		}
	}
};

#define DECLARE_LUMEN_RAYTRACING_SHADER(ShaderClass, ShaderDispatchSize) \
	public: \
	ShaderClass() = default; \
	ShaderClass(const ShaderMetaType::CompiledShaderInitializerType & Initializer)\
		: FLumenHardwareRayTracingShaderBase(Initializer) {}\
	static constexpr const Lumen::ERayTracingShaderDispatchSize DispatchSize = ShaderDispatchSize; \
	using TComputeShaderType = class ShaderClass##CS; \
	using TRayGenShaderType = class ShaderClass##RGS;

#define IMPLEMENT_LUMEN_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
	class ShaderClass##CS : public ShaderClass \
	{ \
		DECLARE_GLOBAL_SHADER(ShaderClass##CS) \
		SHADER_USE_PARAMETER_STRUCT(ShaderClass##CS, ShaderClass) \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) \
		{ return ShaderClass::ShouldCompilePermutation(Parameters, Lumen::ERayTracingShaderDispatchType::Inline); }\
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)\
		{ \
			FIntPoint Size = GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::Inline, DispatchSize); \
			OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_X"), Size.X); \
			OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_Y"), Size.Y); \
			ShaderClass::ModifyCompilationEnvironment(Parameters, Lumen::ERayTracingShaderDispatchType::Inline, OutEnvironment); \
			ModifyCompilationEnvironmentInternal(DispatchSize, OutEnvironment); \
		}\
		static FIntPoint GetThreadGroupSize() { return GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::Inline, DispatchSize); } \
	};

#define IMPLEMENT_LUMEN_RAYGEN_RAYTRACING_SHADER(ShaderClass) \
	class ShaderClass##RGS : public ShaderClass \
	{ \
		DECLARE_GLOBAL_SHADER(ShaderClass##RGS) \
		SHADER_USE_ROOT_PARAMETER_STRUCT(ShaderClass##RGS, ShaderClass) \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) \
		{ return ShaderClass::ShouldCompilePermutation(Parameters, Lumen::ERayTracingShaderDispatchType::RayGen); } \
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment) \
		{ \
			ShaderClass::ModifyCompilationEnvironment(Parameters, Lumen::ERayTracingShaderDispatchType::RayGen, OutEnvironment); \
			ModifyCompilationEnvironmentInternal(DispatchSize, OutEnvironment); \
		} \
		static FIntPoint GetThreadGroupSize() { return GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::RayGen, DispatchSize); } \
	};
	
#define IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(ShaderClass) \
	IMPLEMENT_LUMEN_COMPUTE_RAYTRACING_SHADER(ShaderClass) \
	IMPLEMENT_LUMEN_RAYGEN_RAYTRACING_SHADER(ShaderClass)

class FLumenHardwareRayTracingDeferredMaterialRGS : public FLumenHardwareRayTracingShaderBase
{
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FDeferredMaterialParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER(int, TileSize)
		SHADER_PARAMETER(FIntPoint, DeferredMaterialBufferResolution)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDeferredMaterialPayload>, RWDeferredMaterialBuffer)
	END_SHADER_PARAMETER_STRUCT()

	FLumenHardwareRayTracingDeferredMaterialRGS() = default;
	FLumenHardwareRayTracingDeferredMaterialRGS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
		: FLumenHardwareRayTracingShaderBase(Initializer)
	{}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ESurfaceCacheSampling SurfaceCacheSampling, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, SurfaceCacheSampling, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		return FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType);
	}
};

void SetLumenHardwareRayTracingSharedParameters(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	const FLumenCardTracingInputs& TracingInputs,
	FLumenHardwareRayTracingShaderBase::FSharedParameters* SharedParameters);

// Hardware ray-tracing pipeline
namespace LumenHWRTPipeline
{
	enum class ELightingMode
	{
		// Permutations for tracing modes
		SurfaceCache,
		HitLighting,
		MAX
	};

	enum class ECompactMode
	{
		// Permutations for compaction modes
		HitLightingRetrace,
		FarFieldRetrace,
		ForceHitLighting,
		AppendRays,

		MAX
	};

	// Struct definitions much match those in LumenHardwareRayTracingPipelineCommon.ush
	struct FTraceDataPacked
	{
		//uint32 PackedData[4];
		uint32 PackedData[5];
	};

} // namespace LumenHWRTPipeline

void LumenHWRTCompactRays(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	int32 RayCount,
	LumenHWRTPipeline::ECompactMode CompactMode,
	const FRDGBufferRef& RayAllocatorBuffer,
	const FRDGBufferRef& TraceDataPackedBuffer,
	FRDGBufferRef& OutputRayAllocatorBuffer,
	FRDGBufferRef& OutputTraceDataPackedBuffer
);

void LumenHWRTBucketRaysByMaterialID(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	int32 RayCount,
	FRDGBufferRef& RayAllocatorBuffer,
	FRDGBufferRef& TraceDataPackedBuffer
);


// Pass helpers
template<typename TShaderClass>
static void AddLumenRayTraceDispatchPass(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& PassName,
	const TShaderRef<TShaderClass>& RayGenerationShader,
	typename TShaderClass::FParameters* Parameters,
	FIntPoint Resolution,
	const FViewInfo& View,
	bool bUseMinimalPayload)
{
	ClearUnusedGraphResources(RayGenerationShader, Parameters);

	GraphBuilder.AddPass(
		Forward<FRDGEventName>(PassName),
		Parameters,
		ERDGPassFlags::Compute,
		[Parameters, &View, RayGenerationShader, bUseMinimalPayload, Resolution](FRHIRayTracingCommandList& RHICmdList)
		{
			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenerationShader, *Parameters);

			FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
			FRayTracingPipelineState* Pipeline = bUseMinimalPayload ? View.LumenHardwareRayTracingMaterialPipeline : View.RayTracingMaterialPipeline;

			RHICmdList.RayTraceDispatch(Pipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources,
					Resolution.X, Resolution.Y);
		}
	);
}

template<typename TShaderClass>
static void AddLumenRayTraceDispatchIndirectPass(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& PassName,
	const TShaderRef<TShaderClass>& RayGenerationShader,
	typename TShaderClass::FParameters* Parameters,
	FRDGBufferRef IndirectArgsBuffer,
	uint32 IndirectArgsOffset,
	const FViewInfo& View,
	bool bUseMinimalPayload)
{
	ClearUnusedGraphResources(RayGenerationShader, Parameters, { IndirectArgsBuffer });

	GraphBuilder.AddPass(
		Forward<FRDGEventName>(PassName),
		Parameters,
		ERDGPassFlags::Compute,
		[Parameters, &View, RayGenerationShader, bUseMinimalPayload, IndirectArgsBuffer, IndirectArgsOffset](FRHIRayTracingCommandList& RHICmdList)
		{
			IndirectArgsBuffer->MarkResourceAsUsed();

			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenerationShader, *Parameters);

			FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
			FRayTracingPipelineState* Pipeline = bUseMinimalPayload ? View.LumenHardwareRayTracingMaterialPipeline : View.RayTracingMaterialPipeline;

			RHICmdList.RayTraceDispatchIndirect(Pipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources,
				IndirectArgsBuffer->GetIndirectRHICallBuffer(), IndirectArgsOffset);
		}
	);
}

#endif // RHI_RAYTRACING