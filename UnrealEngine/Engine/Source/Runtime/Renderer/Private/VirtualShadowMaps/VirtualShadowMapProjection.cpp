// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VirtualShadowMapProjection.cpp
=============================================================================*/

#include "VirtualShadowMapProjection.h"
#include "VirtualShadowMapVisualizationData.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "ShadowRendering.h"
#include "DeferredShadingRenderer.h"
#include "PixelShaderUtils.h"
#include "ShadowRendering.h"
#include "SceneRendering.h"
#include "VirtualShadowMapClipmap.h"
#include "HairStrands/HairStrandsData.h"

#define MAX_TEST_PERMUTATION 0

static TAutoConsoleVariable<float> CVarScreenRayLength(
	TEXT( "r.Shadow.Virtual.ScreenRayLength" ),
	0.015f,
	TEXT( "Length of the screen space shadow trace (smart shadow bias) before the virtual shadow map lookup." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNormalBias(
	TEXT( "r.Shadow.Virtual.NormalBias" ),
	0.5f,
	TEXT( "Receiver offset along surface normal for shadow lookup. Scaled by distance to camera." )
	TEXT( "Higher values avoid artifacts on surfaces nearly parallel to the light, but also visibility offset shadows and increase the chance of hitting unmapped pages." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarVirtualShadowOnePassProjection(
	TEXT("r.Shadow.Virtual.OnePassProjection"),
	0,
	TEXT("Single pass projects all local VSMs culled with the light grid. Used in conjunction with clustered deferred shading."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSMRTRayCountLocal(
	TEXT( "r.Shadow.Virtual.SMRT.RayCountLocal" ),
	7,
	TEXT( "Ray count for shadow map tracing of local lights. 0 = disabled." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSMRTSamplesPerRayLocal(
	TEXT( "r.Shadow.Virtual.SMRT.SamplesPerRayLocal" ),
	8,
	TEXT( "Shadow map samples per ray for local lights" ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSMRTMaxRayAngleFromLight(
	TEXT( "r.Shadow.Virtual.SMRT.MaxRayAngleFromLight" ),
	0.03f,
	TEXT( "Max angle (in radians) a ray is allowed to span from the light's perspective for local lights." )
	TEXT( "Smaller angles limit the screen space size of shadow penumbra. " )
	TEXT( "Larger angles lead to more noise. " ),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSMRTRayCountDirectional(
	TEXT( "r.Shadow.Virtual.SMRT.RayCountDirectional" ),
	7,
	TEXT( "Ray count for shadow map tracing of directional lights. 0 = disabled." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSMRTSamplesPerRayDirectional(
	TEXT( "r.Shadow.Virtual.SMRT.SamplesPerRayDirectional" ),
	8,
	TEXT( "Shadow map samples per ray for directional lights" ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSMRTRayLengthScaleDirectional(
	TEXT( "r.Shadow.Virtual.SMRT.RayLengthScaleDirectional" ),
	1.5f,
	TEXT( "Length of ray to shoot for directional lights, scaled by distance to camera." )
	TEXT( "Shorter rays limit the screen space size of shadow penumbra. " )
	TEXT( "Longer rays require more samples to avoid shadows disconnecting from contact points. " ),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSMRTAdaptiveRayCount(
	TEXT( "r.Shadow.Virtual.SMRT.AdaptiveRayCount" ),
	1,
	TEXT( "Shoot fewer rays in fully shadowed and unshadowed regions. Currently only supported with OnePassProjection. " ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSMRTTexelDitherScaleLocal(
	TEXT( "r.Shadow.Virtual.SMRT.TexelDitherScaleLocal" ),
	2.0f,
	TEXT( "Applies a dither to the shadow map ray casts for local lights to help hide aliasing due to insufficient shadow resolution.\n" )
	TEXT( "Setting this too high can cause shadows light leaks near occluders." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSMRTTexelDitherScaleDirectional(
	TEXT( "r.Shadow.Virtual.SMRT.TexelDitherScaleDirectional" ),
	2.0f,
	TEXT( "Applies a dither to the shadow map ray casts for directional lights to help hide aliasing due to insufficient shadow resolution.\n" )
	TEXT( "Setting this too high can cause shadows light leaks near occluders." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarSMRTExtrapolateMaxSlopeLocal(
	TEXT("r.Shadow.Virtual.SMRT.ExtrapolateMaxSlopeLocal"),
	0.05f,
	TEXT("Maximum depth slope when extrapolating behind occluders for local lights.\n")
	TEXT("Higher values allow softer penumbra edges but can introduce light leaks behind second occluders.\n")
	TEXT("Setting to 0 will disable slope extrapolation slightly improving projection performance, at the cost of reduced penumbra quality."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarSMRTExtrapolateMaxSlopeDirectional(
	TEXT("r.Shadow.Virtual.SMRT.ExtrapolateMaxSlopeDirectional"),
	5.0f,
	TEXT("Maximum depth slope when extrapolating behind occluders for directional lights.\n")
	TEXT("Higher values allow softer penumbra edges but can introduce light leaks behind second occluders.\n")
	TEXT("Setting to 0 will disable slope extrapolation slightly improving projection performance, at the cost of reduced penumbra quality."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarSMRTMaxSlopeBiasLocal(
	TEXT("r.Shadow.Virtual.SMRT.MaxSlopeBiasLocal"),
	50.0f,
	TEXT("Maximum depth slope. Low values produce artifacts if shadow resolution is insufficient. High values can worsen light leaks near occluders and sparkly pixels in shadowed areas."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarForcePerLightShadowMaskClear(
	TEXT( "r.Shadow.Virtual.ForcePerLightShadowMaskClear" ),
	0,
	TEXT( "" ),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSubsurfaceShadowMode(
	TEXT( "r.Shadow.Virtual.SubsurfaceShadowMode" ),
	0,
	TEXT( "" ),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSubsurfaceShadowMinSourceAngle(
	TEXT("r.Shadow.Virtual.SubsurfaceShadowMinSourceAngle"),
	5,
	TEXT("Minimum source angle (in degrees) used for shadow & transmittance of sub-surface materials"),
	ECVF_RenderThreadSafe
);

#if MAX_TEST_PERMUTATION > 0
static TAutoConsoleVariable<int32> CVarTestPermutation(
	TEXT( "r.Shadow.Virtual.ProjectionTestPermutation" ),
	0,
	TEXT( "Used for A/B testing projection shader changes. " ),
	ECVF_RenderThreadSafe
);
#endif

const TCHAR* ToString(EVirtualShadowMapProjectionInputType In)
{
	switch (In)
	{
	case EVirtualShadowMapProjectionInputType::HairStrands: return TEXT("HairStrands");
	case EVirtualShadowMapProjectionInputType::GBuffer:     return Strata::IsStrataEnabled() ? TEXT("Strata") : TEXT("GBuffer");
	}
	return TEXT("Invalid");
}


class FVirtualShadowMapProjectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVirtualShadowMapProjectionCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualShadowMapProjectionCS, FGlobalShader)
	
	class FDirectionalLightDim		: SHADER_PERMUTATION_BOOL("DIRECTIONAL_LIGHT");
	class FOnePassProjectionDim		: SHADER_PERMUTATION_BOOL("ONE_PASS_PROJECTION");
	class FHairStrandsDim			: SHADER_PERMUTATION_BOOL("HAS_HAIR_STRANDS");
	class FVisualizeOutputDim		: SHADER_PERMUTATION_BOOL("VISUALIZE_OUTPUT");
	class FExtrapolateSlopeDim		: SHADER_PERMUTATION_BOOL("SMRT_EXTRAPOLATE_SLOPE");
	// -1 means dynamic count
	class FSMRTStaticSampleCount	: SHADER_PERMUTATION_RANGE_INT("SMRT_TEMPLATE_STATIC_SAMPLES_PER_RAY", -1, 2);
	// Used for A/B testing a change that affects reg allocation, etc.
	class FTestDim					: SHADER_PERMUTATION_INT("TEST_PERMUTATION", MAX_TEST_PERMUTATION+1);

	using FPermutationDomain = TShaderPermutationDomain<
		FDirectionalLightDim,
		FOnePassProjectionDim,
		FHairStrandsDim,
		FVisualizeOutputDim,
		FExtrapolateSlopeDim,
		FSMRTStaticSampleCount
#if MAX_TEST_PERMUTATION > 0
		, FTestDim
#endif
	>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, SamplingParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, HairStrandsVoxel)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FIntVector4, ProjectionRect)
		SHADER_PARAMETER(float, ScreenRayLength)
		SHADER_PARAMETER(float, NormalBias)
		SHADER_PARAMETER(int32, SubsurfaceShadowMode)
		SHADER_PARAMETER(float, SubsurfaceMinSourceRadius)
		SHADER_PARAMETER(int32, SMRTRayCount)
		SHADER_PARAMETER(int32, SMRTSamplesPerRay)
		SHADER_PARAMETER(float, SMRTRayLengthScale)
		SHADER_PARAMETER(float, SMRTCotMaxRayAngleFromLight)
		SHADER_PARAMETER(float, SMRTTexelDitherScale)
		SHADER_PARAMETER(float, SMRTExtrapolateSlope)
		SHADER_PARAMETER(float, SMRTMaxSlopeBias)
		SHADER_PARAMETER(uint32, bSMRTUseAdaptiveRayCount)
		SHADER_PARAMETER(uint32, InputType)
		SHADER_PARAMETER(uint32, bCullBackfacingPixels)
		// One pass projection parameters
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutShadowMaskBits)
		// Pass per light parameters
		SHADER_PARAMETER_STRUCT(FLightShaderParameters, Light)
		SHADER_PARAMETER(int32, LightUniformVirtualShadowMapId)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutShadowFactor)
		// Visualization output
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FPhysicalPageMetaData >, PhysicalPageMetaData)
		SHADER_PARAMETER(int32, VisualizeModeId)
		SHADER_PARAMETER(int32, VisualizeVirtualShadowMapId)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutVisualize)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		// TODO: We may no longer need this with SM6 requirement, but shouldn't hurt
		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);

		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		if (FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(Parameters.Platform) == ERHIFeatureSupport::RuntimeGuaranteed)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Directional lights are always in separate passes as forward light data structure currently
		// only contains a single directional light.
		if( PermutationVector.Get< FDirectionalLightDim >() && PermutationVector.Get< FOnePassProjectionDim >() )
		{
			return false;
		}
		
		return DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVirtualShadowMapProjectionCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapProjection.usf", "VirtualShadowMapProjection", SF_Compute);

static float GetNormalBiasForShader()
{
	return CVarNormalBias.GetValueOnRenderThread() / 1000.0f;
}

static void RenderVirtualShadowMapProjectionCommon(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FViewInfo& View, int32 ViewIndex,
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const FIntRect ProjectionRect,
	EVirtualShadowMapProjectionInputType InputType,
	FRDGTextureRef OutputTexture,
	const FLightSceneProxy* LightProxy = nullptr,
	int32 VirtualShadowMapId = INDEX_NONE)
{
	check(GRHISupportsWaveOperations);

	// Use hair strands data (i.e., hair voxel tracing) only for Gbuffer input for casting hair shadow onto opaque geometry.
	const bool bHasHairStrandsData = HairStrands::HasViewHairStrandsData(View);

	FVirtualShadowMapProjectionCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FVirtualShadowMapProjectionCS::FParameters >();
	PassParameters->SamplingParameters = VirtualShadowMapArray.GetSamplingParameters(GraphBuilder);
	PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->ProjectionRect = FIntVector4(ProjectionRect.Min.X, ProjectionRect.Min.Y, ProjectionRect.Max.X, ProjectionRect.Max.Y);
	PassParameters->ScreenRayLength = CVarScreenRayLength.GetValueOnRenderThread();
	PassParameters->NormalBias = GetNormalBiasForShader();
	PassParameters->SubsurfaceShadowMode = CVarSubsurfaceShadowMode.GetValueOnRenderThread();
	PassParameters->SubsurfaceMinSourceRadius = FMath::Sin(0.5f * FMath::DegreesToRadians(CVarSubsurfaceShadowMinSourceAngle.GetValueOnRenderThread()));
	PassParameters->InputType = uint32(InputType);
	PassParameters->bCullBackfacingPixels = VirtualShadowMapArray.ShouldCullBackfacingPixels() ? 1 : 0;
	PassParameters->bSMRTUseAdaptiveRayCount = CVarSMRTAdaptiveRayCount.GetValueOnRenderThread() != 0 ? 1 : 0;
	PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
	if (bHasHairStrandsData)
	{
		PassParameters->HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
		PassParameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
	}

	bool bDirectionalLight = false;
	bool bOnePassProjection = LightProxy == nullptr;
	if (bOnePassProjection)
	{
		// One pass projection
		PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		PassParameters->OutShadowMaskBits = GraphBuilder.CreateUAV( OutputTexture );	
	}
	else
	{
		// Pass per light
		bDirectionalLight = LightProxy->GetLightType() == LightType_Directional;
		FLightRenderParameters LightParameters;
		LightProxy->GetLightShaderParameters(LightParameters);
		LightParameters.MakeShaderParameters(View.ViewMatrices, View.GetLastEyeAdaptationExposure(), PassParameters->Light);
		PassParameters->LightUniformVirtualShadowMapId = VirtualShadowMapId;
		PassParameters->OutShadowFactor = GraphBuilder.CreateUAV( OutputTexture );
	}
 
	if (bDirectionalLight)
	{
		PassParameters->SMRTRayCount = CVarSMRTRayCountDirectional.GetValueOnRenderThread();
		PassParameters->SMRTSamplesPerRay = CVarSMRTSamplesPerRayDirectional.GetValueOnRenderThread();
		PassParameters->SMRTRayLengthScale = CVarSMRTRayLengthScaleDirectional.GetValueOnRenderThread();
		PassParameters->SMRTCotMaxRayAngleFromLight = 0.0f;	// unused in this path
		PassParameters->SMRTTexelDitherScale = CVarSMRTTexelDitherScaleDirectional.GetValueOnRenderThread();
		PassParameters->SMRTExtrapolateSlope = CVarSMRTExtrapolateMaxSlopeDirectional.GetValueOnRenderThread();
		PassParameters->SMRTMaxSlopeBias = 0.0f; // unused in this path
	}
	else
	{
		PassParameters->SMRTRayCount = CVarSMRTRayCountLocal.GetValueOnRenderThread();
		PassParameters->SMRTSamplesPerRay = CVarSMRTSamplesPerRayLocal.GetValueOnRenderThread();
		PassParameters->SMRTRayLengthScale = 0.0f;		// unused in this path
		PassParameters->SMRTCotMaxRayAngleFromLight = 1.0f / FMath::Tan(CVarSMRTMaxRayAngleFromLight.GetValueOnRenderThread());
		PassParameters->SMRTTexelDitherScale = CVarSMRTTexelDitherScaleLocal.GetValueOnRenderThread();
		PassParameters->SMRTExtrapolateSlope = CVarSMRTExtrapolateMaxSlopeLocal.GetValueOnRenderThread();
		PassParameters->SMRTMaxSlopeBias = CVarSMRTMaxSlopeBiasLocal.GetValueOnRenderThread();
	}
	
	bool bDebugOutput = false;
#if !UE_BUILD_SHIPPING
	if ( !VirtualShadowMapArray.DebugVisualizationOutput.IsEmpty() && InputType == EVirtualShadowMapProjectionInputType::GBuffer && VirtualShadowMapArray.VisualizeLight[ViewIndex].IsValid())
	{
		const FVirtualShadowMapVisualizationData& VisualizationData = GetVirtualShadowMapVisualizationData();

		bDebugOutput = true;
		PassParameters->VisualizeModeId = VisualizationData.GetActiveModeID();
		PassParameters->VisualizeVirtualShadowMapId = VirtualShadowMapArray.VisualizeLight[ViewIndex].GetVirtualShadowMapId();
		PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV( VirtualShadowMapArray.PhysicalPageMetaDataRDG );
		PassParameters->OutVisualize = GraphBuilder.CreateUAV( VirtualShadowMapArray.DebugVisualizationOutput[ViewIndex] );
	}
#endif

	// If the requested samples per ray matches one of our static permutations, pick that one
	// Otherwise use the dynamic samples per ray permutation (-1).
	int StaticSamplesPerRay = PassParameters->SMRTSamplesPerRay == 0 ? PassParameters->SMRTSamplesPerRay : -1;

	FVirtualShadowMapProjectionCS::FPermutationDomain PermutationVector;
	PermutationVector.Set< FVirtualShadowMapProjectionCS::FDirectionalLightDim >( bDirectionalLight );
	PermutationVector.Set< FVirtualShadowMapProjectionCS::FOnePassProjectionDim >( bOnePassProjection );
	PermutationVector.Set< FVirtualShadowMapProjectionCS::FHairStrandsDim >( bHasHairStrandsData );
	PermutationVector.Set< FVirtualShadowMapProjectionCS::FVisualizeOutputDim >( bDebugOutput );
	PermutationVector.Set< FVirtualShadowMapProjectionCS::FExtrapolateSlopeDim >( PassParameters->SMRTExtrapolateSlope > 0.0f );
	PermutationVector.Set< FVirtualShadowMapProjectionCS::FSMRTStaticSampleCount >( StaticSamplesPerRay );
#if MAX_TEST_PERMUTATION > 0
	{
		int32 TestPermutation = FMath::Clamp(CVarTestPermutation.GetValueOnRenderThread(), 0, MAX_TEST_PERMUTATION);
		PermutationVector.Set< FVirtualShadowMapProjectionCS::FTestDim >( TestPermutation );
	}
#endif

	auto ComputeShader = View.ShaderMap->GetShader< FVirtualShadowMapProjectionCS >( PermutationVector );
	ClearUnusedGraphResources( ComputeShader, PassParameters );
	ValidateShaderParameters( ComputeShader, *PassParameters );

	const FIntPoint GroupCount = FIntPoint::DivideAndRoundUp( ProjectionRect.Size(), 8 );
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("VirtualShadowMapProjection(RayCount:%u(%s),SamplesPerRay:%u,Input:%s%s)",
			PassParameters->SMRTRayCount,
			PassParameters->bSMRTUseAdaptiveRayCount ? TEXT("Adaptive") : TEXT("Static"), 
			PassParameters->SMRTSamplesPerRay,
			ToString(InputType),
			bDebugOutput ? TEXT(",Debug") : TEXT("")),
		ComputeShader,
		PassParameters,
		FIntVector( GroupCount.X, GroupCount.Y, 1 )
	);
}

FRDGTextureRef CreateVirtualShadowMapMaskBits(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const TCHAR* Name)
{
	const FRDGTextureDesc ShadowMaskDesc = FRDGTextureDesc::Create2D(
		SceneTextures.Config.Extent,
		VirtualShadowMapArray.GetPackedShadowMaskFormat(),
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);

	return GraphBuilder.CreateTexture(ShadowMaskDesc, Name);
}

void RenderVirtualShadowMapProjectionOnePass(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FViewInfo& View, int32 ViewIndex,
	FVirtualShadowMapArray& VirtualShadowMapArray,
	EVirtualShadowMapProjectionInputType InputType,
	FRDGTextureRef ShadowMaskBits)
{
	FIntRect ProjectionRect = View.ViewRect;

	RenderVirtualShadowMapProjectionCommon(
		GraphBuilder,
		SceneTextures,
		View, ViewIndex,
		VirtualShadowMapArray,
		ProjectionRect,
		InputType,
		ShadowMaskBits);
}

static FRDGTextureRef CreateShadowMaskTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent)
{
	const FLinearColor ClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
		Extent,
		PF_G16R16,
		FClearValueBinding(ClearColor),
		TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureRef Texture = GraphBuilder.CreateTexture(Desc, TEXT("Shadow.Virtual.ShadowMask"));

	// NOTE: Projection pass writes all relevant pixels, so should not need to clear here
	if (CVarForcePerLightShadowMaskClear.GetValueOnRenderThread() != 0)
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Texture), ClearColor);
	}

	return Texture;
}

void RenderVirtualShadowMapProjection(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FViewInfo& View, int32 ViewIndex,
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const FIntRect ScissorRect,
	EVirtualShadowMapProjectionInputType InputType,
	const FLightSceneInfo& LightSceneInfo,
	int32 VirtualShadowMapId,
	FRDGTextureRef OutputShadowMaskTexture)
{
	FRDGTextureRef VirtualShadowMaskTexture = CreateShadowMaskTexture(GraphBuilder, View.ViewRect.Max);

	RenderVirtualShadowMapProjectionCommon(
		GraphBuilder,
		SceneTextures,
		View, ViewIndex,
		VirtualShadowMapArray,
		ScissorRect,
		InputType,
		VirtualShadowMaskTexture,
		LightSceneInfo.Proxy,
		VirtualShadowMapId);

	CompositeVirtualShadowMapMask(
		GraphBuilder,
		View,
		ScissorRect,
		VirtualShadowMaskTexture,
		false,	// bDirectionalLight
		OutputShadowMaskTexture);
}

void RenderVirtualShadowMapProjection(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FViewInfo& View, int32 ViewIndex,
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const FIntRect ScissorRect,
	EVirtualShadowMapProjectionInputType InputType,
	const TSharedPtr<FVirtualShadowMapClipmap>& Clipmap,
	FRDGTextureRef OutputShadowMaskTexture)
{
	FRDGTextureRef VirtualShadowMaskTexture = CreateShadowMaskTexture(GraphBuilder, View.ViewRect.Max);

	RenderVirtualShadowMapProjectionCommon(
		GraphBuilder,
		SceneTextures,
		View, ViewIndex,
		VirtualShadowMapArray,		
		ScissorRect,
		InputType,
		VirtualShadowMaskTexture,
		Clipmap->GetLightSceneInfo().Proxy,
		Clipmap->GetVirtualShadowMap()->ID);
	
	CompositeVirtualShadowMapMask(
		GraphBuilder,
		View,
		ScissorRect,
		VirtualShadowMaskTexture,
		true,	// bDirectionalLight
		OutputShadowMaskTexture);
}

// Composite denoised shadow projection mask onto the light's shadow mask
// Basically just a copy shader with a special blend mode
class FVirtualShadowMapProjectionCompositePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVirtualShadowMapProjectionCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualShadowMapProjectionCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputShadowFactor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Required right now due to where the shader function lives, but not actually used
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVirtualShadowMapProjectionCompositePS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapProjectionComposite.usf", "VirtualShadowMapCompositePS", SF_Pixel);

void CompositeVirtualShadowMapMask(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FIntRect ScissorRect,
	const FRDGTextureRef Input,
	bool bDirectionalLight,
	FRDGTextureRef OutputShadowMaskTexture)
{
	FVirtualShadowMapProjectionCompositePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualShadowMapProjectionCompositePS::FParameters>();
	PassParameters->InputShadowFactor = Input;

	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputShadowMaskTexture, ERenderTargetLoadAction::ELoad);

	FRHIBlendState* BlendState = FProjectedShadowInfo::GetBlendStateForProjection(
		0,					// ShadowMapChannel
		bDirectionalLight,	// bIsWholeSceneDirectionalShadow,
		false,				// bUseFadePlane
		false,				// bProjectingForForwardShading, 
		false				// bMobileModulatedProjections
	);

	auto PixelShader = View.ShaderMap->GetShader<FVirtualShadowMapProjectionCompositePS>();
	ValidateShaderParameters(PixelShader, *PassParameters);

	FPixelShaderUtils::AddFullscreenPass(GraphBuilder,
		View.ShaderMap,
		RDG_EVENT_NAME("CompositeVirtualShadowMapMask"),
		PixelShader,
		PassParameters,
		ScissorRect,
		BlendState);
}

class FVirtualShadowMapProjectionCompositeFromMaskBitsPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVirtualShadowMapProjectionCompositeFromMaskBitsPS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualShadowMapProjectionCompositeFromMaskBitsPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, SamplingParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint4>, ShadowMaskBits)
		SHADER_PARAMETER(FIntVector4, ProjectionRect)
		SHADER_PARAMETER(int32, CompositeVirtualShadowMapId)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Required right now due to where the shader function lives, but not actually used
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVirtualShadowMapProjectionCompositeFromMaskBitsPS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapProjectionComposite.usf", "VirtualShadowMapCompositeFromMaskBitsPS", SF_Pixel);

void CompositeVirtualShadowMapFromMaskBits(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FViewInfo& View,
	const FIntRect ScissorRect,
	FVirtualShadowMapArray& VirtualShadowMapArray,
	EVirtualShadowMapProjectionInputType InputType,
	int32 VirtualShadowMapId,
	FRDGTextureRef ShadowMaskBits,
	FRDGTextureRef OutputShadowMaskTexture)
{
	FIntRect ProjectionRect = View.ViewRect;

	FVirtualShadowMapProjectionCompositeFromMaskBitsPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualShadowMapProjectionCompositeFromMaskBitsPS::FParameters>();
	PassParameters->SamplingParameters = VirtualShadowMapArray.GetSamplingParameters(GraphBuilder);
	PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
	PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->ProjectionRect = FIntVector4(ProjectionRect.Min.X, ProjectionRect.Min.Y, ProjectionRect.Max.X, ProjectionRect.Max.Y);
	PassParameters->InputDepthTexture = SceneTextures.UniformBuffer->GetParameters()->SceneDepthTexture;
	if (InputType == EVirtualShadowMapProjectionInputType::HairStrands)
	{
		PassParameters->InputDepthTexture = View.HairStrandsViewData.VisibilityData.HairOnlyDepthTexture;
	}
	PassParameters->ShadowMaskBits = ShadowMaskBits;
	PassParameters->CompositeVirtualShadowMapId = VirtualShadowMapId;

	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputShadowMaskTexture, ERenderTargetLoadAction::ELoad);

	FRHIBlendState* BlendState = FProjectedShadowInfo::GetBlendStateForProjection(
		0,					// ShadowMapChannel
		false,				// bIsWholeSceneDirectionalShadow,
		false,				// bUseFadePlane
		false,				// bProjectingForForwardShading, 
		false				// bMobileModulatedProjections
	);

	auto PixelShader = View.ShaderMap->GetShader<FVirtualShadowMapProjectionCompositeFromMaskBitsPS>();
	ValidateShaderParameters(PixelShader, *PassParameters);
	FPixelShaderUtils::AddFullscreenPass(GraphBuilder,
		View.ShaderMap,
		RDG_EVENT_NAME("CompositeVirtualShadowMapFromMaskBits"),
		PixelShader,
		PassParameters,
		ProjectionRect,
		BlendState);
}
