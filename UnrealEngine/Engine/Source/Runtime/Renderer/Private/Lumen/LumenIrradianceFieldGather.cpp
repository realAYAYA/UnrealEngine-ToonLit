// Copyright Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "ScreenSpaceDenoise.h"
#include "LumenRadianceCache.h"
#include "LumenTracingUtils.h"
#include "LumenReflections.h"

int32 GLumenIrradianceFieldGather = 0;
FAutoConsoleVariableRef CVarLumenIrradianceFieldGather(
	TEXT("r.Lumen.IrradianceFieldGather"),
	GLumenIrradianceFieldGather,
	TEXT("Whether to use the Irradiance Field Final Gather, an experimental opaque final gather that interpolates from pre-calculated irradiance in probes for cheaper, but lower quality GI."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldNumClipmaps = 4;
FAutoConsoleVariableRef CVarLumenIrradianceFieldNumClipmaps(
	TEXT("r.Lumen.IrradianceFieldGather.NumClipmaps"),
	GLumenIrradianceFieldNumClipmaps,
	TEXT("Number of radiance cache clipmaps."),
	ECVF_RenderThreadSafe
);

float GLumenIrradianceFieldClipmapWorldExtent = 5000.0f;
FAutoConsoleVariableRef CVarLumenIrradianceFieldClipmapWorldExtent(
	TEXT("r.Lumen.IrradianceFieldGather.ClipmapWorldExtent"),
	GLumenIrradianceFieldClipmapWorldExtent,
	TEXT("World space extent of the first clipmap"),
	ECVF_RenderThreadSafe
);

float GLumenIrradianceFieldClipmapDistributionBase = 2.0f;
FAutoConsoleVariableRef CVarLumenIrradianceFieldClipmapDistributionBase(
	TEXT("r.Lumen.IrradianceFieldGather.ClipmapDistributionBase"),
	GLumenIrradianceFieldClipmapDistributionBase,
	TEXT("Base of the Pow() that controls the size of each successive clipmap relative to the first."),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldNumProbesToTraceBudget = 200;
FAutoConsoleVariableRef CVarLumenIrradianceFieldNumProbesToTraceBudget(
	TEXT("r.Lumen.IrradianceFieldGather.NumProbesToTraceBudget"),
	GLumenIrradianceFieldNumProbesToTraceBudget,
	TEXT("Number of probes that can be updated in a frame before downsampling."),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldGridResolution = 64;
FAutoConsoleVariableRef CVarLumenIrradianceFieldResolution(
	TEXT("r.Lumen.IrradianceFieldGather.GridResolution"),
	GLumenIrradianceFieldGridResolution,
	TEXT("Resolution of the probe placement grid within each clipmap"),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldProbeResolution = 16;
FAutoConsoleVariableRef CVarLumenIrradianceFieldProbeResolution(
	TEXT("r.Lumen.IrradianceFieldGather.ProbeResolution"),
	GLumenIrradianceFieldProbeResolution,
	TEXT("Resolution of the probe's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2"),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldProbeIrradianceResolution = 6;
FAutoConsoleVariableRef CVarLumenIrradianceFieldProbeIrradianceResolution(
	TEXT("r.Lumen.IrradianceFieldGather.IrradianceProbeResolution"),
	GLumenIrradianceFieldProbeIrradianceResolution,
	TEXT("Resolution of the probe's 2d irradiance layout."),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldProbeOcclusionResolution = 16;
FAutoConsoleVariableRef CVarLumenIrradianceFieldProbeOcclusionResolution(
	TEXT("r.Lumen.IrradianceFieldGather.OcclusionProbeResolution"),
	GLumenIrradianceFieldProbeOcclusionResolution,
	TEXT("Resolution of the probe's 2d occlusion layout."),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldNumMipmaps = 1;
FAutoConsoleVariableRef CVarLumenIrradianceFieldNumMipmaps(
	TEXT("r.Lumen.IrradianceFieldGather.NumMipmaps"),
	GLumenIrradianceFieldNumMipmaps,
	TEXT("Number of radiance cache mipmaps."),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldProbeAtlasResolutionInProbes = 128;
FAutoConsoleVariableRef CVarLumenIrradianceFieldProbeAtlasResolutionInProbes(
	TEXT("r.Lumen.IrradianceFieldGather.ProbeAtlasResolutionInProbes"),
	GLumenIrradianceFieldProbeAtlasResolutionInProbes,
	TEXT("Number of probes along one dimension of the probe atlas cache texture.  This controls the memory usage of the cache.  Overflow currently results in incorrect rendering."),
	ECVF_RenderThreadSafe
);

float GLumenIrradianceFieldProbeOcclusionViewBias = 20;
FAutoConsoleVariableRef CVarLumenIrradianceFieldProbeOcclusionViewBias(
	TEXT("r.Lumen.IrradianceFieldGather.ProbeOcclusionViewBias"),
	GLumenIrradianceFieldProbeOcclusionViewBias,
	TEXT("Bias along the view direction to reduce self-occlusion artifacts from Probe Occlusion"),
	ECVF_RenderThreadSafe
);

float GLumenIrradianceFieldProbeOcclusionNormalBias = 20;
FAutoConsoleVariableRef CVarLumenIrradianceFieldProbeOcclusionNormalBias(
	TEXT("r.Lumen.IrradianceFieldGather.ProbeOcclusionNormalBias"),
	GLumenIrradianceFieldProbeOcclusionNormalBias,
	TEXT("Bias along the normal to reduce self-occlusion artifacts from Probe Occlusion"),
	ECVF_RenderThreadSafe
);

int32 GLumenIrradianceFieldStats = 0;
FAutoConsoleVariableRef CVarLumenIrradianceFieldStats(
	TEXT("r.Lumen.IrradianceFieldGather.RadianceCache.Stats"),
	GLumenIrradianceFieldStats,
	TEXT("GPU print out Radiance Cache update stats."),
	ECVF_RenderThreadSafe
);

namespace LumenIrradianceFieldGather
{
	LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs()
	{
		LumenRadianceCache::FRadianceCacheInputs Parameters = LumenRadianceCache::GetDefaultRadianceCacheInputs();
		Parameters.ReprojectionRadiusScale = 1.5f;
		Parameters.ClipmapWorldExtent = GLumenIrradianceFieldClipmapWorldExtent;
		Parameters.ClipmapDistributionBase = GLumenIrradianceFieldClipmapDistributionBase;
		Parameters.RadianceProbeClipmapResolution = FMath::Clamp(GLumenIrradianceFieldGridResolution, 1, 256);
		Parameters.ProbeAtlasResolutionInProbes = FIntPoint(GLumenIrradianceFieldProbeAtlasResolutionInProbes, GLumenIrradianceFieldProbeAtlasResolutionInProbes);
		Parameters.NumRadianceProbeClipmaps = FMath::Clamp(GLumenIrradianceFieldNumClipmaps, 1, LumenRadianceCache::MaxClipmaps);
		Parameters.RadianceProbeResolution = FMath::Max(GLumenIrradianceFieldProbeResolution, LumenRadianceCache::MinRadianceProbeResolution);
		Parameters.FinalProbeResolution = GLumenIrradianceFieldProbeResolution + 2 * (1 << (GLumenIrradianceFieldNumMipmaps - 1));
		Parameters.FinalRadianceAtlasMaxMip = GLumenIrradianceFieldNumMipmaps - 1;
		Parameters.CalculateIrradiance = 1;
		Parameters.IrradianceProbeResolution = GLumenIrradianceFieldProbeIrradianceResolution;
		Parameters.OcclusionProbeResolution = GLumenIrradianceFieldProbeOcclusionResolution;
		Parameters.NumProbesToTraceBudget = GLumenIrradianceFieldNumProbesToTraceBudget;
		Parameters.RadianceCacheStats = GLumenIrradianceFieldStats;
		return Parameters;
	}
}

class FMarkRadianceProbesUsedByGBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByGBufferCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByGBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
		END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByGBufferCS, "/Engine/Private/Lumen/LumenIrradianceFieldGather.usf", "MarkRadianceProbesUsedByGBufferCS", SF_Compute);


class FIrradianceFieldGatherCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FIrradianceFieldGatherCS)
	SHADER_USE_PARAMETER_STRUCT(FIrradianceFieldGatherCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWRoughSpecularIndirect)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenReflections::FCompositeParameters, ReflectionsCompositeParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER(float, ProbeOcclusionViewBias)
		SHADER_PARAMETER(float, ProbeOcclusionNormalBias)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		return 8;
	}

	using FPermutationDomain = TShaderPermutationDomain<>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FIrradianceFieldGatherCS, "/Engine/Private/Lumen/LumenIrradianceFieldGather.usf", "IrradianceFieldGatherCS", SF_Compute);

static void IrradianceFieldMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	FMarkRadianceProbesUsedByGBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByGBufferCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
	PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;

	auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByGBufferCS>(0);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MarkRadianceProbesUsedByGBuffer %ux%u", View.ViewRect.Width(), View.ViewRect.Height()),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FMarkRadianceProbesUsedByGBufferCS::GetGroupSize()));
}

DECLARE_GPU_STAT(LumenIrradianceFieldGather);

FSSDSignalTextures FDeferredShadingSceneRenderer::RenderLumenIrradianceFieldGather(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FViewInfo& View,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& TranslucencyVolumeRadianceCacheParameters,
	ERDGPassFlags ComputePassFlags)
{
	RDG_EVENT_SCOPE(GraphBuilder, "LumenIrradianceFieldGather");
	RDG_GPU_STAT_SCOPE(GraphBuilder, LumenIrradianceFieldGather);

	check(GLumenIrradianceFieldGather != 0);

	const LumenRadianceCache::FRadianceCacheInputs RadianceCacheInputs = LumenIrradianceFieldGather::SetupRadianceCacheInputs();

	FMarkUsedRadianceCacheProbes Callbacks;
	Callbacks.AddLambda([&SceneTextures, ComputePassFlags](
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
		{
			IrradianceFieldMarkUsedProbes(
				GraphBuilder,
				View,
				SceneTextures,
				RadianceCacheMarkParameters,
				ComputePassFlags);
		});

	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters;

	LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateInputs> InputArray;
	LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateOutputs> OutputArray;

	InputArray.Add(LumenRadianceCache::FUpdateInputs(
		RadianceCacheInputs,
		FRadianceCacheConfiguration(),
		View,
		nullptr,
		nullptr,
		FMarkUsedRadianceCacheProbes(),
		MoveTemp(Callbacks)));

	OutputArray.Add(LumenRadianceCache::FUpdateOutputs(
		View.ViewState->Lumen.RadianceCacheState,
		RadianceCacheParameters));

	LumenRadianceCache::FUpdateInputs TranslucencyVolumeRadianceCacheUpdateInputs = GetLumenTranslucencyGIVolumeRadianceCacheInputs(
		GraphBuilder,
		View, 
		FrameTemporaries,
		ComputePassFlags);

	if (TranslucencyVolumeRadianceCacheUpdateInputs.IsAnyCallbackBound())
	{
		InputArray.Add(TranslucencyVolumeRadianceCacheUpdateInputs);
		OutputArray.Add(LumenRadianceCache::FUpdateOutputs(
			View.ViewState->Lumen.TranslucencyVolumeRadianceCacheState,
			TranslucencyVolumeRadianceCacheParameters));
	}

	LumenRadianceCache::UpdateRadianceCaches(
		GraphBuilder, 
		FrameTemporaries,
		InputArray,
		OutputArray,
		Scene,
		ViewFamily,
		LumenCardRenderer.bPropagateGlobalLightingChange,
		ComputePassFlags);

	FRDGTextureDesc DiffuseIndirectDesc = FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef DiffuseIndirect = GraphBuilder.CreateTexture(DiffuseIndirectDesc, TEXT("DiffuseIndirect"));

	FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef RoughSpecularIndirect = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("RoughSpecularIndirect"));

	{
		FIrradianceFieldGatherCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FIrradianceFieldGatherCS::FParameters>();
		PassParameters->RWDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect));
		PassParameters->RWRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect));
		PassParameters->RadianceCacheParameters = RadianceCacheParameters;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		LumenReflections::SetupCompositeParameters(View, PassParameters->ReflectionsCompositeParameters);
		PassParameters->ProbeOcclusionViewBias = GLumenIrradianceFieldProbeOcclusionViewBias;
		PassParameters->ProbeOcclusionNormalBias = GLumenIrradianceFieldProbeOcclusionNormalBias;

		FIrradianceFieldGatherCS::FPermutationDomain PermutationVector;
		auto ComputeShader = View.ShaderMap->GetShader<FIrradianceFieldGatherCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("IrradianceFieldGather %ux%u", View.ViewRect.Width(), View.ViewRect.Height()),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FIrradianceFieldGatherCS::GetGroupSize()));
	}

	FSSDSignalTextures DenoiserOutputs;
	DenoiserOutputs.Textures[0] = DiffuseIndirect;
	DenoiserOutputs.Textures[1] = RoughSpecularIndirect;

	return DenoiserOutputs;
}

