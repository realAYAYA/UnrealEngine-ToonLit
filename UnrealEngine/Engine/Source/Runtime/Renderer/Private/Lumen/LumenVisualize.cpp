// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenVisualize.h"
#include "Materials/Material.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "ReflectionEnvironment.h"
#include "LumenMeshCards.h"
#include "LumenRadianceCache.h"
#include "DynamicMeshBuilder.h"
#include "ShaderPrintParameters.h"
#include "LumenScreenProbeGather.h"
#include "DistanceFieldAtlas.h"
#include "LumenSurfaceCacheFeedback.h"
#include "LumenVisualizationData.h"
#include "MeshCardBuild.h"
#include "LumenReflections.h"
#include "InstanceDataSceneProxy.h"

// Must be in sync with VISUALIZE_MODE_* in LumenVisualize.h
int32 GLumenVisualize = 0;
FAutoConsoleVariableRef CVarLumenVisualize(
	TEXT("r.Lumen.Visualize"),
	GLumenVisualize,
	TEXT("Lumen scene visualization mode.\n")
	TEXT("0 - Disable\n")
	TEXT("1 - Overview\n")
	TEXT("2 - Performance Overview\n")
	TEXT("3 - Lumen Scene\n")
	TEXT("4 - Reflection View\n")
	TEXT("5 - Surface Cache Coverage\n")
	TEXT("6 - Geometry normals\n")
	TEXT("7 - Dedicated Reflection Rays\n")
	TEXT("8 - Albedo\n")
	TEXT("9 - Normals\n")
	TEXT("10 - Emissive\n")
	TEXT("11 - Opacity (disable alpha masking)\n")
	TEXT("12 - Card weights\n")
	TEXT("13 - Direct lighting\n")
	TEXT("14 - Indirect lighting\n")
	TEXT("15 - Local Position (hardware ray-tracing only)\n")
	TEXT("16 - Velocity (hardware ray-tracing only)\n")
	TEXT("17 - Direct lighting updates\n")
	TEXT("18 - Indirect lighting updates\n")
	TEXT("19 - Last used pages\n")
	TEXT("20 - Last used high res pages"),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenSceneGridPixelSize = 32;
FAutoConsoleVariableRef CVarVisualizeLumenSceneGridPixelSize(
	TEXT("r.Lumen.Visualize.GridPixelSize"),
	GVisualizeLumenSceneGridPixelSize,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GLumenVisualizeIndirectDiffuse = 0;
FAutoConsoleVariableRef CVarLumenVisualizeIndirectDiffuse(
	TEXT("r.Lumen.Visualize.IndirectDiffuse"),
	GLumenVisualizeIndirectDiffuse,
	TEXT("Visualize Lumen Indirect Diffuse."),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenSceneTraceMeshSDFs = 1;
FAutoConsoleVariableRef CVarVisualizeLumenSceneTraceMeshSDFs(
	TEXT("r.Lumen.Visualize.TraceMeshSDFs"),
	GVisualizeLumenSceneTraceMeshSDFs,
	TEXT("Whether to use Mesh SDF tracing for lumen scene visualization."),
	ECVF_RenderThreadSafe
);

float GVisualizeLumenSceneMaxMeshSDFTraceDistance = -1.0f;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardMaxTraceDistance(
	TEXT("r.Lumen.Visualize.MaxMeshSDFTraceDistance"),
	GVisualizeLumenSceneMaxMeshSDFTraceDistance,
	TEXT("Max trace distance for Lumen scene visualization rays. Values below 0 will automatically derrive this from cone angle."),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenSceneHiResSurface = 1;
FAutoConsoleVariableRef CVarVisualizeLumenSceneHiResSurface(
	TEXT("r.Lumen.Visualize.HiResSurface"),
	GVisualizeLumenSceneHiResSurface,
	TEXT("Whether visualization should sample highest available surface data or use lowest res always resident pages."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVisualizeLumenSceneSurfaceCacheFeedback(
	TEXT("r.Lumen.Visualize.SurfaceCacheFeedback"),
	1,
	TEXT("Whether visualization should write surface cache feedback requests into the feedback buffer."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GVisualizeLumenSceneTraceRadianceCache = 0;
FAutoConsoleVariableRef CVarVisualizeLumenSceneTraceRadianceCache(
	TEXT("r.Lumen.Visualize.TraceRadianceCache"),
	GVisualizeLumenSceneTraceRadianceCache,
	TEXT("Whether to use radiance cache for Lumen scene visualization."),
	ECVF_RenderThreadSafe
);

float GVisualizeLumenSceneConeAngle = 0.0f;
FAutoConsoleVariableRef CVarVisualizeLumenSceneConeAngle(
	TEXT("r.Lumen.Visualize.ConeAngle"),
	GVisualizeLumenSceneConeAngle,
	TEXT("Visualize cone angle, in degrees."),
	ECVF_RenderThreadSafe
	);

float GVisualizeLumenSceneConeStepFactor = 2.0f;
FAutoConsoleVariableRef CVarVisualizeLumenSceneConeStepFactor(
	TEXT("r.Lumen.Visualize.ConeStepFactor"),
	GVisualizeLumenSceneConeStepFactor,
	TEXT("Cone step scale on sphere radius step size."),
	ECVF_RenderThreadSafe
	);

float GVisualizeLumenSceneMinTraceDistance = 0;
FAutoConsoleVariableRef CVarVisualizeLumenSceneMinTraceDistance(
	TEXT("r.Lumen.Visualize.MinTraceDistance"),
	GVisualizeLumenSceneMinTraceDistance,
	TEXT(""),
	ECVF_RenderThreadSafe
);

float GVisualizeLumenSceneMaxTraceDistance = 100000;
FAutoConsoleVariableRef CVarVisualizeLumenSceneMaxTraceDistance(
	TEXT("r.Lumen.Visualize.MaxTraceDistance"),
	GVisualizeLumenSceneMaxTraceDistance,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardGenerationSurfels = 0;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardGenerationSurfels(
	TEXT("r.Lumen.Visualize.CardGenerationSurfels"),
	GVisualizeLumenCardGenerationSurfels,
	TEXT(""),
	ECVF_RenderThreadSafe
);

float GVisualizeLumenCardGenerationSurfelScale = 1.0f;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardGenerationSurfelScale(
	TEXT("r.Lumen.Visualize.CardGenerationSurfelScale"),
	GVisualizeLumenCardGenerationSurfelScale,
	TEXT(""),
	ECVF_RenderThreadSafe
);

float GVisualizeLumenCardGenerationClusterScale = 1.0f;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardGenerationClusterScale(
	TEXT("r.Lumen.Visualize.CardGenerationClusterScale"),
	GVisualizeLumenCardGenerationClusterScale,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardGenerationCluster = 0;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardGenerationCluster(
	TEXT("r.Lumen.Visualize.CardGenerationCluster"),
	GVisualizeLumenCardGenerationCluster,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardGenerationMaxSurfel = -1;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardGenerationMaxSurfel(
	TEXT("r.Lumen.Visualize.CardGenerationMaxSurfel"),
	GVisualizeLumenCardGenerationMaxSurfel,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardPlacement = 0;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardPlacement(
	TEXT("r.Lumen.Visualize.CardPlacement"),
	GVisualizeLumenCardPlacement,
	TEXT(""),
	ECVF_RenderThreadSafe
);

float GVisualizeLumenCardPlacementDistance = 5000.0f;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardPlacementDistance(
	TEXT("r.Lumen.Visualize.CardPlacementDistance"),
	GVisualizeLumenCardPlacementDistance,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardPlacementLOD = 0;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardPlacementLOD(
	TEXT("r.Lumen.Visualize.CardPlacementLOD"),
	GVisualizeLumenCardPlacementLOD,
	TEXT("0 - all\n")
	TEXT("1 - only primitives\n")
	TEXT("2 - only merged instances\n")
	TEXT("3 - only merged components\n")
	TEXT("4 - only far field\n"),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardPlacementPrimitives = 0;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardPlacementPrimitives(
	TEXT("r.Lumen.Visualize.CardPlacementPrimitives"),
	GVisualizeLumenCardPlacementPrimitives,
	TEXT("Whether to visualize primitive bounding boxes.\n"),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenRayTracingGroups = 0;
FAutoConsoleVariableRef CVarVisualizeLumenRayTracingGroups(
	TEXT("r.Lumen.Visualize.RayTracingGroups"),
	GVisualizeLumenRayTracingGroups,
	TEXT("Visualize bounds for ray tracing groups. Control visualization distance using r.Lumen.Visualize.CardPlacementDistance.\n")
	TEXT("0 - disable\n")
	TEXT("1 - all groups\n")
	TEXT("2 - groups with a single instance"),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardPlacementIndex = -1;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardPlacementIndex(
	TEXT("r.Lumen.Visualize.CardPlacementIndex"),
	GVisualizeLumenCardPlacementIndex,
	TEXT("Visualize only a single card per mesh."),
	ECVF_RenderThreadSafe
);

int32 GVisualizeLumenCardPlacementDirection = -1;
FAutoConsoleVariableRef CVarVisualizeLumenSceneCardPlacementDirection(
	TEXT("r.Lumen.Visualize.CardPlacementDirection"),
	GVisualizeLumenCardPlacementDirection,
	TEXT("Visualize only a single card direction."),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneDumpStats = 0;
FAutoConsoleVariableRef CVarLumenSceneDumpStats(
	TEXT("r.LumenScene.DumpStats"),
	GLumenSceneDumpStats,
	TEXT("Whether to log Lumen scene stats on the next frame. 2 - dump mesh DF. 3 - dump LumenScene objects."),
	ECVF_RenderThreadSafe
);

float GVisualizeLumenSceneCardInterpolateInfluenceRadius = 10.0f;
FAutoConsoleVariableRef CVarCardInterpolateInfluenceRadius(
	TEXT("r.Lumen.Visualize.CardInterpolateInfluenceRadius"),
	GVisualizeLumenSceneCardInterpolateInfluenceRadius,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int> CVarVisualizeUseShaderPrintForTraces(
	TEXT("r.Lumen.Visualize.UseShaderPrintForTraces"),
	1,
	TEXT("Whether to use ShaderPrint or custom line renderer for trace visualization."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool Lumen::ShouldVisualizeScene(const FEngineShowFlags& ShowFlags)
{
	return ShowFlags.VisualizeLumen || GLumenVisualize > 0;
}

bool LumenVisualize::UseSurfaceCacheFeedback(const FEngineShowFlags& ShowFlags)
{
	return CVarVisualizeLumenSceneSurfaceCacheFeedback.GetValueOnRenderThread() != 0
		&& Lumen::ShouldVisualizeScene(ShowFlags);
}

BEGIN_SHADER_PARAMETER_STRUCT(FLumenVisualizeSceneSoftwareRayTracingParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(LumenVisualize::FSceneParameters, CommonParameters)
	SHADER_PARAMETER(float, VisualizeStepFactor)
	SHADER_PARAMETER(float, MinTraceDistance)
	SHADER_PARAMETER(float, MaxTraceDistance)
	SHADER_PARAMETER(float, MaxMeshSDFTraceDistanceForVoxelTracing)
	SHADER_PARAMETER(float, MaxMeshSDFTraceDistance)
	SHADER_PARAMETER(float, CardInterpolateInfluenceRadius)
	SHADER_PARAMETER(int, HeightfieldMaxTracingSteps)
END_SHADER_PARAMETER_STRUCT()

class FVisualizeLumenSceneCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeLumenSceneCS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizeLumenSceneCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenMeshSDFGridParameters, MeshSDFGridParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenVisualizeSceneSoftwareRayTracingParameters, VisualizeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWSceneColor)
	END_SHADER_PARAMETER_STRUCT()

	class FTraceMeshSDF : SHADER_PERMUTATION_BOOL("TRACE_MESH_SDF");
	class FTraceGlobalSDF : SHADER_PERMUTATION_BOOL("TRACE_GLOBAL_SDF");
	class FSimpleCoverageBasedExpand : SHADER_PERMUTATION_BOOL("GLOBALSDF_SIMPLE_COVERAGE_BASED_EXPAND");
	class FRadianceCache : SHADER_PERMUTATION_BOOL("RADIANCE_CACHE");
	class FTraceHeightfields : SHADER_PERMUTATION_BOOL("SCENE_TRACE_HEIGHTFIELDS");

	using FPermutationDomain = TShaderPermutationDomain<FTraceMeshSDF, FTraceGlobalSDF, FSimpleCoverageBasedExpand, FRadianceCache, FTraceHeightfields>;

public:
	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (!PermutationVector.Get<FTraceGlobalSDF>())
		{
			PermutationVector.Set<FSimpleCoverageBasedExpand>(false);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("SURFACE_CACHE_FEEDBACK"), 1);
		OutEnvironment.SetDefine(TEXT("SURFACE_CACHE_HIGH_RES_PAGES"), 1);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);

		// Workaround for an internal PC FXC compiler crash when compiling with disabled optimizations
		if (Parameters.Platform == SP_PCD3D_SM5)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
		}
	}

	static int32 GetGroupSize()
	{
		return 8;
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeLumenSceneCS, "/Engine/Private/Lumen/LumenVisualize.usf", "VisualizeQuadsCS", SF_Compute);

class FVisualizeTracesVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeTracesVS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizeTracesVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float3>, VisualizeTracesData)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeTracesVS, "/Engine/Private/Lumen/LumenVisualize.usf", "VisualizeTracesVS", SF_Vertex);


class FVisualizeTracesPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeTracesPS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizeTracesPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeTracesPS, "/Engine/Private/Lumen/LumenVisualize.usf", "VisualizeTracesPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FVisualizeTraces, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FVisualizeTracesVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVisualizeTracesPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FVisualizeTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizeTracesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenVisualize::FTonemappingParameters, TonemappingParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float3>, VisualizeTracesData)
		SHADER_PARAMETER(uint32, NumTracesToVisualize)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeTracesCS, "/Engine/Private/Lumen/LumenVisualize.usf", "VisualizeTracesCS", SF_Compute);

class FVisualizeTracesVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FVisualizeTracesVertexDeclaration() {}

	virtual void InitRHI(FRHICommandListBase& RHICmdList)
	{
		FVertexDeclarationElementList Elements;
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVisualizeTracesVertexDeclaration> GVisualizeTracesVertexDeclaration;

namespace LumenVisualize
{
	FTonemappingParameters GetTonemappingParameters(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef ColorGradingTexture,
		FRDGBufferRef EyeAdaptationBuffer)
	{
		FTonemappingParameters TonemappingParameters;
		TonemappingParameters.Tonemap = (EyeAdaptationBuffer != nullptr && ColorGradingTexture != nullptr) ? 1 : 0;
		TonemappingParameters.ColorGradingLUT = ColorGradingTexture;
		TonemappingParameters.ColorGradingLUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		TonemappingParameters.EyeAdaptationBuffer = GraphBuilder.CreateSRV(EyeAdaptationBuffer);

		if (!TonemappingParameters.ColorGradingLUT)
		{
			TonemappingParameters.ColorGradingLUT = FRDGSystemTextures::Get(GraphBuilder).VolumetricBlack;
		}

		return TonemappingParameters;
	}
};

/**
 * Render gathered traces using ShaderPrint line rendering.
 */
void RenderVisualizeTraces(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef ColorGradingTexture,
	FRDGBufferRef EyeAdaptationBuffer)
{
	if (CVarVisualizeUseShaderPrintForTraces.GetValueOnRenderThread() == 0)
	{
		return;
	}

	extern void GetReflectionsVisualizeTracesBuffer(TRefCountPtr<FRDGPooledBuffer>&VisualizeTracesData);
	extern void GetScreenProbeVisualizeTracesBuffer(TRefCountPtr<FRDGPooledBuffer>&VisualizeTracesData);

	TRefCountPtr<FRDGPooledBuffer> PooledVisualizeTracesData;
	GetReflectionsVisualizeTracesBuffer(PooledVisualizeTracesData);
	GetScreenProbeVisualizeTracesBuffer(PooledVisualizeTracesData);

	if (PooledVisualizeTracesData.IsValid())
	{
		FRDGBufferRef VisualizeTracesData = GraphBuilder.RegisterExternalBuffer(PooledVisualizeTracesData);
		const int32 NumTraces = LumenScreenProbeGather::GetTracingOctahedronResolution(View) * LumenScreenProbeGather::GetTracingOctahedronResolution(View);

		ShaderPrint::SetEnabled(true);
		ShaderPrint::RequestSpaceForLines(FMath::Max(NumTraces, 1024));

		FVisualizeTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeTracesCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->TonemappingParameters = LumenVisualize::GetTonemappingParameters(GraphBuilder, ColorGradingTexture, EyeAdaptationBuffer);
		PassParameters->VisualizeTracesData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(VisualizeTracesData, PF_A32B32G32R32F));
		PassParameters->NumTracesToVisualize = NumTraces;
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);

		auto ComputeShader = View.ShaderMap->GetShader<FVisualizeTracesCS>();

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(NumTraces, FVisualizeTracesCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("VisualizeTraces %d", NumTraces),
			ComputeShader,
			PassParameters,
			GroupCount);
	}
}

void RenderVisualizeTraces(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FMinimalSceneTextures& SceneTextures)
{
	if (CVarVisualizeUseShaderPrintForTraces.GetValueOnRenderThread() != 0)
	{
		return;
	}

	extern void GetReflectionsVisualizeTracesBuffer(TRefCountPtr<FRDGPooledBuffer>& VisualizeTracesData);
	extern void GetScreenProbeVisualizeTracesBuffer(TRefCountPtr<FRDGPooledBuffer>& VisualizeTracesData);

	TRefCountPtr<FRDGPooledBuffer> PooledVisualizeTracesData;
	GetReflectionsVisualizeTracesBuffer(PooledVisualizeTracesData);
	GetScreenProbeVisualizeTracesBuffer(PooledVisualizeTracesData);

	if (PooledVisualizeTracesData.IsValid())
	{
		FRDGBufferRef VisualizeTracesData = GraphBuilder.RegisterExternalBuffer(PooledVisualizeTracesData);

		FVisualizeTraces* PassParameters = GraphBuilder.AllocParameters<FVisualizeTraces>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilNop);
		PassParameters->VS.View = View.ViewUniformBuffer;
		PassParameters->VS.VisualizeTracesData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(VisualizeTracesData, PF_A32B32G32R32F));

		auto VertexShader = View.ShaderMap->GetShader<FVisualizeTracesVS>();
		auto PixelShader = View.ShaderMap->GetShader<FVisualizeTracesPS>();

		const int32 NumPrimitives = LumenScreenProbeGather::GetTracingOctahedronResolution(View) * LumenScreenProbeGather::GetTracingOctahedronResolution(View);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("VisualizeTraces"),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, VertexShader, PixelShader, &View, NumPrimitives](FRHICommandListImmediate& RHICmdList)
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB>::GetRHI();

				GraphicsPSOInit.PrimitiveType = PT_LineList;

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GVisualizeTracesVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, NumPrimitives, 1);
			});
	}
}

void GetVisualizeTileOutputView(const FIntRect& ViewRect, int32 TileIndex, FIntPoint& OutputViewOffset, FIntPoint& OutputViewSize)
{
	if (TileIndex >= 0)
	{
		const FIntPoint TileSize = FMath::DivideAndRoundDown<FIntPoint>(ViewRect.Size() - LumenVisualize::OverviewTileMargin * (LumenVisualize::NumOverviewTilesPerRow + 1), LumenVisualize::NumOverviewTilesPerRow);

		OutputViewSize = TileSize;
		OutputViewOffset.X = ViewRect.Min.X + TileSize.X * TileIndex + LumenVisualize::OverviewTileMargin * (TileIndex + 1);
		OutputViewOffset.Y = ViewRect.Min.Y + LumenVisualize::OverviewTileMargin;
	}
	else
	{
		OutputViewOffset = ViewRect.Min;
		OutputViewSize = ViewRect.Size();
	}
}

void SetupVisualizeParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FIntRect& ViewRect, 
	FRDGTextureRef ColorGradingTexture,
	FRDGBufferRef EyeAdaptationBuffer,
	int32 VisualizeMode, 
	int32 VisualizeTileIndex, 
	FLumenVisualizeSceneSoftwareRayTracingParameters& VisualizeParameters)
{
	float MaxMeshSDFTraceDistance = GVisualizeLumenSceneMaxMeshSDFTraceDistance >= 0.0f ? GVisualizeLumenSceneMaxMeshSDFTraceDistance : FLT_MAX;
	if (!View.IsPerspectiveProjection())
	{
		const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Lumen.Ortho.OverrideMeshDFTraceDistances"));
		if (CVar && CVar->GetValueOnRenderThread() > 0)
		{
			MaxMeshSDFTraceDistance = View.ViewMatrices.GetOrthoDimensions().GetMax();
		}		
	}

	float MaxTraceDistance = GVisualizeLumenSceneMaxTraceDistance;
	uint32 MaxReflectionBounces = 1;
	uint32 MaxRefractionBounces = LumenReflections::UseTranslucentRayTracing(View) ? 1 : 0;

	// Reflection scene view uses reflection setup
	if (VisualizeMode == VISUALIZE_MODE_REFLECTION_VIEW)
	{
		extern FLumenGatherCvarState GLumenGatherCvars;
		MaxMeshSDFTraceDistance = GLumenGatherCvars.MeshSDFTraceDistance;
		MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
		MaxReflectionBounces = LumenReflections::GetMaxReflectionBounces(View);
		MaxRefractionBounces = LumenReflections::GetMaxRefractionBounces(View);
	}

	// FLumenVisualizeSceneParameters
	{
		LumenVisualize::FSceneParameters& CommonParameters = VisualizeParameters.CommonParameters;

		CommonParameters.TonemappingParameters = LumenVisualize::GetTonemappingParameters(GraphBuilder, ColorGradingTexture, EyeAdaptationBuffer);
		CommonParameters.VisualizeHiResSurface = GVisualizeLumenSceneHiResSurface ? 1 : 0;
		CommonParameters.VisualizeMode = VisualizeMode;
		CommonParameters.MaxReflectionBounces = MaxReflectionBounces;
		CommonParameters.MaxRefractionBounces = MaxRefractionBounces;

		LumenReflections::SetupCompositeParameters(View, CommonParameters.ReflectionsCompositeParameters);
		CommonParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
		CommonParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		CommonParameters.InputViewOffset = ViewRect.Min;
		CommonParameters.OutputViewOffset = ViewRect.Min;
		CommonParameters.InputViewSize = ViewRect.Size();
		CommonParameters.OutputViewSize = ViewRect.Size();

		GetVisualizeTileOutputView(ViewRect, VisualizeTileIndex, CommonParameters.OutputViewOffset, CommonParameters.OutputViewSize);
	}

	// FLumenVisualizeSceneSoftwareRayTracingParameters
	{
		bool bTraceMeshSDF = GVisualizeLumenSceneTraceMeshSDFs != 0 && View.Family->EngineShowFlags.LumenDetailTraces;
		if (!bTraceMeshSDF)
		{
			MaxMeshSDFTraceDistance = 0.0f;
		}

		VisualizeParameters.VisualizeStepFactor = FMath::Clamp(GVisualizeLumenSceneConeStepFactor, .1f, 10.0f);
		VisualizeParameters.MinTraceDistance = FMath::Clamp(GVisualizeLumenSceneMinTraceDistance, .01f, 1000.0f);
		VisualizeParameters.MaxTraceDistance = FMath::Clamp(MaxTraceDistance, .01f, Lumen::MaxTraceDistance);
		VisualizeParameters.CardInterpolateInfluenceRadius = GVisualizeLumenSceneCardInterpolateInfluenceRadius;
		VisualizeParameters.MaxMeshSDFTraceDistanceForVoxelTracing = FMath::Clamp(MaxMeshSDFTraceDistance, VisualizeParameters.MinTraceDistance, VisualizeParameters.MaxTraceDistance);
		VisualizeParameters.MaxMeshSDFTraceDistance = FMath::Clamp(MaxMeshSDFTraceDistance, VisualizeParameters.MinTraceDistance, VisualizeParameters.MaxTraceDistance);
		VisualizeParameters.HeightfieldMaxTracingSteps = Lumen::GetHeightfieldMaxTracingSteps();
	}
}

void FDeferredShadingSceneRenderer::RenderLumenMiscVisualizations(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures, const FLumenSceneFrameTemporaries& FrameTemporaries)
{
	const FViewInfo& View = Views[0];
	const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
	const bool bAnyLumenActive = ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen || ViewPipelineState.ReflectionsMethod == EReflectionsMethod::Lumen;

	if (Lumen::IsLumenFeatureAllowedForView(Scene, View) && bAnyLumenActive)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "MiscLumenVisualization");

		RenderVisualizeTraces(GraphBuilder, View, SceneTextures);
	}

	RenderLumenRadianceCacheVisualization(GraphBuilder, SceneTextures);
	RenderLumenRadiosityProbeVisualization(GraphBuilder, SceneTextures, FrameTemporaries);

	if (GLumenSceneDumpStats)
	{
		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);
		const FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

		LumenSceneData.DumpStats(
			DistanceFieldSceneData,
			/*bDumpMeshDistanceFields*/ GLumenSceneDumpStats == 2,
			/*bDumpPrimitiveGroups*/ GLumenSceneDumpStats == 3);

		GLumenSceneDumpStats = 0;
	}
}

LumenRadianceCache::FRadianceCacheInputs GetFinalGatherRadianceCacheInputsForVisualize(const FViewInfo& View)
{
	if (GLumenIrradianceFieldGather)
	{
		return LumenIrradianceFieldGather::SetupRadianceCacheInputs();
	}
	else
	{
		return LumenScreenProbeGatherRadianceCache::SetupRadianceCacheInputs(View);
	}
}

void VisualizeLumenScene(
	const FScene* Scene,
	FRDGBuilder& GraphBuilder,
	const FEngineShowFlags& ShowFlags,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	FScreenPassTexture Output,
	FRDGTextureRef ColorGradingTexture,
	FRDGBufferRef EyeAdaptationBuffer,
	FSceneTextureShaderParameters SceneTextures,
	int32 VisualizeMode,
	int32 VisualizeTileIndex,
	bool bLumenGIEnabled)
{
	FRDGTextureUAVRef SceneColorUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Output.Texture));

	FLumenVisualizeSceneSoftwareRayTracingParameters VisualizeParameters;
	SetupVisualizeParameters(GraphBuilder, View, Output.ViewRect, ColorGradingTexture, EyeAdaptationBuffer, VisualizeMode, VisualizeTileIndex, VisualizeParameters);

	FLumenCardTracingParameters TracingParameters;
	GetLumenCardTracingParameters(GraphBuilder, View, *Scene->GetLumenSceneData(View), FrameTemporaries, LumenVisualize::UseSurfaceCacheFeedback(ShowFlags), TracingParameters);

	const FRadianceCacheState& RadianceCacheState = View.ViewState->Lumen.RadianceCacheState;
	const LumenRadianceCache::FRadianceCacheInputs RadianceCacheInputs = GetFinalGatherRadianceCacheInputsForVisualize(View);

	if (Lumen::ShouldVisualizeHardwareRayTracing(*View.Family))
	{
		FLumenIndirectTracingParameters IndirectTracingParameters;
		IndirectTracingParameters.CardInterpolateInfluenceRadius = VisualizeParameters.CardInterpolateInfluenceRadius;
		IndirectTracingParameters.MinTraceDistance = VisualizeParameters.MinTraceDistance;
		IndirectTracingParameters.MaxTraceDistance = VisualizeParameters.MaxTraceDistance;
		IndirectTracingParameters.MaxMeshSDFTraceDistance = VisualizeParameters.MaxMeshSDFTraceDistance;

		const bool bVisualizeModeWithHitLighting = (VisualizeMode == VISUALIZE_MODE_LUMEN_SCENE || VisualizeMode == VISUALIZE_MODE_REFLECTION_VIEW);

		LumenVisualize::VisualizeHardwareRayTracing(
			GraphBuilder,
			Scene,
			GetSceneTextureParameters(GraphBuilder, View),
			View,
			FrameTemporaries,
			TracingParameters,
			IndirectTracingParameters,
			VisualizeParameters.CommonParameters,
			Output.Texture,
			bVisualizeModeWithHitLighting,
			bLumenGIEnabled);
	}
	else
	{
		const uint32 CullGridPixelSize = FMath::Clamp(GVisualizeLumenSceneGridPixelSize, 8, 1024);
		const FIntPoint CullGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), CullGridPixelSize);
		const FIntVector CullGridSize = FIntVector(CullGridSizeXY.X, CullGridSizeXY.Y, 1);

		FLumenMeshSDFGridParameters MeshSDFGridParameters;
		MeshSDFGridParameters.CardGridPixelSizeShift = FMath::FloorLog2(CullGridPixelSize);
		MeshSDFGridParameters.CullGridSize = CullGridSize;

		{
			const float CardTraceEndDistanceFromCamera = VisualizeParameters.MaxMeshSDFTraceDistance;

			CullMeshObjectsToViewGrid(
				View,
				Scene,
				FrameTemporaries,
				0,
				CardTraceEndDistanceFromCamera,
				CullGridPixelSize,
				1,
				FVector::ZeroVector,
				GraphBuilder,
				MeshSDFGridParameters);
		}

		const bool bTraceGlobalSDF = Lumen::UseGlobalSDFTracing(*View.Family);
		const bool bTraceMeshSDF = Lumen::UseMeshSDFTracing(*View.Family)
			&& MeshSDFGridParameters.TracingParameters.DistanceFieldObjectBuffers.NumSceneObjects > 0
			&& VisualizeParameters.MaxMeshSDFTraceDistance > VisualizeParameters.MinTraceDistance;

		FVisualizeLumenSceneCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeLumenSceneCS::FParameters>();
		PassParameters->RWSceneColor = SceneColorUAV;
		PassParameters->SceneTextures = SceneTextures;
		PassParameters->MeshSDFGridParameters = MeshSDFGridParameters;
		PassParameters->VisualizeParameters = VisualizeParameters;
		LumenRadianceCache::GetInterpolationParameters(View, GraphBuilder, RadianceCacheState, RadianceCacheInputs, PassParameters->RadianceCacheParameters);
		PassParameters->TracingParameters = TracingParameters;

		FVisualizeLumenSceneCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FVisualizeLumenSceneCS::FTraceMeshSDF>(bTraceMeshSDF);
		PermutationVector.Set<FVisualizeLumenSceneCS::FTraceGlobalSDF>(bTraceGlobalSDF);
		PermutationVector.Set<FVisualizeLumenSceneCS::FSimpleCoverageBasedExpand>(bTraceGlobalSDF && Lumen::UseGlobalSDFSimpleCoverageBasedExpand());
		PermutationVector.Set<FVisualizeLumenSceneCS::FRadianceCache>(GVisualizeLumenSceneTraceRadianceCache != 0 && LumenScreenProbeGather::UseRadianceCache(View));
		PermutationVector.Set<FVisualizeLumenSceneCS::FTraceHeightfields>(Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View)));
		PermutationVector = FVisualizeLumenSceneCS::RemapPermutation(PermutationVector);

		auto ComputeShader = View.ShaderMap->GetShader<FVisualizeLumenSceneCS>(PermutationVector);
		FIntPoint GroupSize(FIntPoint::DivideAndRoundUp(VisualizeParameters.CommonParameters.OutputViewSize, FVisualizeLumenSceneCS::GetGroupSize()));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("LumenSceneVisualization"),
			ComputeShader,
			PassParameters,
			FIntVector(GroupSize.X, GroupSize.Y, 1));
	}
}

int32 GetLumenVisualizeMode(const FViewInfo& View)
{
	const FLumenVisualizationData& VisualizationData = GetLumenVisualizationData();
	const int32 VisualizeMode = GLumenVisualize > 0 ? GLumenVisualize : VisualizationData.GetModeID(View.CurrentLumenVisualizationMode);
	return VisualizeMode;
}

FScreenPassTexture AddVisualizeLumenScenePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, bool bAnyLumenActive, bool bLumenGIEnabled, const FVisualizeLumenSceneInputs& Inputs, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	check(Inputs.SceneColor.IsValid());

	FScreenPassTexture Output = Inputs.SceneColor;

	const FScene* Scene = (const FScene*)View.Family->Scene;
	const FSceneViewFamily& ViewFamily = *View.Family;

	if (Lumen::IsLumenFeatureAllowedForView(Scene, View) && bAnyLumenActive)
	{
		RenderVisualizeTraces(GraphBuilder, View, Inputs.ColorGradingTexture, Inputs.EyeAdaptationBuffer);

		const bool bVisualizeScene = Lumen::ShouldVisualizeScene(ViewFamily.EngineShowFlags);

		if (bVisualizeScene && (Lumen::ShouldVisualizeHardwareRayTracing(ViewFamily) || Lumen::IsSoftwareRayTracingSupported()))
		{
			RDG_EVENT_SCOPE(GraphBuilder, "VisualizeLumenScene");

			// Create a new output just to make sure the right flags are set
			FRDGTextureDesc VisualizeOutputDesc = Inputs.SceneColor.Texture->Desc;
			VisualizeOutputDesc.Flags |= TexCreate_UAV | TexCreate_RenderTargetable;
			Output = FScreenPassTexture(GraphBuilder.CreateTexture(VisualizeOutputDesc, TEXT("VisualizeLumenScene")), Inputs.SceneColor.ViewRect);

			const int32 VisualizeMode = GetLumenVisualizeMode(View);

			// In the overview mode we don't fully overwrite, copy the old Scene Color
			if (VisualizeMode == VISUALIZE_MODE_OVERVIEW || VisualizeMode == VISUALIZE_MODE_PERFORMANCE_OVERVIEW)
			{
				FRHICopyTextureInfo CopyInfo;

				AddCopyTexturePass(
					GraphBuilder,
					Inputs.SceneColor.Texture,
					Output.Texture,
					CopyInfo);
			}

			if (VisualizeMode == VISUALIZE_MODE_OVERVIEW)
			{
				struct FVisualizeTile
				{
					int32 Mode;
					const TCHAR* Name;
				};

				FVisualizeTile VisualizeTiles[LumenVisualize::NumOverviewTilesPerRow];
				VisualizeTiles[0].Mode = VISUALIZE_MODE_GEOMETRY_NORMALS;
				VisualizeTiles[0].Name = TEXT("Geometry Normals");
				VisualizeTiles[1].Mode = VISUALIZE_MODE_REFLECTION_VIEW;
				if (Lumen::UseHardwareRayTracing(ViewFamily))
				{
					VisualizeTiles[1].Name = LumenReflections::UseHitLighting(View, bLumenGIEnabled) ? TEXT("Reflection View, HWRT with hit lighting") : TEXT("Reflection View, HWRT");
				}
				else
				{
					VisualizeTiles[1].Name = Lumen::UseMeshSDFTracing(ViewFamily) ? TEXT("Reflection View, SWRT with detail tracing") : TEXT("Reflection View, SWRT");
				}
				VisualizeTiles[2].Mode = VISUALIZE_MODE_SURFACE_CACHE;
				VisualizeTiles[2].Name = TEXT("Lumen Scene, Pink - missing Surface Cache coverage, Yellow - culled Surface Cache");

				for (int32 TileIndex = 0; TileIndex < LumenVisualize::NumOverviewTilesPerRow; ++TileIndex)
				{
					VisualizeLumenScene(Scene, GraphBuilder, ViewFamily.EngineShowFlags, View, FrameTemporaries, Output, Inputs.ColorGradingTexture, Inputs.EyeAdaptationBuffer, Inputs.SceneTextures, VisualizeTiles[TileIndex].Mode, TileIndex, bLumenGIEnabled);
				}

				AddDrawCanvasPass(GraphBuilder, RDG_EVENT_NAME("LumenVisualizeLabels"), View, FScreenPassRenderTarget(Output, ERenderTargetLoadAction::ELoad),
					[&ViewRect = Inputs.SceneColor.ViewRect, &VisualizeTiles](FCanvas& Canvas)
				{
					const float DPIScale = Canvas.GetDPIScale();
					Canvas.SetBaseTransform(FMatrix(FScaleMatrix(DPIScale)* Canvas.CalcBaseTransform2D(Canvas.GetViewRect().Width(), Canvas.GetViewRect().Height())));

					const FLinearColor LabelColor(1, 1, 0);

					for (int32 TileIndex = 0; TileIndex < LumenVisualize::NumOverviewTilesPerRow; ++TileIndex)
					{
						FIntPoint OutputViewSize;
						FIntPoint OutputViewOffset;
						GetVisualizeTileOutputView(ViewRect, TileIndex, OutputViewOffset, OutputViewSize);

						FIntPoint LabelLocation(OutputViewOffset.X + 2 * LumenVisualize::OverviewTileMargin, OutputViewOffset.Y + OutputViewSize.Y - 20);
						Canvas.DrawShadowedString(LabelLocation.X / DPIScale, LabelLocation.Y / DPIScale, VisualizeTiles[TileIndex].Name, GetStatsFont(), LabelColor);
					}
				});
			}
			else if (VisualizeMode == VISUALIZE_MODE_PERFORMANCE_OVERVIEW)
			{
				struct FVisualizeTile
				{
					int32 Mode;
					FString Name;
				};

				LumenReflections::FCompositeParameters ReflectionCompositeParameters;
				LumenReflections::SetupCompositeParameters(View, ReflectionCompositeParameters);

				FVisualizeTile VisualizeTiles[1];
				VisualizeTiles[0].Mode = VISUALIZE_MODE_DEDICATED_REFLECTION_RAYS;
				VisualizeTiles[0].Name = FString::Printf(
					TEXT("Pixels tracing dedicated reflection rays.")
					TEXT("\nGreen - foliage(Subsurface or Two Sided Foliage shading model). Red - other.")
					TEXT("\nMaxRoughness: %.2f MaxFoliageRoughness: %.2f"),
					ReflectionCompositeParameters.MaxRoughnessToTrace,
					ReflectionCompositeParameters.MaxRoughnessToTraceForFoliage);

				for (int32 TileIndex = 0; TileIndex < UE_ARRAY_COUNT(VisualizeTiles); ++TileIndex)
				{
					VisualizeLumenScene(Scene, GraphBuilder, ViewFamily.EngineShowFlags, View, FrameTemporaries, Output, Inputs.ColorGradingTexture, Inputs.EyeAdaptationBuffer, Inputs.SceneTextures, VisualizeTiles[TileIndex].Mode, TileIndex, bLumenGIEnabled);
				}

				AddDrawCanvasPass(GraphBuilder, RDG_EVENT_NAME("LumenVisualizeLabels"), View, FScreenPassRenderTarget(Output, ERenderTargetLoadAction::ELoad),
					[&ViewRect = Inputs.SceneColor.ViewRect, &VisualizeTiles](FCanvas& Canvas)
					{
						const float DPIScale = Canvas.GetDPIScale();
						Canvas.SetBaseTransform(FMatrix(FScaleMatrix(DPIScale) * Canvas.CalcBaseTransform2D(Canvas.GetViewRect().Width(), Canvas.GetViewRect().Height())));

						const FLinearColor LabelColor(1, 1, 0);

						for (int32 TileIndex = 0; TileIndex < UE_ARRAY_COUNT(VisualizeTiles); ++TileIndex)
						{
							FIntPoint OutputViewSize;
							FIntPoint OutputViewOffset;
							GetVisualizeTileOutputView(ViewRect, TileIndex, OutputViewOffset, OutputViewSize);

							FIntPoint LabelLocation(OutputViewOffset.X + 2 * LumenVisualize::OverviewTileMargin, OutputViewOffset.Y + OutputViewSize.Y - 46);
							Canvas.DrawShadowedString(LabelLocation.X / DPIScale, LabelLocation.Y / DPIScale, *VisualizeTiles[TileIndex].Name, GetStatsFont(), LabelColor);
						}
					});
			}
			else
			{
				VisualizeLumenScene(Scene, GraphBuilder, ViewFamily.EngineShowFlags, View, FrameTemporaries, Output, Inputs.ColorGradingTexture, Inputs.EyeAdaptationBuffer, Inputs.SceneTextures, VisualizeMode, /*VisualizeTileIndex*/ -1, bLumenGIEnabled);
			}
		}
	}

	if (Inputs.OverrideOutput.IsValid())
	{
		AddDrawTexturePass(GraphBuilder, View, Output, Inputs.OverrideOutput);
		return Inputs.OverrideOutput;
	}

	return MoveTemp(Output);
}

void AddBoxFaceTriangles(FDynamicMeshBuilder& MeshBuilder, int32 FaceIndex)
{
	const int32 BoxIndices[6][4] =
	{
		{ 0, 2, 3, 1 },	// back, -z
		{ 4, 5, 7, 6 },	// front, +z
		{ 0, 4, 6, 2 },	// left, -x
		{ 1, 3, 7, 5 },	// right, +x,
		{ 0, 4, 5, 1 },	// bottom, -y
		{ 2, 3, 7, 6 }	// top, +y
	};

	MeshBuilder.AddTriangle(BoxIndices[FaceIndex][0], BoxIndices[FaceIndex][2], BoxIndices[FaceIndex][1]);
	MeshBuilder.AddTriangle(BoxIndices[FaceIndex][0], BoxIndices[FaceIndex][3], BoxIndices[FaceIndex][2]);
}

void DrawPrimitiveBounds(const FLumenPrimitiveGroup& PrimitiveGroup, FLinearColor BoundsColor, FViewElementPDI& ViewPDI)
{
	const uint8 DepthPriority = SDPG_World;

	for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : PrimitiveGroup.Primitives)
	{
		const FMatrix& PrimitiveToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
		if (const FInstanceSceneDataBuffers *InstanceData = PrimitiveSceneInfo->GetInstanceSceneDataBuffers())
		{
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceData->GetNumInstances(); ++InstanceIndex)
			{
				const FBox LocalBoundingBox = InstanceData->GetInstanceLocalBounds(InstanceIndex).ToBox();
				FMatrix LocalToWorld = InstanceData->GetInstanceToWorld(InstanceIndex);
				DrawWireBox(&ViewPDI, LocalToWorld, LocalBoundingBox, BoundsColor, DepthPriority);
			}
		}
		else
		{
			const FBox LocalBoundingBox = PrimitiveSceneInfo->Proxy->GetLocalBounds().GetBox();
			DrawWireBox(&ViewPDI, PrimitiveToWorld, LocalBoundingBox, BoundsColor, DepthPriority);
		}
	}
}

void DrawSurfels(FSceneRenderingBulkObjectAllocator& Allocator, const TArray<FLumenCardBuildDebugData::FSurfel>& Surfels, const FMatrix& PrimitiveToWorld, FLumenCardBuildDebugData::ESurfelType SurfelType, FLinearColor SurfelColor, FViewElementPDI& ViewPDI, float SurfelRadius = 2.0f)
{
	FColoredMaterialRenderProxy* MaterialRenderProxy = Allocator.Create<FColoredMaterialRenderProxy>(GEngine->LevelColorationUnlitMaterial->GetRenderProxy(), SurfelColor);

	FDynamicMeshBuilder MeshBuilder(ViewPDI.View->GetFeatureLevel());

	int32 NumSurfels = 0;
	FVector3f NormalSum(0.0f, 0.0f, 0.0f);
	FBox LocalBounds;
	LocalBounds.Init();

	const FMatrix WorldToPrimitiveT = PrimitiveToWorld.Inverse().GetTransposed();

	int32 BaseVertex = 0;
	for (int32 SurfelIndex = 0; SurfelIndex < Surfels.Num(); ++SurfelIndex)
	{
		if (GVisualizeLumenCardGenerationMaxSurfel >= 0 && NumSurfels >= GVisualizeLumenCardGenerationMaxSurfel)
		{
			break;
		}

		const FLumenCardBuildDebugData::FSurfel& Surfel = Surfels[SurfelIndex];
		if (Surfel.Type == SurfelType)
		{
			FVector3f DiskPosition = (FVector4f)PrimitiveToWorld.TransformPosition((FVector)Surfel.Position);
			FVector3f DiskNormal = (FVector4f)WorldToPrimitiveT.TransformVector((FVector)Surfel.Normal).GetSafeNormal();

			// Surface bias
			DiskPosition += DiskNormal * 0.5f;

			FVector3f AxisX;
			FVector3f AxisY;
			DiskNormal.FindBestAxisVectors(AxisX, AxisY);

			const int32 NumSides = 6;
			const float	AngleDelta = 2.0f * PI / NumSides;
			for (int32 SideIndex = 0; SideIndex < NumSides; ++SideIndex)
			{
				const FVector3f VertexPosition = DiskPosition + (AxisX * FMath::Cos(AngleDelta * (SideIndex)) + AxisY * FMath::Sin(AngleDelta * (SideIndex))) * SurfelRadius * GVisualizeLumenCardGenerationSurfelScale;

				MeshBuilder.AddVertex(VertexPosition, FVector2f(0, 0), FVector3f(1, 0, 0), FVector3f(0, 1, 0), FVector3f(0, 0, 1), FColor::White);
			}

			for (int32 SideIndex = 0; SideIndex < NumSides - 1; ++SideIndex)
			{
				int32 V0 = BaseVertex + 0;
				int32 V1 = BaseVertex + SideIndex;
				int32 V2 = BaseVertex + (SideIndex + 1);

				MeshBuilder.AddTriangle(V0, V1, V2);
			}
			BaseVertex += NumSides;
			NormalSum += DiskNormal;
			++NumSurfels;

			LocalBounds += (FVector)Surfel.Position;
		}
	}

	const uint8 DepthPriority = SDPG_World;
	MeshBuilder.Draw(&ViewPDI, FMatrix::Identity, MaterialRenderProxy, DepthPriority, false);

	if (SurfelType == FLumenCardBuildDebugData::ESurfelType::Cluster 
		&& GVisualizeLumenCardGenerationMaxSurfel >= 0)
	{
		LocalBounds = LocalBounds.ExpandBy(1.0f);

		DrawWireBox(&ViewPDI, PrimitiveToWorld, LocalBounds, SurfelColor, DepthPriority, GVisualizeLumenCardGenerationClusterScale);

		const FVector Start = PrimitiveToWorld.TransformPosition(LocalBounds.GetCenter());
		const FVector End = PrimitiveToWorld.TransformPosition(LocalBounds.GetCenter() + (FVector)NormalSum.GetSafeNormal() * 100.0f);
		ViewPDI.DrawLine(Start, End, FLinearColor::Red, 0, 0.2f, 0.0f, false);
	}
}

void VisualizeRayTracingGroups(const FViewInfo& View, const FLumenSceneData& LumenSceneData, FViewElementPDI& ViewPDI)
{
	if (GVisualizeLumenRayTracingGroups == 0)
	{
		return;
	}

	FConvexVolume ViewFrustum;
	GetViewFrustumBounds(ViewFrustum, View.ViewMatrices.GetViewProjectionMatrix(), true);
	
	for (const FLumenPrimitiveGroup& PrimitiveGroup : LumenSceneData.PrimitiveGroups)
	{
		if ((GVisualizeLumenRayTracingGroups != 2 || !PrimitiveGroup.HasMergedInstances())
			&& PrimitiveGroup.HasMergedPrimitives()
			&& PrimitiveGroup.WorldSpaceBoundingBox.ComputeSquaredDistanceToPoint((FVector3f)View.ViewMatrices.GetViewOrigin()) < GVisualizeLumenCardPlacementDistance * GVisualizeLumenCardPlacementDistance
			&& ViewFrustum.IntersectBox((FVector)PrimitiveGroup.WorldSpaceBoundingBox.GetCenter(), (FVector)PrimitiveGroup.WorldSpaceBoundingBox.GetExtent()))
		{
			const uint32 GroupIdHash = GetTypeHash(PrimitiveGroup.RayTracingGroupMapElementId.GetIndex());
			const uint8 DepthPriority = SDPG_World;
			const uint8 Hue = GroupIdHash & 0xFF;
			const uint8 Saturation = 0xFF;
			const uint8 Value = 0xFF;

			FLinearColor GroupColor = FLinearColor::MakeFromHSV8(Hue, Saturation, Value);
			GroupColor.A = 1.0f;

			DrawPrimitiveBounds(PrimitiveGroup, GroupColor, ViewPDI);
		}
	}
}

void VisualizeCardPlacement(FSceneRenderingBulkObjectAllocator& Allocator, const FViewInfo& View, const FLumenSceneData& LumenSceneData, FViewElementPDI& ViewPDI)
{
	if (GVisualizeLumenCardPlacement == 0)
	{
		return;
	}

	FConvexVolume ViewFrustum;
	GetViewFrustumBounds(ViewFrustum, View.ViewMatrices.GetViewProjectionMatrix(), true);

	for (const FLumenPrimitiveGroup& PrimitiveGroup : LumenSceneData.PrimitiveGroups)
	{
		bool bVisible = PrimitiveGroup.MeshCardsIndex >= 0;

		switch (GVisualizeLumenCardPlacementLOD)
		{
		case 1:
			bVisible = bVisible && !PrimitiveGroup.HasMergedInstances();
			break;

		case 2:
			bVisible = bVisible && PrimitiveGroup.HasMergedInstances() && !PrimitiveGroup.HasMergedPrimitives();
			break;

		case 3:
			bVisible = bVisible && PrimitiveGroup.HasMergedInstances() && PrimitiveGroup.HasMergedPrimitives();
			break;

		case 4:
			bVisible = bVisible && PrimitiveGroup.bFarField;
			break;
		}

		if (bVisible
			&& PrimitiveGroup.WorldSpaceBoundingBox.ComputeSquaredDistanceToPoint((FVector3f)View.ViewMatrices.GetViewOrigin()) < GVisualizeLumenCardPlacementDistance * GVisualizeLumenCardPlacementDistance
			&& ViewFrustum.IntersectBox((FVector)PrimitiveGroup.WorldSpaceBoundingBox.GetCenter(), (FVector)PrimitiveGroup.WorldSpaceBoundingBox.GetExtent()))
		{
			const FLumenMeshCards& MeshCardsEntry = LumenSceneData.MeshCards[PrimitiveGroup.MeshCardsIndex];

			for (uint32 CardIndex = MeshCardsEntry.FirstCardIndex; CardIndex < MeshCardsEntry.FirstCardIndex + MeshCardsEntry.NumCards; ++CardIndex)
			{
				const FLumenCard& Card = LumenSceneData.Cards[CardIndex];

				bVisible = Card.bVisible;

				if (GVisualizeLumenCardPlacementIndex >= 0 && Card.IndexInMeshCards != GVisualizeLumenCardPlacementIndex)
				{
					bVisible = false;
				}

				if (bVisible)
				{
					uint32 CardHash = HashCombine(GetTypeHash(Card.LocalOBB.Origin), GetTypeHash(Card.LocalOBB.Extent));
					CardHash = HashCombine(CardHash, GetTypeHash(Card.LocalOBB.AxisZ));
					CardHash = HashCombine(CardHash, GetTypeHash(CardIndex));

					const uint8 DepthPriority = SDPG_World;
					const uint8 CardHue = CardHash & 0xFF;
					const uint8 CardSaturation = 0xFF;
					const uint8 CardValue = 0xFF;

					FLinearColor CardColor = FLinearColor::MakeFromHSV8(CardHue, CardSaturation, CardValue);
					CardColor.A = 1.0f;

					const FMatrix CardToWorld(Card.WorldOBB.GetCardToLocal());
					const FBox LocalBounds(-Card.WorldOBB.Extent, Card.WorldOBB.Extent);

					DrawWireBox(&ViewPDI, CardToWorld, LocalBounds, CardColor, DepthPriority);

					// Visualize bounds of primitives which make current card
					if (GVisualizeLumenCardPlacementPrimitives != 0 && PrimitiveGroup.HasMergedInstances())
					{
						DrawPrimitiveBounds(PrimitiveGroup, CardColor, ViewPDI);
					}

					// Draw card "projection face"
					{
						CardColor.A = 0.25f;

						FColoredMaterialRenderProxy* MaterialRenderProxy = Allocator.Create<FColoredMaterialRenderProxy>(GEngine->EmissiveMeshMaterial->GetRenderProxy(), CardColor, NAME_Color);

						FDynamicMeshBuilder MeshBuilder(ViewPDI.View->GetFeatureLevel());

						for (int32 VertIndex = 0; VertIndex < 8; ++VertIndex)
						{
							FVector BoxVertex;
							BoxVertex.X = VertIndex & 0x1 ? LocalBounds.Max.X : LocalBounds.Min.X;
							BoxVertex.Y = VertIndex & 0x2 ? LocalBounds.Max.Y : LocalBounds.Min.Y;
							BoxVertex.Z = VertIndex & 0x4 ? LocalBounds.Max.Z : LocalBounds.Min.Z;
							MeshBuilder.AddVertex((FVector3f)BoxVertex, FVector2f(0, 0), FVector3f(1, 0, 0), FVector3f(0, 1, 0), FVector3f(0, 0, 1), FColor::White);
						}

						AddBoxFaceTriangles(MeshBuilder, 1);

						MeshBuilder.Draw(&ViewPDI, CardToWorld, MaterialRenderProxy, DepthPriority, false);
					}
				}
			}
		}
	}
}

void VisualizeCardGeneration(FSceneRenderingBulkObjectAllocator& Allocator, const FViewInfo& View, const FLumenSceneData& LumenSceneData, FViewElementPDI& ViewPDI)
{
	if (GVisualizeLumenCardGenerationSurfels == 0 
		&& GVisualizeLumenCardGenerationCluster == 0)
	{
		return;
	}

	// VisualizeCardGeneration runs before LumenSceneUpdate and LumenScene may contain deleted proxies
	if (LumenSceneData.PendingRemoveOperations.Num() > 0)
	{
		return;
	}

	FConvexVolume ViewFrustum;
	GetViewFrustumBounds(ViewFrustum, View.ViewMatrices.GetViewProjectionMatrix(), true);

	for (const FLumenPrimitiveGroup& PrimitiveGroup : LumenSceneData.PrimitiveGroups)
	{
		if (PrimitiveGroup.WorldSpaceBoundingBox.ComputeSquaredDistanceToPoint((FVector3f)View.ViewMatrices.GetViewOrigin()) < GVisualizeLumenCardPlacementDistance * GVisualizeLumenCardPlacementDistance
			&& ViewFrustum.IntersectBox((FVector)PrimitiveGroup.WorldSpaceBoundingBox.GetCenter(), (FVector)PrimitiveGroup.WorldSpaceBoundingBox.GetExtent()))
		{
			for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : PrimitiveGroup.Primitives)
			{
				if (PrimitiveSceneInfo && PrimitiveSceneInfo->Proxy)
				{
					const FCardRepresentationData* CardRepresentationData = PrimitiveSceneInfo->Proxy->GetMeshCardRepresentation();
					if (CardRepresentationData)
					{
						const uint8 DepthPriority = SDPG_World;
						const FMatrix PrimitiveToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
						const FLumenCardBuildDebugData& DebugData = CardRepresentationData->MeshCardsBuildData.DebugData;

						if (GVisualizeLumenCardGenerationSurfels)
						{
							DrawSurfels(Allocator, DebugData.Surfels, PrimitiveToWorld, FLumenCardBuildDebugData::ESurfelType::Valid, FLinearColor::Green, ViewPDI);
							DrawSurfels(Allocator, DebugData.Surfels, PrimitiveToWorld, FLumenCardBuildDebugData::ESurfelType::Invalid, FLinearColor::Red, ViewPDI);

							for (const FLumenCardBuildDebugData::FRay& Ray : DebugData.SurfelRays)
							{
								const FVector Start = PrimitiveToWorld.TransformPosition((FVector)Ray.RayStart);
								const FVector End = PrimitiveToWorld.TransformPosition((FVector)Ray.RayEnd);
								ViewPDI.DrawLine(Start, End, Ray.bHit ? FLinearColor::Red : FLinearColor::White, 0, 0.2f, 0.0f, false);
							}
						}

						if (GVisualizeLumenCardGenerationSurfels == 0
							&& GVisualizeLumenCardGenerationCluster != 0
							&& PrimitiveGroup.MeshCardsIndex >= 0)
						{
							const FLumenMeshCards& MeshCardsEntry = LumenSceneData.MeshCards[PrimitiveGroup.MeshCardsIndex];
							for (uint32 CardIndex = MeshCardsEntry.FirstCardIndex; CardIndex < MeshCardsEntry.FirstCardIndex + MeshCardsEntry.NumCards; ++CardIndex)
							{
								const FLumenCard& Card = LumenSceneData.Cards[CardIndex];

								if (Card.bVisible
									&& (Card.IndexInMeshCards == GVisualizeLumenCardPlacementIndex || GVisualizeLumenCardPlacementIndex < 0)
									&& (Card.AxisAlignedDirectionIndex == GVisualizeLumenCardPlacementDirection || GVisualizeLumenCardPlacementDirection < 0)
									&& Card.IndexInBuildData < DebugData.Clusters.Num())
								{
									const FLumenCardBuildDebugData::FSurfelCluster& Cluster = DebugData.Clusters[Card.IndexInBuildData];

									uint32 CardHash = HashCombine(GetTypeHash(Card.LocalOBB.Origin), GetTypeHash(Card.LocalOBB.Extent));
									CardHash = HashCombine(CardHash, GetTypeHash(Card.IndexInBuildData));

									const uint8 CardHue = CardHash & 0xFF;
									const uint8 CardSaturation = 0xFF;
									const uint8 CardValue = 0xFF;

									FLinearColor CardColor = FLinearColor::MakeFromHSV8(CardHue, CardSaturation, CardValue);
									CardColor.A = 1.0f;

									DrawSurfels(Allocator, Cluster.Surfels, PrimitiveToWorld, FLumenCardBuildDebugData::ESurfelType::Cluster, CardColor, ViewPDI);
									DrawSurfels(Allocator, Cluster.Surfels, PrimitiveToWorld, FLumenCardBuildDebugData::ESurfelType::Used, FLinearColor::Gray, ViewPDI);
									DrawSurfels(Allocator, Cluster.Surfels, PrimitiveToWorld, FLumenCardBuildDebugData::ESurfelType::Idle, FLinearColor::Blue, ViewPDI);

									for (const FLumenCardBuildDebugData::FRay& Ray : Cluster.Rays)
									{
										const FVector Start = PrimitiveToWorld.TransformPosition((FVector)Ray.RayStart);
										const FVector End = PrimitiveToWorld.TransformPosition((FVector)Ray.RayEnd);
										ViewPDI.DrawLine(Start, End, Ray.bHit ? FLinearColor::Red : FLinearColor::White, 0, 0.2f, 0.0f, false);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void FDeferredShadingSceneRenderer::LumenScenePDIVisualization()
{
	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);

	const bool bAnyLumenEnabled = ShouldRenderLumenDiffuseGI(Scene, Views[0])
		|| ShouldRenderLumenReflections(Views[0]);

	if (bAnyLumenEnabled)
	{
		if (GVisualizeLumenCardPlacement != 0
			|| GVisualizeLumenRayTracingGroups != 0
			|| GVisualizeLumenCardGenerationCluster != 0
			|| GVisualizeLumenCardGenerationSurfels != 0)
		{
			FViewElementPDI ViewPDI(&Views[0], nullptr, &Views[0].DynamicPrimitiveCollector);
			VisualizeRayTracingGroups(Views[0], LumenSceneData, ViewPDI);
			VisualizeCardPlacement(Allocator, Views[0], LumenSceneData, ViewPDI);
			VisualizeCardGeneration(Allocator, Views[0], LumenSceneData, ViewPDI);
		}
	}

	static bool bVisualizeLumenSceneViewOrigin = false;

	if (bVisualizeLumenSceneViewOrigin)
	{
		const int32 NumClipmaps = Lumen::GetNumGlobalDFClipmaps(Views[0]);

		for (int32 ClipmapIndex = 0; ClipmapIndex < NumClipmaps; ClipmapIndex++)
		{
			FViewElementPDI ViewPDI(&Views[0], nullptr, &Views[0].DynamicPrimitiveCollector);
			const uint8 MarkerHue = (ClipmapIndex * 100) & 0xFF;
			const uint8 MarkerSaturation = 0xFF;
			const uint8 MarkerValue = 0xFF;

			FLinearColor MarkerColor = FLinearColor::MakeFromHSV8(MarkerHue, MarkerSaturation, MarkerValue);
			MarkerColor.A = 0.5f;
			const FVector LumenSceneCameraOrigin = Lumen::GetLumenSceneViewOrigin(Views[0], ClipmapIndex);
			DrawWireSphere(&ViewPDI, LumenSceneCameraOrigin, MarkerColor, 10 * (1 << ClipmapIndex), 32, SDPG_World);
		}
	}
}