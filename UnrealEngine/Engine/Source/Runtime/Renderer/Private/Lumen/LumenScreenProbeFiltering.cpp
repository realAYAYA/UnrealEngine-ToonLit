// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenScreenProbeFiltering.cpp
=============================================================================*/

#include "LumenScreenProbeGather.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"

int32 GLumenScreenProbeSpatialFilterNumPasses = 3;
FAutoConsoleVariableRef GVarLumenScreenProbeSpatialFilterNumPasses(
	TEXT("r.Lumen.ScreenProbeGather.SpatialFilterNumPasses"),
	GLumenScreenProbeSpatialFilterNumPasses,
	TEXT("Number of spatial filter passes"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeSpatialFilterHalfKernelSize = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeSpatialFilterHalfKernelSize(
	TEXT("r.Lumen.ScreenProbeGather.SpatialFilterHalfKernelSize"),
	GLumenScreenProbeSpatialFilterHalfKernelSize,
	TEXT("Experimental"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeFilterMaxRadianceHitAngle = 10.0f;
FAutoConsoleVariableRef GVarLumenScreenProbeFilterMaxRadianceHitAngle(
	TEXT("r.Lumen.ScreenProbeGather.SpatialFilterMaxRadianceHitAngle"),
	GLumenScreenProbeFilterMaxRadianceHitAngle,
	TEXT("In Degrees.  Larger angles allow more filtering but lose contact shadows."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenFilterPositionWeightScale = 1000.0f;
FAutoConsoleVariableRef GVarLumenScreenFilterPositionWeightScale(
	TEXT("r.Lumen.ScreenProbeGather.SpatialFilterPositionWeightScale"),
	GLumenScreenFilterPositionWeightScale,
	TEXT("Determines how far probes can be in world space while still filtering lighting"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeGatherNumMips = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeGatherNumMips(
	TEXT("r.Lumen.ScreenProbeGather.GatherNumMips"),
	GLumenScreenProbeGatherNumMips,
	TEXT("Number of mip maps to prepare for diffuse integration"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeGatherMaxRayIntensity = 40;
FAutoConsoleVariableRef GVarLumenScreenProbeGatherMaxRayIntensity(
	TEXT("r.Lumen.ScreenProbeGather.MaxRayIntensity"),
	GLumenScreenProbeGatherMaxRayIntensity,
	TEXT("Clamps the maximum ray lighting intensity (with PreExposure) to reduce fireflies."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeTemporalFilterProbesHistoryDistanceThreshold = 30;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalFilterProbesHistoryDistanceThreshold(
	TEXT("r.Lumen.ScreenProbeGather.TemporalFilterProbes.HistoryDistanceThreshold"),
	GLumenScreenProbeTemporalFilterProbesHistoryDistanceThreshold,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GLumenScreenProbeTemporalFilterProbesHistoryWeight = .5f;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalFilterProbesHistoryWeight(
	TEXT("r.Lumen.ScreenProbeGather.TemporalFilterProbes.HistoryWeight"),
	GLumenScreenProbeTemporalFilterProbesHistoryWeight,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeTemporalDebugForceTracesMoving = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalForceTracesMoving(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.DebugForceTracesMoving"),
	GLumenScreenProbeTemporalDebugForceTracesMoving,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeFilteringWaveOps = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeFilteringWaveOps(
	TEXT("r.Lumen.ScreenProbeGather.Filtering.WaveOps"),
	GLumenScreenProbeFilteringWaveOps,
	TEXT("Whether to use Wave Ops path for screen probe filtering."),
	ECVF_RenderThreadSafe
);

class FScreenProbeCompositeTracesWithScatterCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeCompositeTracesWithScatterCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeCompositeTracesWithScatterCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeTraceMoving)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, MaxRayIntensity)
	END_SHADER_PARAMETER_STRUCT()

	static uint32 GetThreadGroupSize(uint32 GatherResolution)
	{
		if (GatherResolution <= 8)
		{
			return 8;
		}
		else if (GatherResolution <= 16)
		{
			return 16;
		}
		else if (GatherResolution <= 32)
		{
			return 32;
		}
		else
		{
			return MAX_uint32;
		}
	}

	class FThreadGroupSize : SHADER_PERMUTATION_SPARSE_INT("THREADGROUP_SIZE", 8, 16, 32);
	class FStructuredImportanceSampling : SHADER_PERMUTATION_BOOL("STRUCTURED_IMPORTANCE_SAMPLING");
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize, FStructuredImportanceSampling>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeCompositeTracesWithScatterCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeCompositeTracesWithScatterCS", SF_Compute);


class FScreenProbeTemporallyAccumulateTraceRadianceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeTemporallyAccumulateTraceRadianceCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeTemporallyAccumulateTraceRadianceCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeRadiance)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(float, ProbeTemporalFilterHistoryWeight)
		SHADER_PARAMETER(float, HistoryDistanceThreshold)
		SHADER_PARAMETER(float, PrevInvPreExposure)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, HistoryScreenProbeSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, HistoryScreenProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, HistoryScreenProbeTranslatedWorldPosition)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeTemporallyAccumulateTraceRadianceCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeTemporallyAccumulateTraceRadianceCS", SF_Compute);


class FScreenProbeFilterGatherTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeFilterGatherTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeFilterGatherTracesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ScreenProbeMoving)
		SHADER_PARAMETER(float, SpatialFilterMaxRadianceHitAngle)
		SHADER_PARAMETER(float, SpatialFilterPositionWeightScale)
		SHADER_PARAMETER(int32, SpatialFilterHalfKernelSize)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeFilterGatherTracesCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeFilterGatherTracesCS", SF_Compute);


class FScreenProbeInjectLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeInjectLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeInjectLightSamplesCS, FGlobalShader)

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeRadiance)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeInjectLightSamplesCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeInjectLightSamplesCS", SF_Compute);


class FScreenProbeConvertToIrradianceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeConvertToIrradianceCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeConvertToIrradianceCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeRadianceSHAmbient)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWScreenProbeRadianceSHDirectional)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeIrradianceWithBorder)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeRadiance)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FWaveOpWaveSize>() > 0 && !RHISupportsWaveOperations(Parameters.Platform))
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetThreadGroupSize(uint32 GatherResolution)
	{
		if (GatherResolution <= 8)
		{
			return 8;
		}
		else if (GatherResolution <= 16)
		{
			return 16;
		}
		else
		{
			return MAX_uint32;
		}
	}

	class FThreadGroupSize : SHADER_PERMUTATION_SPARSE_INT("THREADGROUP_SIZE", 8, 16);
	class FWaveOpWaveSize : SHADER_PERMUTATION_SPARSE_INT("WAVE_OP_WAVE_SIZE", 0, 32, 64);
	class FProbeIrradianceFormat : SHADER_PERMUTATION_ENUM_CLASS("PROBE_IRRADIANCE_FORMAT", EScreenProbeIrradianceFormat);
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize, FWaveOpWaveSize, FProbeIrradianceFormat>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FWaveOpWaveSize>() > 0)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeConvertToIrradianceCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeConvertToIrradianceCS", SF_Compute);


class FScreenProbeCalculateMovingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeCalculateMovingCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeCalculateMovingCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float>, RWScreenProbeMoving)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeTraceMoving)
		SHADER_PARAMETER(float, DebugForceTracesMoving)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetThreadGroupSize(uint32 GatherResolution)
	{
		if (GatherResolution <= 4)
		{
			return 4;
		}
		else if (GatherResolution <= 8)
		{
			return 8;
		}
		else if (GatherResolution <= 16)
		{
			return 16;
		}
		else
		{
			return MAX_uint32;
		}
	}

	class FThreadGroupSize : SHADER_PERMUTATION_SPARSE_INT("THREADGROUP_SIZE", 4, 8, 16);
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize>;
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeCalculateMovingCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeCalculateMovingCS", SF_Compute);


class FScreenProbeFixupBordersCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeFixupBordersCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeFixupBordersCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenProbeRadiance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeFixupBordersCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeFixupBordersCS", SF_Compute);


class FScreenProbeGenerateMipLevelCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeGenerateMipLevelCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeGenerateMipLevelCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeRadianceWithBorderMip)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ScreenProbeRadianceWithBorderParentMip)
		SHADER_PARAMETER(uint32, MipLevel)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeGenerateMipLevelCS, "/Engine/Private/Lumen/LumenScreenProbeFiltering.usf", "ScreenProbeGenerateMipLevelCS", SF_Compute);

void FilterScreenProbes(
	FRDGBuilder& GraphBuilder, 
	const FViewInfo& View, 
	const FSceneTextures& SceneTextures,
	const FScreenProbeParameters& ScreenProbeParameters,
	bool bRenderDirectLighting,
	FScreenProbeGatherParameters& GatherParameters,
	ERDGPassFlags ComputePassFlags)
{
	const FIntPoint ScreenProbeGatherBufferSize = ScreenProbeParameters.ScreenProbeAtlasBufferSize * ScreenProbeParameters.ScreenProbeGatherOctahedronResolution;
	FRDGTextureDesc ScreenProbeRadianceDesc(FRDGTextureDesc::Create2D(ScreenProbeGatherBufferSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	FRDGTextureRef ScreenProbeRadiance = GraphBuilder.CreateTexture(ScreenProbeRadianceDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeRadiance"));

	FRDGTextureDesc ScreenProbeHitDistanceDesc(FRDGTextureDesc::Create2D(ScreenProbeGatherBufferSize, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	FRDGTextureRef ScreenProbeHitDistance = GraphBuilder.CreateTexture(ScreenProbeHitDistanceDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeHitDistance"));
	FRDGTextureRef ScreenProbeTraceMoving = GraphBuilder.CreateTexture(ScreenProbeHitDistanceDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeTraceMoving"));

	{
		const uint32 CompositeScatterThreadGroupSize = FScreenProbeCompositeTracesWithScatterCS::GetThreadGroupSize(FMath::Max(ScreenProbeParameters.ScreenProbeGatherOctahedronResolution, ScreenProbeParameters.ScreenProbeTracingOctahedronResolution));
		ensureMsgf(CompositeScatterThreadGroupSize != MAX_uint32, TEXT("Missing permutation for FScreenProbeCompositeTracesWithScatterCS"));
		FScreenProbeCompositeTracesWithScatterCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeCompositeTracesWithScatterCS::FParameters>();
		PassParameters->RWScreenProbeRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeRadiance));
		PassParameters->RWScreenProbeHitDistance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeHitDistance));
		PassParameters->RWScreenProbeTraceMoving = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeTraceMoving));
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;
		PassParameters->View = View.ViewUniformBuffer;
		// This is used to quantize to uint during compositing, prevent poor precision
		PassParameters->MaxRayIntensity = FMath::Min(GLumenScreenProbeGatherMaxRayIntensity, 100000.0f);

		FScreenProbeCompositeTracesWithScatterCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FScreenProbeCompositeTracesWithScatterCS::FThreadGroupSize >(CompositeScatterThreadGroupSize);
		PermutationVector.Set< FScreenProbeCompositeTracesWithScatterCS::FStructuredImportanceSampling >(LumenScreenProbeGather::UseImportanceSampling(View));
		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeCompositeTracesWithScatterCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompositeTraces"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ScreenProbeParameters.ProbeIndirectArgs,
			(uint32)EScreenProbeIndirectArgs::GroupPerProbe * sizeof(FRHIDispatchIndirectParameters));
	}

	const uint32 CalculateMovingThreadGroupSize = FScreenProbeCalculateMovingCS::GetThreadGroupSize(ScreenProbeParameters.ScreenProbeGatherOctahedronResolution);
	
	FRDGTextureDesc ScreenProbeMovingDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	FRDGTextureRef ScreenProbeMoving = GraphBuilder.CreateTexture(ScreenProbeMovingDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeMoving"));

	ensureMsgf(CalculateMovingThreadGroupSize != MAX_uint32, TEXT("Unsupported gather resolution"));

	{
		FScreenProbeCalculateMovingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeCalculateMovingCS::FParameters>();
		PassParameters->RWScreenProbeMoving = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeMoving));
		PassParameters->ScreenProbeTraceMoving = ScreenProbeTraceMoving;
		PassParameters->DebugForceTracesMoving = GLumenScreenProbeTemporalDebugForceTracesMoving != 0 ? 1.0f : 0.0f;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		FScreenProbeCalculateMovingCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FScreenProbeCalculateMovingCS::FThreadGroupSize >(CalculateMovingThreadGroupSize);
		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeCalculateMovingCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CalculateMoving"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ScreenProbeParameters.ProbeIndirectArgs,
			(uint32)EScreenProbeIndirectArgs::GroupPerProbe * sizeof(FRHIDispatchIndirectParameters));
	}

	const FRDGTextureRef CompositedScreenProbeRadiance = ScreenProbeRadiance;

	const FScreenProbeGatherTemporalState& ScreenProbeGatherState = View.ViewState->Lumen.ScreenProbeGatherState;

	const bool bUseProbeTemporalFilter = LumenScreenProbeGather::UseProbeTemporalFilter()
		&& ScreenProbeGatherState.ProbeHistoryScreenProbeRadiance.IsValid()
		&& ScreenProbeGatherState.HistoryScreenProbeTranslatedWorldPosition.IsValid()
		&& !View.bCameraCut 
		&& !View.bPrevTransformsReset
		&& ScreenProbeGatherState.ProbeHistoryScreenProbeRadiance->GetDesc().Extent == ScreenProbeRadianceDesc.Extent;

	if (bUseProbeTemporalFilter)
	{
		FRDGTextureRef TemporallyFilteredScreenProbeRadiance = GraphBuilder.CreateTexture(ScreenProbeRadianceDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeTemporallyFilteredRadiance"));

		FScreenProbeTemporallyAccumulateTraceRadianceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeTemporallyAccumulateTraceRadianceCS::FParameters>();
		PassParameters->RWScreenProbeRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TemporallyFilteredScreenProbeRadiance));
		PassParameters->ScreenProbeRadiance = ScreenProbeRadiance;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		PassParameters->PrevInvPreExposure = 1.0f / View.PrevViewInfo.SceneColorPreExposure;
		PassParameters->HistoryScreenPositionScaleBias = ScreenProbeGatherState.ProbeHistoryScreenPositionScaleBias;

		const FIntPoint SceneTexturesExtent = View.GetSceneTexturesConfig().Extent;
		const FVector2D InvBufferSize(1.0f / SceneTexturesExtent.X, 1.0f / SceneTexturesExtent.Y);

		// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
		PassParameters->HistoryUVMinMax = FVector4f(
			(ScreenProbeGatherState.ProbeHistoryViewRect.Min.X + 0.5f) * InvBufferSize.X,
			(ScreenProbeGatherState.ProbeHistoryViewRect.Min.Y + 0.5f) * InvBufferSize.Y,
			(ScreenProbeGatherState.ProbeHistoryViewRect.Max.X - 0.5f) * InvBufferSize.X,
			(ScreenProbeGatherState.ProbeHistoryViewRect.Max.Y - 0.5f) * InvBufferSize.Y);

		PassParameters->HistoryDistanceThreshold = GLumenScreenProbeTemporalFilterProbesHistoryDistanceThreshold;
		PassParameters->HistoryScreenProbeRadiance = GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.ProbeHistoryScreenProbeRadiance);
		PassParameters->HistoryScreenProbeSceneDepth = GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.HistoryScreenProbeSceneDepth);
		PassParameters->HistoryScreenProbeTranslatedWorldPosition = GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.HistoryScreenProbeTranslatedWorldPosition);
		PassParameters->ProbeTemporalFilterHistoryWeight = GLumenScreenProbeTemporalFilterProbesHistoryWeight;

		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeTemporallyAccumulateTraceRadianceCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TemporallyAccumulateRadiance"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ScreenProbeParameters.ProbeIndirectArgs,
			(uint32)EScreenProbeIndirectArgs::ThreadPerGather * sizeof(FRHIDispatchIndirectParameters));

		ScreenProbeRadiance = TemporallyFilteredScreenProbeRadiance;
	}

	if (LumenScreenProbeGather::UseProbeTemporalFilter() && !View.bStatePrevViewInfoIsReadOnly)
	{
		FScreenProbeGatherTemporalState& ScreenProbeGatherWriteableState = View.ViewState->Lumen.ScreenProbeGatherState;
		ScreenProbeGatherWriteableState.ProbeHistoryScreenProbeRadiance = GraphBuilder.ConvertToExternalTexture(CompositedScreenProbeRadiance);
	}

	if (LumenScreenProbeGather::UseProbeSpatialFilter() && GLumenScreenProbeSpatialFilterHalfKernelSize > 0)
	{
		for (int32 PassIndex = 0; PassIndex < GLumenScreenProbeSpatialFilterNumPasses; PassIndex++)
		{
			FRDGTextureRef FilteredScreenProbeRadiance = GraphBuilder.CreateTexture(ScreenProbeRadianceDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeFilteredRadiance"));

			FScreenProbeFilterGatherTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeFilterGatherTracesCS::FParameters>();
			PassParameters->RWScreenProbeRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(FilteredScreenProbeRadiance));
			PassParameters->ScreenProbeRadiance = ScreenProbeRadiance;
			PassParameters->ScreenProbeHitDistance = ScreenProbeHitDistance;
			PassParameters->ScreenProbeMoving = ScreenProbeMoving;
			PassParameters->SpatialFilterMaxRadianceHitAngle = FMath::Clamp<float>(GLumenScreenProbeFilterMaxRadianceHitAngle * PI / 180.0f, 0.0f, PI);
			PassParameters->SpatialFilterPositionWeightScale = GLumenScreenFilterPositionWeightScale;
			PassParameters->SpatialFilterHalfKernelSize = GLumenScreenProbeSpatialFilterHalfKernelSize;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->ScreenProbeParameters = ScreenProbeParameters;

			auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeFilterGatherTracesCS>(0);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("FilterRadianceWithGather"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				ScreenProbeParameters.ProbeIndirectArgs,
				(uint32)EScreenProbeIndirectArgs::ThreadPerGather * sizeof(FRHIDispatchIndirectParameters));

			ScreenProbeRadiance = FilteredScreenProbeRadiance;
		}
	}

	if (!View.bStatePrevViewInfoIsReadOnly)
	{
		FScreenProbeGatherTemporalState& ScreenProbeGatherWriteableState = View.ViewState->Lumen.ScreenProbeGatherState;
		ScreenProbeGatherWriteableState.ImportanceSamplingHistoryScreenProbeRadiance = GraphBuilder.ConvertToExternalTexture(ScreenProbeRadiance);
		ScreenProbeGatherWriteableState.HistoryScreenProbeSceneDepth = GraphBuilder.ConvertToExternalTexture(ScreenProbeParameters.ScreenProbeSceneDepth);
		ScreenProbeGatherWriteableState.HistoryScreenProbeTranslatedWorldPosition = GraphBuilder.ConvertToExternalTexture(ScreenProbeParameters.ScreenProbeTranslatedWorldPosition);
		ScreenProbeGatherWriteableState.ProbeHistoryViewRect = View.ViewRect;
		ScreenProbeGatherWriteableState.ProbeHistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);
	}

	extern int32 GLumenScreenProbeInjectLightsToProbes;
	if (bRenderDirectLighting && GLumenScreenProbeInjectLightsToProbes != 0)
	{
		FRDGTextureRef ScreenProbeRadianceWithLightsInjected = GraphBuilder.CreateTexture(ScreenProbeRadianceDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeRadianceWithLightsInjected"));

		FScreenProbeInjectLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeInjectLightSamplesCS::FParameters>();
		PassParameters->RWScreenProbeRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeRadianceWithLightsInjected));
		PassParameters->ScreenProbeRadiance = ScreenProbeRadiance;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeInjectLightSamplesCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InjectLightSamples"),
			ComputeShader,
			PassParameters,
			ScreenProbeParameters.ProbeIndirectArgs,
			(uint32)EScreenProbeIndirectArgs::ThreadPerGather * sizeof(FRHIDispatchIndirectParameters));

		ScreenProbeRadiance = ScreenProbeRadianceWithLightsInjected;
	}

	const uint32 ConvertToSHThreadGroupSize = FScreenProbeConvertToIrradianceCS::GetThreadGroupSize(ScreenProbeParameters.ScreenProbeGatherOctahedronResolution);

	FRDGTextureRef ScreenProbeRadianceSHAmbient = nullptr;
	FRDGTextureRef ScreenProbeRadianceSHDirectional = nullptr;
	FRDGTextureRef ScreenProbeIrradianceWithBorder = nullptr;

	const EScreenProbeIrradianceFormat ScreenProbeIrradianceFormat = LumenScreenProbeGather::GetScreenProbeIrradianceFormat(View.Family->EngineShowFlags);
	if (ScreenProbeIrradianceFormat == EScreenProbeIrradianceFormat::SH3)
	{
		FRDGTextureDesc ScreenProbeRadianceSHAmbientDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
		ScreenProbeRadianceSHAmbient = GraphBuilder.CreateTexture(ScreenProbeRadianceSHAmbientDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeRadianceSHAmbient"));

		const FIntPoint ProbeRadianceSHDirectionalBufferSize(ScreenProbeParameters.ScreenProbeAtlasBufferSize.X * 6, ScreenProbeParameters.ScreenProbeAtlasBufferSize.Y);
		FRDGTextureDesc ScreenProbeRadianceSHDirectionalDesc(FRDGTextureDesc::Create2D(ProbeRadianceSHDirectionalBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
		ScreenProbeRadianceSHDirectional = GraphBuilder.CreateTexture(ScreenProbeRadianceSHDirectionalDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeRadianceSHDirectional"));
	}
	else
	{
		const FIntPoint ScreenProbeIrradianceWithBorderBufferSize(ScreenProbeParameters.ScreenProbeAtlasBufferSize.X * LumenScreenProbeGather::IrradianceProbeWithBorderRes, ScreenProbeParameters.ScreenProbeAtlasBufferSize.Y * LumenScreenProbeGather::IrradianceProbeWithBorderRes);
		FRDGTextureDesc ScreenProbeIrradianceWithBorderDesc(FRDGTextureDesc::Create2D(ScreenProbeIrradianceWithBorderBufferSize, PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
		ScreenProbeIrradianceWithBorder = GraphBuilder.CreateTexture(ScreenProbeIrradianceWithBorderDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeIrradianceWithBorder"));
	}

	if (ConvertToSHThreadGroupSize != MAX_uint32)
	{
		FScreenProbeConvertToIrradianceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeConvertToIrradianceCS::FParameters>();
		PassParameters->RWScreenProbeRadianceSHAmbient = ScreenProbeRadianceSHAmbient ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeRadianceSHAmbient)) : nullptr;
		PassParameters->RWScreenProbeRadianceSHDirectional = ScreenProbeRadianceSHDirectional ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeRadianceSHDirectional)) : nullptr;
		PassParameters->RWScreenProbeIrradianceWithBorder = ScreenProbeIrradianceWithBorder ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeIrradianceWithBorder)) : nullptr;
		PassParameters->ScreenProbeRadiance = ScreenProbeRadiance;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		int32 WaveOpWaveSize = 0;

		if (GLumenScreenProbeFilteringWaveOps != 0
			&& GRHISupportsWaveOperations
			&& RHISupportsWaveOperations(View.GetShaderPlatform())
			&& ConvertToSHThreadGroupSize * ConvertToSHThreadGroupSize <= 64)
		{
			// 64 wave size is preferred for FScreenProbeConvertToIrradianceCS
			if (GRHIMinimumWaveSize <= 64 && GRHIMaximumWaveSize >= 64)
			{
				WaveOpWaveSize = 64;
			}
			else if (GRHIMinimumWaveSize <= 32 && GRHIMaximumWaveSize >= 32)
			{
				WaveOpWaveSize = 32;
			}
		}

		FScreenProbeConvertToIrradianceCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FScreenProbeConvertToIrradianceCS::FThreadGroupSize >(ConvertToSHThreadGroupSize);
		PermutationVector.Set< FScreenProbeConvertToIrradianceCS::FWaveOpWaveSize>(WaveOpWaveSize);
		PermutationVector.Set< FScreenProbeConvertToIrradianceCS::FProbeIrradianceFormat >(ScreenProbeIrradianceFormat);
		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeConvertToIrradianceCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ScreenProbeConvertToIrradiance Format:%d WaveOpWaveSize:%d", (int32)ScreenProbeIrradianceFormat, WaveOpWaveSize),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ScreenProbeParameters.ProbeIndirectArgs,
			(uint32)EScreenProbeIndirectArgs::GroupPerProbe * sizeof(FRHIDispatchIndirectParameters));
	}

	FRDGTextureRef ScreenProbeRadianceWithBorder;
	{
		const FIntPoint ScreenProbeGatherWithBorderBufferSize = ScreenProbeParameters.ScreenProbeAtlasBufferSize * ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder;
		FRDGTextureDesc ScreenProbeRadianceWithBorderDesc(FRDGTextureDesc::Create2D(ScreenProbeGatherWithBorderBufferSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, GLumenScreenProbeGatherNumMips));
		ScreenProbeRadianceWithBorder = GraphBuilder.CreateTexture(ScreenProbeRadianceWithBorderDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeFilteredRadianceWithBorder"));

		FScreenProbeFixupBordersCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeFixupBordersCS::FParameters>();
		PassParameters->RWScreenProbeRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeRadianceWithBorder));
		PassParameters->ScreenProbeRadiance = ScreenProbeRadiance;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeFixupBordersCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FixupBorders"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			ScreenProbeParameters.ProbeIndirectArgs,
			(uint32)EScreenProbeIndirectArgs::ThreadPerGatherWithBorder * sizeof(FRHIDispatchIndirectParameters));
	}

	for (int32 MipLevel = 1; MipLevel < GLumenScreenProbeGatherNumMips; MipLevel++)
	{
		FScreenProbeGenerateMipLevelCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeGenerateMipLevelCS::FParameters>();
		PassParameters->RWScreenProbeRadianceWithBorderMip = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeRadianceWithBorder, MipLevel));
		PassParameters->ScreenProbeRadianceWithBorderParentMip = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(ScreenProbeRadianceWithBorder, MipLevel - 1));
		PassParameters->MipLevel = MipLevel;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;
		PassParameters->View = View.ViewUniformBuffer;

		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeGenerateMipLevelCS>();

		const uint32 MipSize = ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder >> MipLevel;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GenerateMip"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(ScreenProbeParameters.ScreenProbeAtlasViewSize * MipSize, FScreenProbeGenerateMipLevelCS::GetGroupSize()));
	}

	GatherParameters.ScreenProbeRadiance = ScreenProbeRadiance;
	GatherParameters.ScreenProbeRadianceWithBorder = ScreenProbeRadianceWithBorder;
	GatherParameters.ScreenProbeRadianceSHAmbient =  ScreenProbeRadianceSHAmbient;
	GatherParameters.ScreenProbeRadianceSHDirectional = ScreenProbeRadianceSHDirectional;
	GatherParameters.ScreenProbeIrradianceWithBorder = ScreenProbeIrradianceWithBorder;
	GatherParameters.ScreenProbeMoving = ScreenProbeMoving;
}
