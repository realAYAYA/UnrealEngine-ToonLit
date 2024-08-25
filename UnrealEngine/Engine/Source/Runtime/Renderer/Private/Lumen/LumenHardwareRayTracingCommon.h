// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"
#include "GlobalShader.h"
#include "Lumen/LumenTracingUtils.h"
#include "RayTracing/RayTracingLighting.h"
#include "RayTracingPayloadType.h"
#include "SceneTextureParameters.h"
#include "Substrate/Substrate.h" 

namespace LumenHardwareRayTracing
{
	bool IsInlineSupported();
	bool IsRayGenSupported();
}

#if RHI_RAYTRACING

namespace Lumen
{
	enum class EHardwareRayTracingLightingMode;

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
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_SRV(StructuredBuffer, RayTracingSceneMetadata)

		// Lighting structures
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRayTracingLightGrid, LightGridParameters)
		SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, Forward)

		// Surface cache
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)

		// Inline data
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<Lumen::FHitGroupRootConstants>, HitGroupData)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	FLumenHardwareRayTracingShaderBase();
	FLumenHardwareRayTracingShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	
	class FUseThreadGroupSize64 : SHADER_PERMUTATION_BOOL("RAY_TRACING_USE_THREAD_GROUP_SIZE_64");
	using FBasePermutationDomain = TShaderPermutationDomain<FUseThreadGroupSize64>;
	using FPermutationDomain = TShaderPermutationDomain<FBasePermutationDomain>; // The default that is used if derived classes don't define their own

	static constexpr const Lumen::ERayTracingShaderDispatchSize DispatchSize = Lumen::ERayTracingShaderDispatchSize::DispatchSize2D;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ESurfaceCacheSampling SurfaceCacheSampling, FShaderCompilerEnvironment& OutEnvironment);

	static void ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ERayTracingShaderDispatchSize Size, bool UseThreadGroupSize64, FShaderCompilerEnvironment& OutEnvironment);

	static FIntPoint GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ERayTracingShaderDispatchSize ShaderDispatchSize, bool UseThreadGroupSize64);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType);

	static bool UseThreadGroupSize64(EShaderPlatform ShaderPlatform);
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
		{ \
			FPermutationDomain PermutationVector(Parameters.PermutationId); \
			if (PermutationVector.Get<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>().Get<FUseThreadGroupSize64>() && !RHISupportsWaveSize64(Parameters.Platform)) \
			{ \
				return false; \
			} \
			return ShaderClass::ShouldCompilePermutation(Parameters, Lumen::ERayTracingShaderDispatchType::Inline); \
		}\
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)\
		{ \
			FPermutationDomain PermutationVector(Parameters.PermutationId); \
			const bool UseThreadGroupSize64 = PermutationVector.Get<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>().Get<FUseThreadGroupSize64>() ; \
			FIntPoint Size = GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::Inline, DispatchSize, UseThreadGroupSize64); \
			OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_X"), Size.X); \
			OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_Y"), Size.Y); \
			ShaderClass::ModifyCompilationEnvironment(Parameters, Lumen::ERayTracingShaderDispatchType::Inline, OutEnvironment); \
			ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchType::Inline, DispatchSize, UseThreadGroupSize64, OutEnvironment); \
		}\
		static FPermutationDomain MakePermutationVector(ShaderClass::FPermutationDomain PermutationVector, EShaderPlatform ShaderPlatform) \
		{ \
			FLumenHardwareRayTracingShaderBase::FBasePermutationDomain Base; \
			Base.Set<FUseThreadGroupSize64>(UseThreadGroupSize64(ShaderPlatform)); \
			PermutationVector.Set<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>(Base); \
			return PermutationVector; \
		} \
		static FIntPoint GetThreadGroupSize(EShaderPlatform ShaderPlatform) { return GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::Inline, DispatchSize, UseThreadGroupSize64(ShaderPlatform)); } \
		static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId) { return static_cast<ERayTracingPayloadType>(0); } \
		static void AddLumenRayTracingDispatchIndirect(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FRDGBufferRef IndirectArgsBuffer, uint32 IndirectArgsOffset, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##CS> ComputeShader = View.ShaderMap->GetShader<ShaderClass##CS>(MakePermutationVector(PermutationVector, View.GetShaderPlatform())); \
			FComputeShaderUtils::AddPass(GraphBuilder, std::move(EventName), ComputePassFlags, ComputeShader, PassParameters, IndirectArgsBuffer, IndirectArgsOffset); \
		} \
		static void AddLumenRayTracingDispatch(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FIntVector GroupCount, ERDGPassFlags ComputePassFlags) \
		{ \
			TShaderRef<ShaderClass##CS> ComputeShader = View.ShaderMap->GetShader<ShaderClass##CS>(MakePermutationVector(PermutationVector, View.GetShaderPlatform())); \
			FComputeShaderUtils::AddPass(GraphBuilder, std::move(EventName), ComputePassFlags, ComputeShader, PassParameters, GroupCount); \
		} \
	};

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

#define IMPLEMENT_LUMEN_RAYGEN_RAYTRACING_SHADER(ShaderClass) \
	class ShaderClass##RGS : public ShaderClass \
	{ \
		DECLARE_GLOBAL_SHADER(ShaderClass##RGS) \
		SHADER_USE_ROOT_PARAMETER_STRUCT(ShaderClass##RGS, ShaderClass) \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) \
		{ \
			FPermutationDomain PermutationVector(Parameters.PermutationId); \
			if (PermutationVector.Get<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain>().Get<FUseThreadGroupSize64>()) \
			{ \
				return false; /* Wave 64 is only relevant for CS */ \
			} \
			return ShaderClass::ShouldCompilePermutation(Parameters, Lumen::ERayTracingShaderDispatchType::RayGen); \
		} \
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment) \
		{ \
			ShaderClass::ModifyCompilationEnvironment(Parameters, Lumen::ERayTracingShaderDispatchType::RayGen, OutEnvironment); \
			ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchType::RayGen, DispatchSize, false, OutEnvironment); \
		} \
		static FIntPoint GetThreadGroupSize() { return GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType::RayGen, DispatchSize, false); } \
		static void AddLumenRayTracingDispatchIndirect(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FRDGBufferRef IndirectArgsBuffer, uint32 IndirectArgsOffset, bool bUseMinimalPayload) \
		{ \
			TShaderRef<ShaderClass##RGS> RayGenerationShader = View.ShaderMap->GetShader<ShaderClass##RGS>(PermutationVector); \
			AddLumenRayTraceDispatchIndirectPass(GraphBuilder, std::move(EventName), RayGenerationShader, PassParameters, IndirectArgsBuffer, IndirectArgsOffset, View, bUseMinimalPayload); \
		} \
		static void AddLumenRayTracingDispatch(FRDGBuilder& GraphBuilder, FRDGEventName&& EventName, const FViewInfo& View, ShaderClass::FPermutationDomain PermutationVector, \
			ShaderClass::FParameters* PassParameters, FIntPoint DispatchResolution, bool bUseMinimalPayload) \
		{ \
			TShaderRef<ShaderClass##RGS> RayGenerationShader = View.ShaderMap->GetShader<ShaderClass##RGS>(PermutationVector); \
			AddLumenRayTraceDispatchPass(GraphBuilder, std::move(EventName), RayGenerationShader, PassParameters, DispatchResolution, View, bUseMinimalPayload); \
		} \
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
	const FLumenCardTracingParameters& TracingParameters,
	FLumenHardwareRayTracingShaderBase::FSharedParameters* SharedParameters);

#endif // RHI_RAYTRACING

BEGIN_UNIFORM_BUFFER_STRUCT(FLumenHardwareRayTracingUniformBufferParameters, )
	SHADER_PARAMETER(float, SkipBackFaceHitDistance)
	SHADER_PARAMETER(float, SkipTwoSidedHitDistance)
	SHADER_PARAMETER(float, SkipTranslucent)
END_UNIFORM_BUFFER_STRUCT()