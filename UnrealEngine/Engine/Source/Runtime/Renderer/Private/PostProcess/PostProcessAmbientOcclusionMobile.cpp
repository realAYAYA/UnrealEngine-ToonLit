// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbientOcclusionMobile.cpp
=============================================================================*/

#include "PostProcess/PostProcessAmbientOcclusionMobile.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderParameterStruct.h"
#include "SceneRendering.h"
#include "RenderTargetPool.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SystemTextures.h"
#include "ScreenPass.h"
#include "ScenePrivate.h"
#include "SceneTextureParameters.h"
#include "SceneRenderTargetParameters.h"
#include "ClearQuad.h"
#include "PixelShaderUtils.h"

static TAutoConsoleVariable<int32> CVarMobileAmbientOcclusion(
	TEXT("r.Mobile.AmbientOcclusion"),
	0,
	TEXT("Caution: An extra sampler will be occupied in mobile base pass pixel shader after enable the mobile ambient occlusion.\n")
	TEXT("0: Disable Ambient Occlusion on mobile platform. [default]\n")
	TEXT("1: Enable Ambient Occlusion on mobile platform.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMobileAmbientOcclusionTechnique(
	TEXT("r.Mobile.AmbientOcclusionTechnique"),
	0,
	TEXT("0: GTAO (default).\n")
	TEXT("1: SSAO.\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMobileGTAOPreIntegratedTextureType(
	TEXT("r.Mobile.GTAOPreIntegratedTextureType"),
	2,
	TEXT("0: No Texture.\n")
	TEXT("1: Texture2D LUT.\n")
	TEXT("2: Volume LUT(Default)."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarMobileAmbientOcclusionQuality(
	TEXT("r.Mobile.AmbientOcclusionQuality"),
	1,
	TEXT("The quality of screen space ambient occlusion on mobile platform.\n")
	TEXT("0: Disabled.\n")
	TEXT("1: Low.(Default)\n")
	TEXT("2: Medium.\n")
	TEXT("3: High.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMobileAmbientOcclusionShaderType(
	TEXT("r.Mobile.AmbientOcclusionShaderType"),
	2,
	TEXT("0: ComputeShader.\n")
	TEXT("1: Seperate ComputeShader.\n")
	TEXT("2: PixelShader.\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMobileAmbientOcclusionDepthBoundsTest(
	TEXT("r.Mobile.AmbientOcclusionDepthBoundsTest"),
	1,
	TEXT("Whether to use depth bounds test to cull distant pixels during AO pass. This option is only valid when pixel shader path is used"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileSSAOHalfResolution(
	TEXT("r.Mobile.SSAOHalfResolution"),
	0,
	TEXT("Whether to calculate SSAO at half resolution.\n")
	TEXT("0: Disabled.\n")
	TEXT("1: Half Resolution with bilinear upsample\n")
	TEXT("2: Half Resolution with 4 tap bilateral upsample\n")
	TEXT("3: Half Resolution with 9 tap bilateral upsample\n"),
	ECVF_RenderThreadSafe);

// --------------------------------------------------------------------------------------------------------------------
DECLARE_GPU_STAT_NAMED(MobileSSAO, TEXT("SSAO"));


// --------------------------------------------------------------------------------------------------------------------

bool IsUsingMobileAmbientOcclusion(EShaderPlatform ShaderPlatform)
{
	static const auto MobileAmbientOcclusionQualityCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AmbientOcclusionQuality"));

	return IsMobileAmbientOcclusionEnabled(ShaderPlatform) && MobileAmbientOcclusionQualityCVar->GetValueOnAnyThread() > 0;
}

// --------------------------------------------------------------------------------------------------------------------
class FGTAOMobile_HorizonSearchIntegral : public FGlobalShader
{
public:
	class FLUTTextureTypeDim : SHADER_PERMUTATION_INT("PREINTEGRATED_LUT_TYPE", 3);
	class FShaderQualityDim : SHADER_PERMUTATION_INT("SHADER_QUALITY", 3);

	using FCommonPermutationDomain = TShaderPermutationDomain<
		FLUTTextureTypeDim,
		FShaderQualityDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_EX(FVector4f, ViewRectMin, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, DepthBufferSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, BufferSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, ViewSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER(FVector4f, FadeRadiusMulAdd_FadeDistance_AttenFactor)
		SHADER_PARAMETER(FVector4f, WorldRadiusAdj_SinDeltaAngle_CosDeltaAngle_Thickness)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NormalTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, NormalSampler)

		SHADER_PARAMETER_TEXTURE(Texture2D, GTAOPreIntegrated2D)
		SHADER_PARAMETER_TEXTURE(Texture3D, GTAOPreIntegrated3D)
		SHADER_PARAMETER_SAMPLER(SamplerState, GTAOPreIntegratedSampler)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, const FCommonPermutationDomain& CommonPermutationVector)
	{
		auto LUTTextureType = CommonPermutationVector.Get<FLUTTextureTypeDim>();

		int32 MobileGTAOPreIntegratedTextureType = CVarMobileGTAOPreIntegratedTextureType.GetValueOnAnyThread();
		return IsMobileAmbientOcclusionEnabled(Parameters.Platform) && (MobileGTAOPreIntegratedTextureType == LUTTextureType);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_NORMALBUFFER"), 0);
	}

	static FCommonPermutationDomain BuildPermutationVector(int32 LUTTextureType, int32 ShaderQuality)
	{
		FCommonPermutationDomain PermutationVector;
		PermutationVector.Set<FLUTTextureTypeDim>(LUTTextureType);
		PermutationVector.Set<FShaderQualityDim>(ShaderQuality);
		return PermutationVector;
	}

	static void SetupShaderParameters(FParameters& ShaderParameters, FRDGBuilder& GraphBuilder, const FViewInfo& View, const FIntRect& ViewRect, const FIntPoint& DepthBufferSize, const FIntPoint& BufferSize, const FVector4f& FallOffStartEndScaleBias, const FVector4f& WorldRadiusAdjSinCosDeltaAngleThickness, FRDGTextureRef SceneDepthTexture)
	{
		const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;

		float FadeRadius = FMath::Max(1.0f, Settings.AmbientOcclusionFadeRadius);
		float InvFadeRadius = 1.0f / FadeRadius;

		ShaderParameters.View = View.ViewUniformBuffer;
		ShaderParameters.ViewRectMin = FVector4f(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, 0.0f);
		ShaderParameters.DepthBufferSizeAndInvSize = FVector4f(DepthBufferSize.X, DepthBufferSize.Y, 1.0f / DepthBufferSize.X, 1.0f / DepthBufferSize.Y);
		ShaderParameters.BufferSizeAndInvSize = FVector4f(BufferSize.X, BufferSize.Y, 1.0f / BufferSize.X, 1.0f / BufferSize.Y);
		ShaderParameters.ViewSizeAndInvSize = FVector4f(ViewRect.Width(), ViewRect.Height(), 1.0f / ViewRect.Width(), 1.0f / ViewRect.Height());
		ShaderParameters.FadeRadiusMulAdd_FadeDistance_AttenFactor = FVector4f(InvFadeRadius, -(Settings.AmbientOcclusionFadeDistance - FadeRadius) * InvFadeRadius, Settings.AmbientOcclusionFadeDistance, 2.0f / (FallOffStartEndScaleBias.Y * FallOffStartEndScaleBias.Y));
		ShaderParameters.WorldRadiusAdj_SinDeltaAngle_CosDeltaAngle_Thickness = WorldRadiusAdjSinCosDeltaAngleThickness;

		ShaderParameters.SceneDepthTexture = SceneDepthTexture;
		ShaderParameters.SceneDepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		if (GSystemTextures.GTAOPreIntegrated.IsValid())
		{
			ShaderParameters.GTAOPreIntegrated2D = GSystemTextures.GTAOPreIntegrated->GetRHI();
			ShaderParameters.GTAOPreIntegrated3D = GSystemTextures.GTAOPreIntegrated->GetRHI();
			ShaderParameters.GTAOPreIntegratedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		}
	}

	FGTAOMobile_HorizonSearchIntegral() = default;
	FGTAOMobile_HorizonSearchIntegral(const CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

class FGTAOMobile_HorizonSearchIntegralSpatialFilterCS : public FGTAOMobile_HorizonSearchIntegral
{
	using Super = FGTAOMobile_HorizonSearchIntegral;

public:
	// Changing these numbers requires PostProcessAmbientOcclusionMobile.usf to be recompiled.
	// The maximum thread group is 512 on IOS A9 and A10 and the shared memory is 16K
	static const uint32 ThreadGroupSizeX = 32;
	static const uint32 ThreadGroupSizeY = 32;

	// The number of texels on each axis processed by a single thread group.
	static const FIntPoint TexelsPerThreadGroup;

	DECLARE_GLOBAL_SHADER(FGTAOMobile_HorizonSearchIntegralSpatialFilterCS);
	SHADER_USE_PARAMETER_STRUCT(FGTAOMobile_HorizonSearchIntegralSpatialFilterCS, Super);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGTAOMobile_HorizonSearchIntegral::FParameters, Common)
		SHADER_PARAMETER_EX(FVector4f, Power_Intensity_ScreenPixelsToSearch, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<half4>, OutTexture)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<
		Super::FCommonPermutationDomain>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return Super::ShouldCompilePermutation(Parameters, PermutationVector.Get<Super::FCommonPermutationDomain>());
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("HORIZONSEARCH_INTEGRAL_SPATIALFILTER_COMPUTE_SHADER"), 1u);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}

	static FPermutationDomain BuildPermutationVector(int32 LUTTextureType, int32 ShaderQuality)
	{
		FPermutationDomain PermutationVector;
		PermutationVector.Set<Super::FCommonPermutationDomain>(Super::BuildPermutationVector(LUTTextureType, ShaderQuality));
		return PermutationVector;
	}
};

const FIntPoint FGTAOMobile_HorizonSearchIntegralSpatialFilterCS::TexelsPerThreadGroup(ThreadGroupSizeX, ThreadGroupSizeY);

IMPLEMENT_GLOBAL_SHADER(FGTAOMobile_HorizonSearchIntegralSpatialFilterCS, "/Engine/Private/PostProcessAmbientOcclusionMobile.usf", "GTAOHorizonSearchIntegralSpatialFilterCS", SF_Compute);

class FGTAOMobile_HorizonSearchIntegralCS : public FGTAOMobile_HorizonSearchIntegral
{
	using Super = FGTAOMobile_HorizonSearchIntegral;
public:
	// Changing these numbers requires PostProcessAmbientOcclusionMobile.usf to be recompiled.
	// Use smaller thread group for low end devices
	static const uint32 ThreadGroupSizeX = 16;
	static const uint32 ThreadGroupSizeY = 8;

	// The number of texels on each axis processed by a single thread group.
	static const FIntPoint TexelsPerThreadGroup;

	DECLARE_GLOBAL_SHADER(FGTAOMobile_HorizonSearchIntegralCS);
	SHADER_USE_PARAMETER_STRUCT(FGTAOMobile_HorizonSearchIntegralCS, Super);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGTAOMobile_HorizonSearchIntegral::FParameters, Common)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<half4>, OutTexture)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<
		Super::FCommonPermutationDomain>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return Super::ShouldCompilePermutation(Parameters, PermutationVector.Get<Super::FCommonPermutationDomain>());
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("HORIZONSEARCH_INTEGRAL_COMPUTE_SHADER"), 1u);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}

	static FPermutationDomain BuildPermutationVector(int32 LUTTextureType, int32 ShaderQuality)
	{
		FPermutationDomain PermutationVector;
		PermutationVector.Set<Super::FCommonPermutationDomain>(Super::BuildPermutationVector(LUTTextureType, ShaderQuality));
		return PermutationVector;
	}
};

const FIntPoint FGTAOMobile_HorizonSearchIntegralCS::TexelsPerThreadGroup(ThreadGroupSizeX, ThreadGroupSizeY);

IMPLEMENT_GLOBAL_SHADER(FGTAOMobile_HorizonSearchIntegralCS, "/Engine/Private/PostProcessAmbientOcclusionMobile.usf", "GTAOHorizonSearchIntegralCS", SF_Compute);

class FGTAOMobile_SpatialFilter : public FGlobalShader
{
public:

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_EX(FVector4f, ViewRectMin, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, BufferSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, ViewSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, Power_Intensity_ScreenPixelsToSearch, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AOInputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, AOInputSampler)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return IsMobileAmbientOcclusionEnabled(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static void SetupShaderParameters(FParameters& ShaderParameters, const FViewInfo& View, const FIntRect& ViewRect, const FIntPoint& BufferSize, FRDGTextureRef HorizonSearchIntegralTexture)
	{
		const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;

		ShaderParameters.ViewRectMin = FVector4f(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, 0.0f);
		ShaderParameters.BufferSizeAndInvSize = FVector4f(BufferSize.X, BufferSize.Y, 1.0f / BufferSize.X, 1.0f / BufferSize.Y);
		ShaderParameters.ViewSizeAndInvSize = FVector4f(ViewRect.Width(), ViewRect.Height(), 1.0f / ViewRect.Width(), 1.0f / ViewRect.Height());
		ShaderParameters.Power_Intensity_ScreenPixelsToSearch = FVector4f(Settings.AmbientOcclusionPower * 0.5f, Settings.AmbientOcclusionIntensity, 0.0f, 0.0f);

		ShaderParameters.AOInputTexture = HorizonSearchIntegralTexture;
		ShaderParameters.AOInputSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	FGTAOMobile_SpatialFilter() = default;
	FGTAOMobile_SpatialFilter(const CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

class FGTAOMobile_SpatialFilterCS : public FGTAOMobile_SpatialFilter
{
	using Super = FGTAOMobile_SpatialFilter;
public:
	// Changing these numbers requires PostProcessAmbientOcclusionMobile.usf to be recompiled.
	// Use smaller thread group for low end devices
	static const uint32 ThreadGroupSizeX = 16;
	static const uint32 ThreadGroupSizeY = 8;

	// The number of texels on each axis processed by a single thread group.
	static const FIntPoint TexelsPerThreadGroup;

	DECLARE_GLOBAL_SHADER(FGTAOMobile_SpatialFilterCS);
	SHADER_USE_PARAMETER_STRUCT(FGTAOMobile_SpatialFilterCS, Super);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGTAOMobile_SpatialFilter::FParameters, Common)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<half4>, OutTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Super::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SPATIALFILTER_COMPUTE_SHADER"), 1u);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}
};

const FIntPoint FGTAOMobile_SpatialFilterCS::TexelsPerThreadGroup(ThreadGroupSizeX, ThreadGroupSizeY);

IMPLEMENT_GLOBAL_SHADER(FGTAOMobile_SpatialFilterCS, "/Engine/Private/PostProcessAmbientOcclusionMobile.usf", "GTAOSpatialFilterCS", SF_Compute);

class FGTAOMobile_HorizonSearchIntegralPS : public FGTAOMobile_HorizonSearchIntegral
{
	using Super = FGTAOMobile_HorizonSearchIntegral;
public:

	DECLARE_GLOBAL_SHADER(FGTAOMobile_HorizonSearchIntegralPS);
	SHADER_USE_PARAMETER_STRUCT(FGTAOMobile_HorizonSearchIntegralPS, Super);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGTAOMobile_HorizonSearchIntegral::FParameters, Common)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<
		Super::FCommonPermutationDomain>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return Super::ShouldCompilePermutation(Parameters, PermutationVector.Get<Super::FCommonPermutationDomain>());
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("HORIZONSEARCH_INTEGRAL_PIXEL_SHADER"), 1u);
		OutEnvironment.SetDefine(TEXT("FORCE_DEPTH_TEXTURE_READS"), 1);
	}

	static FPermutationDomain BuildPermutationVector(int32 LUTTextureType, int32 ShaderQuality)
	{
		FPermutationDomain PermutationVector;
		PermutationVector.Set<Super::FCommonPermutationDomain>(Super::BuildPermutationVector(LUTTextureType, ShaderQuality));
		return PermutationVector;
	}
};

IMPLEMENT_GLOBAL_SHADER(FGTAOMobile_HorizonSearchIntegralPS, "/Engine/Private/PostProcessAmbientOcclusionMobile.usf", "GTAOHorizonSearchIntegralPS", SF_Pixel);

class FGTAOMobile_SpatialFilterPS : public FGTAOMobile_SpatialFilter
{
	using Super = FGTAOMobile_SpatialFilter;
public:
	DECLARE_GLOBAL_SHADER(FGTAOMobile_SpatialFilterPS);
	SHADER_USE_PARAMETER_STRUCT(FGTAOMobile_SpatialFilterPS, Super);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGTAOMobile_SpatialFilter::FParameters, Common)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Super::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SPATIALFILTER_PIXEL_SHADER"), 1u);
		OutEnvironment.SetDefine(TEXT("FORCE_DEPTH_TEXTURE_READS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGTAOMobile_SpatialFilterPS, "/Engine/Private/PostProcessAmbientOcclusionMobile.usf", "GTAOSpatialFilterPS", SF_Pixel);

FRDGTextureRef CreateMobileScreenSpaceAOTexture(FRDGBuilder& GraphBuilder, const FSceneTexturesConfig& Config)
{
	bool bGTAO = (CVarMobileAmbientOcclusionTechnique.GetValueOnRenderThread() == 0);
	const uint32 DownsampleFactor = bGTAO ? 2 : 1;

	const FIntPoint Extent = FIntPoint::DivideAndRoundUp(Config.Extent, DownsampleFactor);

	EPixelFormat Format = PF_G8;

	// G8 isn't supported as UAV on Android OpenGLES, fall back to RGBA8 for compute shader usage.
	if (bGTAO
		&& IsOpenGLPlatform(Config.ShaderPlatform)
		&& ((GetMaxWorkGroupInvocations() >= 1024 && CVarMobileAmbientOcclusionShaderType.GetValueOnRenderThread() == 0) || CVarMobileAmbientOcclusionShaderType.GetValueOnRenderThread() == 1)
		)
	{
		Format = PF_R8G8B8A8;
	}

	return GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Extent, Format, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV),
		TEXT("ScreenSpaceAO"));
}

static void RenderGTAO(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneDepthTexture, FRDGTextureRef AmbientOcclusionTexture, const TArray<FViewInfo>& Views)
{
	static const auto GTAOThicknessBlendCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.GTAO.ThicknessBlend"));
	static const auto GTAOFalloffStartRatioCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.GTAO.FalloffStartRatio"));
	static const auto GTAOFalloffEndCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.GTAO.FalloffEnd"));
	static const auto GTAONumAnglesCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.GTAO.NumAngles"));
	const uint32 DownsampleFactor = 2;

	const int32 MobileGTAOPreIntegratedTextureType = FMath::Min(CVarMobileGTAOPreIntegratedTextureType.GetValueOnRenderThread(), 2);
	const int32 MobileAmbientOcclusionQuality = FMath::Min(CVarMobileAmbientOcclusionQuality.GetValueOnRenderThread(), 3);

	FRDGTextureUAVRef AmbientOcclusionTextureUAV = GraphBuilder.CreateUAV(AmbientOcclusionTexture);

	const FIntPoint& DepthBufferSize = SceneDepthTexture->Desc.Extent;
	const FIntPoint& BufferSize = AmbientOcclusionTexture->Desc.Extent;

	float FallOffEnd = GTAOFalloffEndCVar ? GTAOFalloffEndCVar->GetValueOnRenderThread() : 200.0f;
	float FallOffStartRatio = GTAOFalloffStartRatioCVar ? FMath::Clamp(GTAOFalloffStartRatioCVar->GetValueOnRenderThread(), 0.0f, 0.999f) : 0.5f;
	float FallOffStart = FallOffEnd * FallOffStartRatio;
	float FallOffStartSq = FallOffStart * FallOffStart;
	float FallOffEndSq = FallOffEnd * FallOffEnd;

	float FallOffScale = 1.0f / (FallOffEndSq - FallOffStartSq);
	float FallOffBias = -FallOffStartSq * FallOffScale;

	FVector4f FallOffStartEndScaleBias(FallOffStart, FallOffEnd, FallOffScale, FallOffBias);

	float ThicknessBlend = GTAOThicknessBlendCVar ? GTAOThicknessBlendCVar->GetValueOnRenderThread() : 0.5f;
	ThicknessBlend = FMath::Clamp(1.0f - (ThicknessBlend*ThicknessBlend), 0.0f, 0.99f);

	float NumAngles = GTAONumAnglesCVar ? FMath::Clamp(GTAONumAnglesCVar->GetValueOnRenderThread(), 1.0f, 16.0f) : 2;
	float SinDeltaAngle, CosDeltaAngle;
	FMath::SinCos(&SinDeltaAngle, &CosDeltaAngle, PI / NumAngles);

	ETextureCreateFlags TextureCreateFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV;
	FRDGTextureRef HorizonSearchIntegralTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(BufferSize, PF_R8G8B8A8, FClearValueBinding::Black, TextureCreateFlags), TEXT("HorizonSearchIntegralTexture"));
	FRDGTextureUAVRef HorizonSearchIntegralTextureUAV = GraphBuilder.CreateUAV(HorizonSearchIntegralTexture);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;

		const FIntRect& ViewRect = FIntRect::DivideAndRoundUp(View.ViewRect, DownsampleFactor);

		FVector4f WorldRadiusAdjSinCosDeltaAngleThickness(FallOffStartEndScaleBias.Y * DepthBufferSize.Y * View.ViewMatrices.GetProjectionMatrix().M[0][0], SinDeltaAngle, CosDeltaAngle, ThicknessBlend);

		if (GetMaxWorkGroupInvocations() >= 1024 && CVarMobileAmbientOcclusionShaderType.GetValueOnRenderThread() == 0)
		{
			FGTAOMobile_HorizonSearchIntegralSpatialFilterCS::FParameters* HorizonSearchIntegralSpatialFilterParameters = GraphBuilder.AllocParameters<FGTAOMobile_HorizonSearchIntegralSpatialFilterCS::FParameters>();
			FGTAOMobile_HorizonSearchIntegral::SetupShaderParameters(HorizonSearchIntegralSpatialFilterParameters->Common, GraphBuilder, View, ViewRect, DepthBufferSize, BufferSize, FallOffStartEndScaleBias, WorldRadiusAdjSinCosDeltaAngleThickness, SceneDepthTexture);
			
			HorizonSearchIntegralSpatialFilterParameters->Power_Intensity_ScreenPixelsToSearch = FVector4f(Settings.AmbientOcclusionPower * 0.5f, Settings.AmbientOcclusionIntensity, 0.0f, 0.0f);

			HorizonSearchIntegralSpatialFilterParameters->OutTexture = AmbientOcclusionTextureUAV;

			auto ComputeShaderPermutationVector = FGTAOMobile_HorizonSearchIntegralSpatialFilterCS::BuildPermutationVector(MobileGTAOPreIntegratedTextureType, MobileAmbientOcclusionQuality - 1);
			TShaderMapRef<FGTAOMobile_HorizonSearchIntegralSpatialFilterCS> ComputeShader(View.ShaderMap, ComputeShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("AmbientOcclusion_HorizonSearchIntegralSpatialFilter %dx%d (CS)", ViewRect.Width(), ViewRect.Height()),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				HorizonSearchIntegralSpatialFilterParameters,
				FComputeShaderUtils::GetGroupCount(ViewRect.Size(), FGTAOMobile_HorizonSearchIntegralSpatialFilterCS::TexelsPerThreadGroup));
		}
		else if (CVarMobileAmbientOcclusionShaderType.GetValueOnRenderThread() != 1)
		{
			TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);

			FScreenPassRenderTarget HorizonSearchIntegralRT(HorizonSearchIntegralTexture, ViewRect, ViewIndex > 0 ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::EClear);

			FGTAOMobile_HorizonSearchIntegralPS::FParameters* HorizonSearchIntegralParameters = GraphBuilder.AllocParameters<FGTAOMobile_HorizonSearchIntegralPS::FParameters>();
			FGTAOMobile_HorizonSearchIntegral::SetupShaderParameters(HorizonSearchIntegralParameters->Common, GraphBuilder, View, ViewRect, DepthBufferSize, BufferSize, FallOffStartEndScaleBias, WorldRadiusAdjSinCosDeltaAngleThickness, SceneDepthTexture);

			HorizonSearchIntegralParameters->RenderTargets[0] = HorizonSearchIntegralRT.GetRenderTargetBinding();

			auto HorizonSearchIntegralShaderPermutationVector = FGTAOMobile_HorizonSearchIntegralPS::BuildPermutationVector(MobileGTAOPreIntegratedTextureType, MobileAmbientOcclusionQuality - 1);
			TShaderMapRef<FGTAOMobile_HorizonSearchIntegralPS> HorizonSearchIntegralShader(View.ShaderMap, HorizonSearchIntegralShaderPermutationVector);

			ClearUnusedGraphResources(HorizonSearchIntegralShader, HorizonSearchIntegralParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("AmbientOcclusion_HorizonSearchIntegral %dx%d (PS)", ViewRect.Width(), ViewRect.Height()),
				HorizonSearchIntegralParameters,
				ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
				[VertexShader, HorizonSearchIntegralShader, HorizonSearchIntegralParameters, ViewRect, BufferSize](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = HorizonSearchIntegralShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, HorizonSearchIntegralShader, HorizonSearchIntegralShader.GetPixelShader(), *HorizonSearchIntegralParameters);

				DrawRectangle(
					RHICmdList,
					0, 0,
					BufferSize.X, BufferSize.Y,
					ViewRect.Min.X, ViewRect.Min.Y,
					ViewRect.Width(), ViewRect.Height(),
					BufferSize,
					BufferSize,
					VertexShader,
					EDRF_UseTriangleOptimization);
			});

			FScreenPassRenderTarget AmbientOcclusionRT(AmbientOcclusionTexture, ViewRect, ViewIndex > 0 ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::EClear);

			FGTAOMobile_SpatialFilterPS::FParameters* SpatialFilterParameters = GraphBuilder.AllocParameters<FGTAOMobile_SpatialFilterPS::FParameters>();
			FGTAOMobile_SpatialFilter::SetupShaderParameters(SpatialFilterParameters->Common, View, ViewRect, BufferSize, HorizonSearchIntegralTexture);
			SpatialFilterParameters->RenderTargets[0] = AmbientOcclusionRT.GetRenderTargetBinding();

			TShaderMapRef<FGTAOMobile_SpatialFilterPS> SpatialFilterShader(View.ShaderMap);

			ClearUnusedGraphResources(SpatialFilterShader, SpatialFilterParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("AmbientOcclusion_SpatialFilter %dx%d (PS)", ViewRect.Width(), ViewRect.Height()),
				SpatialFilterParameters,
				ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
				[VertexShader, SpatialFilterShader, SpatialFilterParameters, ViewRect, BufferSize](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = SpatialFilterShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, SpatialFilterShader, SpatialFilterShader.GetPixelShader(), *SpatialFilterParameters);

				DrawRectangle(
					RHICmdList,
					0, 0,
					BufferSize.X, BufferSize.Y,
					ViewRect.Min.X, ViewRect.Min.Y,
					ViewRect.Width(), ViewRect.Height(),
					BufferSize,
					BufferSize,
					VertexShader,
					EDRF_UseTriangleOptimization);
			});
		}
		else
		{
			FGTAOMobile_HorizonSearchIntegralCS::FParameters* HorizonSearchIntegralParameters = GraphBuilder.AllocParameters<FGTAOMobile_HorizonSearchIntegralCS::FParameters>();
			FGTAOMobile_HorizonSearchIntegral::SetupShaderParameters(HorizonSearchIntegralParameters->Common, GraphBuilder, View, ViewRect, DepthBufferSize, BufferSize, FallOffStartEndScaleBias, WorldRadiusAdjSinCosDeltaAngleThickness, SceneDepthTexture);

			HorizonSearchIntegralParameters->OutTexture = HorizonSearchIntegralTextureUAV;

			auto HorizonSearchIntegralShaderPermutationVector = FGTAOMobile_HorizonSearchIntegralCS::BuildPermutationVector(MobileGTAOPreIntegratedTextureType, MobileAmbientOcclusionQuality - 1);
			TShaderMapRef<FGTAOMobile_HorizonSearchIntegralCS> HorizonSearchIntegralShader(View.ShaderMap, HorizonSearchIntegralShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("AmbientOcclusion_HorizonSearchIntegral %dx%d (CS)", ViewRect.Width(), ViewRect.Height()),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				HorizonSearchIntegralShader,
				HorizonSearchIntegralParameters,
				FComputeShaderUtils::GetGroupCount(ViewRect.Size(), FGTAOMobile_HorizonSearchIntegralCS::TexelsPerThreadGroup));

			FGTAOMobile_SpatialFilterCS::FParameters* SpatialFilterParameters = GraphBuilder.AllocParameters<FGTAOMobile_SpatialFilterCS::FParameters>();
			FGTAOMobile_SpatialFilter::SetupShaderParameters(SpatialFilterParameters->Common, View, ViewRect, BufferSize, HorizonSearchIntegralTexture);
			SpatialFilterParameters->OutTexture = AmbientOcclusionTextureUAV;

			TShaderMapRef<FGTAOMobile_SpatialFilterCS> SpatialFilterShader(View.ShaderMap);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("AmbientOcclusion_SpatialFilter %dx%d (CS)", ViewRect.Width(), ViewRect.Height()),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				SpatialFilterShader,
				SpatialFilterParameters,
				FComputeShaderUtils::GetGroupCount(ViewRect.Size(), FGTAOMobile_SpatialFilterCS::TexelsPerThreadGroup));
		}

		if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
		{
			GraphBuilder.QueueTextureExtraction(AmbientOcclusionTexture, &View.ViewState->PrevFrameViewInfo.MobileAmbientOcclusion);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
struct FMobileSSAOCommonParameters
{
	TRDGUniformBufferRef<FMobileSceneTextureUniformParameters> SceneTexturesUniformBufferRDG;
	FScreenPassTextureViewport SceneTexturesViewport;

	FScreenPassTexture HZBInput;
	FScreenPassTexture SceneDepth;
};

static const uint32 kMobileSSAOParametersArraySize = 5; 

BEGIN_SHADER_PARAMETER_STRUCT(FMobileSSAOShaderParameters, )
	SHADER_PARAMETER_ARRAY(FVector4f, ScreenSpaceAOParams, [kMobileSSAOParametersArraySize])

	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, AOViewport)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, AOSceneViewport)
END_SHADER_PARAMETER_STRUCT();


BEGIN_SHADER_PARAMETER_STRUCT(FMobileAmbientOcclusionParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileSceneTextureUniformParameters, SceneTextures)

	SHADER_PARAMETER_STRUCT_INCLUDE(FHZBParameters, HZBParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FMobileSSAOShaderParameters, SSAOParameters)

	SHADER_PARAMETER_SAMPLER(SamplerState, SSAO_Sampler)

	SHADER_PARAMETER(FVector2f, SSAO_DownsampledAOInverseSize)
	SHADER_PARAMETER(FVector2f, SSAO_SvPositionScaleBias)

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RandomNormalTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, RandomNormalTextureSampler)
END_SHADER_PARAMETER_STRUCT();

class FMobileAmbientOcclusionPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMobileAmbientOcclusionPS);
	SHADER_USE_PARAMETER_STRUCT(FMobileAmbientOcclusionPS, FGlobalShader);

	class FShaderQualityDim : SHADER_PERMUTATION_INT("SHADER_QUALITY", 5);
	class FOutputDepth : SHADER_PERMUTATION_BOOL("OUTPUT_DEPTH");
	using FPermutationDomain = TShaderPermutationDomain<FShaderQualityDim, FOutputDepth>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SSAO"), 1);
		OutEnvironment.SetDefine(TEXT("FORCE_DEPTH_TEXTURE_READS"), 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobileAmbientOcclusionEnabled(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMobileAmbientOcclusionParameters, SharedParameters)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT();
};
IMPLEMENT_GLOBAL_SHADER(FMobileAmbientOcclusionPS, "/Engine/Private/PostProcessAmbientOcclusionMobile.usf", "MainPS", SF_Pixel);

class FMobileAmbientOcclusionUpsamplePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMobileAmbientOcclusionUpsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FMobileAmbientOcclusionUpsamplePS, FGlobalShader);

	class FUpsampleQualityDim : SHADER_PERMUTATION_INT("UPSAMPLE_QUALITY", 3);
	using FPermutationDomain = TShaderPermutationDomain <FUpsampleQualityDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AOTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, AOSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobileAmbientOcclusionEnabled(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("UPSAMPLE_PASS"), 1);
	
	}
};
IMPLEMENT_GLOBAL_SHADER(FMobileAmbientOcclusionUpsamplePS, "/Engine/Private/PostProcessAmbientOcclusionMobile.usf", "AmbientOcclusionUpsamplePS", SF_Pixel);

// --------------------------------------------------------------------------------------------------------------------
static FMobileSSAOCommonParameters GetMobileSSAOCommonParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef SceneDepthTexture,
	TRDGUniformBufferRef<FMobileSceneTextureUniformParameters> SceneTexturesUniformBufferRDG)
{
	FMobileSSAOCommonParameters CommonParameters;
	CommonParameters.SceneTexturesUniformBufferRDG = SceneTexturesUniformBufferRDG;
	CommonParameters.SceneTexturesViewport = FScreenPassTextureViewport(SceneDepthTexture, View.ViewRect);

	CommonParameters.HZBInput = FScreenPassTexture(View.HZB);
	CommonParameters.SceneDepth = FScreenPassTexture(SceneDepthTexture);
	return CommonParameters;
}

static FMobileSSAOShaderParameters GetMobileSSAOShaderParameters(
	const FViewInfo& View,
	const FScreenPassTextureViewport& InputViewport,
	const FScreenPassTextureViewport& OutputViewport,
	const FScreenPassTextureViewport& SceneViewport)
{
	const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;

	FIntPoint RandomizationSize = GSystemTextures.SSAORandomization->GetDesc().Extent * InputViewport.Extent / OutputViewport.Extent;
	FVector2D ViewportUVToRandomUV(InputViewport.Extent.X / (float)RandomizationSize.X, InputViewport.Extent.Y / (float)RandomizationSize.Y);

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleToFullRes = SceneViewport.Extent.X / InputViewport.Extent.X;

	FIntRect ViewRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes);

	float AORadiusInShader = Settings.AmbientOcclusionRadius;
	float ScaleRadiusInWorldSpace = 1.0f;

	if (!Settings.AmbientOcclusionRadiusInWS)
	{
		// radius is defined in view space in 400 units
		AORadiusInShader /= 400.0f;
		ScaleRadiusInWorldSpace = 0.0f;
	}

	// /4 is an adjustment for usage with multiple mips
	float f = FMath::Log2(float(ScaleToFullRes));
	float g = pow(Settings.AmbientOcclusionMipScale, f);
	AORadiusInShader *= pow(Settings.AmbientOcclusionMipScale, FMath::Log2(float(ScaleToFullRes))) / 4.0f;

	float Ratio = View.UnscaledViewRect.Width() / (float)View.UnscaledViewRect.Height();
		
	// Grab this and pass into shader so we can negate the fov influence of projection on the screen pos.
	float InvTanHalfFov = View.ViewMatrices.GetProjectionMatrix().M[0][0];

	float StaticFraction = FMath::Clamp(Settings.AmbientOcclusionStaticFraction, 0.0f, 1.0f);

	// clamp to prevent user error
	float FadeRadius = FMath::Max(1.0f, Settings.AmbientOcclusionFadeRadius);
	float InvFadeRadius = 1.0f / FadeRadius;

	FVector2D TemporalOffset(0.0f, 0.0f);

	if (View.State)
	{
		TemporalOffset = (View.State->GetCurrentTemporalAASampleIndex() % 8) * FVector2D(2.48f, 7.52f) / (float)RandomizationSize.X;
	}
	const float HzbStepMipLevelFactorValue = FMath::Clamp(FSSAOHelper::GetAmbientOcclusionStepMipLevelFactor(), 0.0f, 100.0f);
	const float InvAmbientOcclusionDistance = 1.0f / FMath::Max(Settings.AmbientOcclusionDistance_DEPRECATED, KINDA_SMALL_NUMBER);

	FMobileSSAOShaderParameters Result{};

	// /1000 to be able to define the value in that distance
	Result.ScreenSpaceAOParams[0] = FVector4f(Settings.AmbientOcclusionPower, Settings.AmbientOcclusionBias / 1000.0f, InvAmbientOcclusionDistance, Settings.AmbientOcclusionIntensity);
	Result.ScreenSpaceAOParams[1] = FVector4f(ViewportUVToRandomUV.X, ViewportUVToRandomUV.Y, AORadiusInShader, Ratio);
	Result.ScreenSpaceAOParams[2] = FVector4f(ScaleToFullRes, Settings.AmbientOcclusionMipThreshold / ScaleToFullRes, ScaleRadiusInWorldSpace, Settings.AmbientOcclusionMipBlend);
	Result.ScreenSpaceAOParams[3] = FVector4f(TemporalOffset.X, TemporalOffset.Y, StaticFraction, InvTanHalfFov);
	Result.ScreenSpaceAOParams[4] = FVector4f(InvFadeRadius, -(Settings.AmbientOcclusionFadeDistance - FadeRadius) * InvFadeRadius, HzbStepMipLevelFactorValue, Settings.AmbientOcclusionFadeDistance);

	Result.AOViewport = GetScreenPassTextureViewportParameters(OutputViewport);
	Result.AOSceneViewport = GetScreenPassTextureViewportParameters(SceneViewport);

	return Result;
}

static void AddMobileAmbientOcclusionPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FMobileSSAOCommonParameters& CommonParameters,
	bool bHalfResolution,
	bool bOutputDepth,
	FScreenPassRenderTarget Output)
{
	check(Output.IsValid());

	const FScreenPassTextureViewport InputViewport = CommonParameters.SceneTexturesViewport;
	const FScreenPassTextureViewport OutputViewport = GetDownscaledViewport(CommonParameters.SceneTexturesViewport, bHalfResolution ? 2 : 1);
	
	const bool bDepthBoundsTestEnabled =
		CVarMobileAmbientOcclusionDepthBoundsTest.GetValueOnRenderThread()
		&& !bHalfResolution
		&& GSupportsDepthBoundsTest
		&& CommonParameters.SceneDepth.IsValid()
		&& CommonParameters.SceneDepth.Texture->Desc.NumSamples == 1;

	float DepthFar = 0.0f;
	FDepthStencilBinding DepthStencilBinding(CommonParameters.SceneDepth.Texture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite);

	if (bDepthBoundsTestEnabled)
	{
		const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;
		const FMatrix& ProjectionMatrix = View.ViewMatrices.GetProjectionMatrix();
		const FVector4f Far = (FVector4f)ProjectionMatrix.TransformFVector4(FVector4(0, 0, Settings.AmbientOcclusionFadeDistance));
		DepthFar = FMath::Min(1.0f, Far.Z / Far.W);

		static_assert(bool(ERHIZBuffer::IsInverted), "Inverted depth buffer is assumed when setting depth bounds test for AO.");

		FRenderTargetParameters* ClearParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		ClearParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		ClearParameters->RenderTargets.DepthStencil = DepthStencilBinding;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DepthBounds ClearQuad(%s)", Output.Texture->Name),
			ClearParameters,
			ERDGPassFlags::Raster,
			[OutputViewport, DepthFar](FRHICommandListImmediate& RHICmdList)
			{
				// We must clear all pixels that won't be touched by AO shader.
				FClearQuadCallbacks Callbacks;
				Callbacks.PSOModifier = [](FGraphicsPipelineStateInitializer& PSOInitializer)
				{
					PSOInitializer.bDepthBounds = true;
				};
				Callbacks.PreClear = [DepthFar](FRHICommandList& InRHICmdList)
				{
					// This is done by rendering a clear quad over a depth range from AmbientOcclusionFadeDistance to far plane.
					InRHICmdList.SetDepthBounds(0, DepthFar);	// NOTE: Inverted depth
				};
				Callbacks.PostClear = [DepthFar](FRHICommandList& InRHICmdList)
				{
					// Set depth bounds test to cover everything from near plane to AmbientOcclusionFadeDistance and run AO pixel shader.
					InRHICmdList.SetDepthBounds(DepthFar, 1.0f);
				};

				RHICmdList.SetViewport(OutputViewport.Rect.Min.X, OutputViewport.Rect.Min.Y, 0.0f, OutputViewport.Rect.Max.X, OutputViewport.Rect.Max.Y, 1.0f);

				DrawClearQuad(RHICmdList, FLinearColor::White, Callbacks);
			});

		// Make sure the following pass doesn't clear or ignore the data
		Output.LoadAction = ERenderTargetLoadAction::ELoad;
	}

	FMobileAmbientOcclusionParameters SharedParameters;
	SharedParameters.View = View.ViewUniformBuffer;
	SharedParameters.SceneTextures = CommonParameters.SceneTexturesUniformBufferRDG;
	SharedParameters.HZBParameters = GetHZBParameters(View, CommonParameters.HZBInput, CommonParameters.SceneTexturesViewport.Extent, EAOTechnique::SSAO);
	SharedParameters.SSAOParameters = GetMobileSSAOShaderParameters(View, InputViewport, OutputViewport, CommonParameters.SceneTexturesViewport);
	SharedParameters.SSAO_Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	SharedParameters.SSAO_SvPositionScaleBias = bHalfResolution ? FVector2f(2, -0.5) : FVector2f(1, 0);
	SharedParameters.SSAO_DownsampledAOInverseSize = bHalfResolution ? FVector2f(1, 1) / FVector2f(OutputViewport.Extent) : FVector2f(1, 1);
	SharedParameters.RandomNormalTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.SSAORandomization, TEXT("SSAORandomization"));
	SharedParameters.RandomNormalTextureSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	FRDGEventName EventName(TEXT("AmbientOcclusionPS %dx%d"),
		OutputViewport.Rect.Width(), OutputViewport.Rect.Height());

	FMobileAmbientOcclusionPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMobileAmbientOcclusionPS::FParameters>();
	PassParameters->SharedParameters = MoveTemp(SharedParameters);
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	if (bDepthBoundsTestEnabled)
	{
		PassParameters->RenderTargets.DepthStencil = DepthStencilBinding;
	}

	const int32 MobileAmbientOcclusionQuality = FMath::Min(CVarMobileAmbientOcclusionQuality.GetValueOnRenderThread(), 3) - 1; 
	FMobileAmbientOcclusionPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FMobileAmbientOcclusionPS::FShaderQualityDim>(MobileAmbientOcclusionQuality);
	PermutationVector.Set<FMobileAmbientOcclusionPS::FOutputDepth>(bOutputDepth);

	TShaderMapRef<FMobileAmbientOcclusionPS> PixelShader(View.ShaderMap, PermutationVector);
	TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);

	check(PassParameters);
	ClearUnusedGraphResources(PixelShader, PassParameters);

	GraphBuilder.AddPass(
		MoveTemp(EventName),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, OutputViewport, InputViewport, VertexShader, PixelShader, PassParameters, bDepthBoundsTestEnabled, DepthFar](FRHICommandListImmediate& RHICmdList)
		{
			const FIntRect InputRect = InputViewport.Rect;
			const FIntPoint InputSize = InputViewport.Extent;
			const FIntRect OutputRect = OutputViewport.Rect;
			const FIntPoint OutputSize = OutputRect.Size();

			RHICmdList.SetViewport(OutputRect.Min.X, OutputRect.Min.Y, 0.0f, OutputRect.Max.X, OutputRect.Max.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.bDepthBounds = bDepthBoundsTestEnabled;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			if (bDepthBoundsTestEnabled)
			{
				RHICmdList.SetDepthBounds(DepthFar, 1.0f);
			}

			DrawPostProcessPass(
				RHICmdList,
				0, 0, OutputSize.X, OutputSize.Y,
				InputRect.Min.X, InputRect.Min.Y, InputRect.Width(), InputRect.Height(),
				OutputSize,
				InputSize,
				VertexShader,
				View.StereoViewIndex,
				false,
				EDRF_UseTriangleOptimization);

			if (bDepthBoundsTestEnabled)
			{
				RHICmdList.SetDepthBounds(0, 1.0f);
			}
		});	
}

static void AddMobileAmbientOcclusionUpsamplePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FMobileSSAOCommonParameters& CommonParameters,
	int32 UpsampleQuality,
	FRDGTextureRef SourceTexture,
	FScreenPassRenderTarget Output)
{
	const bool bDepthBoundsTestEnabled =
		CVarMobileAmbientOcclusionDepthBoundsTest.GetValueOnRenderThread()
		&& GSupportsDepthBoundsTest
		&& CommonParameters.SceneDepth.IsValid()
		&& CommonParameters.SceneDepth.Texture->Desc.NumSamples == 1;

	const FScreenPassTextureViewport OutputViewport = CommonParameters.SceneTexturesViewport;

	float DepthFar = 0.0f;
	FDepthStencilBinding DepthStencilBinding(CommonParameters.SceneDepth.Texture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite);

	if (bDepthBoundsTestEnabled)
	{
		const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;
		const FMatrix& ProjectionMatrix = View.ViewMatrices.GetProjectionMatrix();
		const FVector4f Far = (FVector4f)ProjectionMatrix.TransformFVector4(FVector4(0, 0, Settings.AmbientOcclusionFadeDistance));
		DepthFar = FMath::Min(1.0f, Far.Z / Far.W);

		static_assert(bool(ERHIZBuffer::IsInverted), "Inverted depth buffer is assumed when setting depth bounds test for AO.");

		FRenderTargetParameters* ClearParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		ClearParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		ClearParameters->RenderTargets.DepthStencil = DepthStencilBinding;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DepthBounds ClearQuad(%s)", Output.Texture->Name),
			ClearParameters,
			ERDGPassFlags::Raster,
			[OutputViewport, DepthFar](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.SetViewport(OutputViewport.Rect.Min.X, OutputViewport.Rect.Min.Y, 0.0f, OutputViewport.Rect.Max.X, OutputViewport.Rect.Max.Y, 1.0f);

				// We must clear all pixels that won't be touched by AO shader.
				FClearQuadCallbacks Callbacks;
				Callbacks.PSOModifier = [](FGraphicsPipelineStateInitializer& PSOInitializer)
				{
					PSOInitializer.bDepthBounds = true;
				};
				Callbacks.PreClear = [DepthFar](FRHICommandList& InRHICmdList)
				{
					// This is done by rendering a clear quad over a depth range from AmbientOcclusionFadeDistance to far plane.
					InRHICmdList.SetDepthBounds(0, DepthFar);	// NOTE: Inverted depth
				};
				Callbacks.PostClear = [DepthFar](FRHICommandList& InRHICmdList)
				{
					// Set depth bounds test to cover everything from near plane to AmbientOcclusionFadeDistance and run AO pixel shader.
					InRHICmdList.SetDepthBounds(DepthFar, 1.0f);
				};

				DrawClearQuad(RHICmdList, FLinearColor::White, Callbacks);
			});

		// Make sure the following pass doesn't clear or ignore the data
		Output.LoadAction = ERenderTargetLoadAction::ELoad;
	}

	FMobileAmbientOcclusionUpsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMobileAmbientOcclusionUpsamplePS::FParameters>();
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	if (bDepthBoundsTestEnabled)
	{
		PassParameters->RenderTargets.DepthStencil = DepthStencilBinding;
	}

	PassParameters->View = GetShaderBinding(View.ViewUniformBuffer);
	PassParameters->SceneTextures = CommonParameters.SceneTexturesUniformBufferRDG;
	PassParameters->AOTexture = SourceTexture;
	PassParameters->AOSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	typename FMobileAmbientOcclusionUpsamplePS::FPermutationDomain PermutationVector;
	PermutationVector.Set<typename FMobileAmbientOcclusionUpsamplePS::FUpsampleQualityDim>(UpsampleQuality);
	TShaderMapRef<FMobileAmbientOcclusionUpsamplePS> PixelShader(View.ShaderMap, PermutationVector);

	TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);

	ClearUnusedGraphResources(PixelShader, PassParameters);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("UpsamplePS Quality %d", UpsampleQuality),
		PassParameters,
		ERDGPassFlags::Raster,
		[PassParameters, &View, OutputViewport, VertexShader, PixelShader, bDepthBoundsTestEnabled, DepthFar](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(OutputViewport.Rect.Min.X, OutputViewport.Rect.Min.Y, 0.0f, OutputViewport.Rect.Max.X, OutputViewport.Rect.Max.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			FPixelShaderUtils::InitFullscreenPipelineState(RHICmdList, View.ShaderMap, PixelShader, GraphicsPSOInit);
			GraphicsPSOInit.bDepthBounds = bDepthBoundsTestEnabled;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			if (bDepthBoundsTestEnabled)
			{
				RHICmdList.SetDepthBounds(DepthFar, 1.0f);
			}

			FPixelShaderUtils::DrawFullscreenTriangle(RHICmdList);
			
			if (bDepthBoundsTestEnabled)
			{
				RHICmdList.SetDepthBounds(0.0f, 1.0f);
			}
		});
}

static void RenderSSAO(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneDepthTexture, FRDGTextureRef AmbientOcclusionTexture, const TArray<FViewInfo>& Views)
{
	RDG_EVENT_SCOPE(GraphBuilder, "MobileSSAO");
	RDG_GPU_STAT_SCOPE(GraphBuilder, MobileSSAO);

	const int32 HalfResolutionSetting = CVarMobileSSAOHalfResolution.GetValueOnRenderThread();
	const bool bHalfResolution = HalfResolutionSetting > 0;
	const int32 UpsampleQuality = FMath::Clamp(HalfResolutionSetting - 1, 0, 2);
	const bool bBilateralUpsample = UpsampleQuality > 0;

	FRDGTextureRef HalfResolutionTexture = nullptr;
	if (bHalfResolution)
	{
		// Bilateral requires 32bit format for AO + depth.
		const EPixelFormat Format = bBilateralUpsample ? PF_R8G8B8A8 : AmbientOcclusionTexture->Desc.Format;
		const FIntPoint Extent = GetDownscaledExtent(AmbientOcclusionTexture->Desc.Extent, 2);
		HalfResolutionTexture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(Extent, Format, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("HalfResolutionScreenSpaceAO"));
	}

	TRDGUniformBufferRef<FMobileSceneTextureUniformParameters> SceneTexturesUniformBufferRDG = CreateMobileSceneTextureUniformBuffer(
		GraphBuilder, GetViewFamilyInfo(Views).GetSceneTexturesChecked(), EMobileSceneTextureSetupMode::SceneDepth);

	for (FViewInfo const& View : Views)
	{
		FMobileSSAOCommonParameters Parameters = GetMobileSSAOCommonParameters(GraphBuilder, View, SceneDepthTexture, SceneTexturesUniformBufferRDG);
		FScreenPassRenderTarget FinalTarget = FScreenPassRenderTarget(AmbientOcclusionTexture, View.ViewRect, ERenderTargetLoadAction::ENoAction);

		if (HalfResolutionTexture != nullptr)
		{
			FScreenPassRenderTarget HalfResolutionTarget = FScreenPassRenderTarget(HalfResolutionTexture, View.ViewRect, ERenderTargetLoadAction::ENoAction);
			AddMobileAmbientOcclusionPass(GraphBuilder,	View, Parameters, bHalfResolution, bBilateralUpsample, HalfResolutionTarget);
			AddMobileAmbientOcclusionUpsamplePass(GraphBuilder, View, Parameters, UpsampleQuality, HalfResolutionTexture, FinalTarget);
		}
		else
		{
			AddMobileAmbientOcclusionPass(GraphBuilder, View, Parameters, bHalfResolution, bBilateralUpsample, FinalTarget);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
void FMobileSceneRenderer::RenderAmbientOcclusion(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneDepthTexture, FRDGTextureRef AmbientOcclusionTexture)
{
	const int32 Technique = CVarMobileAmbientOcclusionTechnique.GetValueOnRenderThread();
	switch (Technique)
	{
	case 0:
		RenderGTAO(GraphBuilder, SceneDepthTexture, AmbientOcclusionTexture, Views);
		break;
	case 1:
		RenderSSAO(GraphBuilder, SceneDepthTexture, AmbientOcclusionTexture, Views);
		break;
	}
}
