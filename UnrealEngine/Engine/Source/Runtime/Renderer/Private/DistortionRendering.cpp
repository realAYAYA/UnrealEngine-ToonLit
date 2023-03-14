// Copyright Epic Games, Inc. All Rights Reserved.

#include "DistortionRendering.h"
#include "RHIStaticStates.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MeshMaterialShader.h"
#include "DeferredShadingRenderer.h"
#include "TranslucentRendering.h"
#include "Materials/Material.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"
#include "ScreenPass.h"
#include "MeshPassProcessor.inl"
#include "Strata/Strata.h"
#include "ScreenRendering.h"

DECLARE_GPU_DRAWCALL_STAT(Distortion);

const uint8 kStencilMaskBit = STENCIL_SANDBOX_MASK;

static TAutoConsoleVariable<int32> CVarDisableDistortion(
	TEXT("r.DisableDistortion"),
	0,
	TEXT("Prevents distortion effects from rendering.  Saves a full-screen framebuffer's worth of memory."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarRefractionBlur(
	TEXT("r.Refraction.Blur"),
	1,
	TEXT("Prevent rough refraction from happening, i.e. blurring of the background."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarRefractionBlurScale(
	TEXT("r.Refraction.BlurScale"),
	0.2f,
	TEXT("Global scale the background blur amount after it is mapped form the surface back roughness/scattering amount."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarRefractionBlurCenterWeight(
	TEXT("r.Refraction.BlurCenterWeight"),
	0.0f,
	TEXT("The weight of the center sample. Value greater than 0 means the sharp image is more and more visible."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRefractionOffsetQuality(
	TEXT("r.Refraction.OffsetQuality"),
	0,
	TEXT("When enabled, the offset buffer is made float for higher quality. This is important to maintain the softness of the blurred scene buffer."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// Using a simple scale to map roughness to mip level.
// Later we could remove that and use depth+variance to have a better lob roughness to mip match.
static TAutoConsoleVariable<float> CVarRefractionRoughnessToMipLevelFactor(
	TEXT("r.Refraction.RoughnessToMipLevelFactor"),
	5.0f,
	TEXT("Factor to translate the roughness factor into a mip level."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FDistortionPassUniformParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER(FVector4f, DistortionParams)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FDistortionPassUniformParameters, "DistortionPass", SceneTextures);

int32 FSceneRenderer::GetRefractionQuality(const FSceneViewFamily& ViewFamily)
{
	static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RefractionQuality"));

	int32 Value = 0;

	if (ViewFamily.EngineShowFlags.Refraction)
	{
		Value = ICVar->GetValueOnRenderThread();
	}

	return Value;
}

void SetupDistortionParams(FVector4f& DistortionParams, const FViewInfo& View)
{
	float Ratio = View.UnscaledViewRect.Width() / (float)View.UnscaledViewRect.Height();
	DistortionParams.X = View.ViewMatrices.GetProjectionMatrix().M[0][0];
	DistortionParams.Y = Ratio;
	DistortionParams.Z = (float)View.UnscaledViewRect.Width();
	DistortionParams.W = (float)View.UnscaledViewRect.Height();

	// When ISR is enabled we store two FOVs in the distortion parameters and compute the aspect ratio in the shader instead.
	if (View.IsInstancedStereoPass() || View.bIsMobileMultiViewEnabled)
	{
		const FSceneView* InstancedView = View.GetInstancedView();
		if (InstancedView)
		{
			DistortionParams.Y = InstancedView->ViewMatrices.GetProjectionMatrix().M[0][0];
		}
	}
}

TRDGUniformBufferRef<FDistortionPassUniformParameters> CreateDistortionPassUniformBuffer(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	auto* Parameters = GraphBuilder.AllocParameters<FDistortionPassUniformParameters>();
	SetupSceneTextureUniformParameters(GraphBuilder, View.GetSceneTexturesChecked(), View.FeatureLevel, ESceneTextureSetupMode::All, Parameters->SceneTextures);
	SetupDistortionParams(Parameters->DistortionParams, View);
	return GraphBuilder.CreateUniformBuffer(Parameters);
}

static bool GetUseRoughRefraction()
{
	return Strata::IsStrataEnabled() && CVarRefractionBlur.GetValueOnRenderThread() > 0;
}

class FDistortionScreenPS : public FGlobalShader
{
public:
	class FUseMSAADim : SHADER_PERMUTATION_BOOL("USE_MSAA");
	class FUseRoughRefractionDim : SHADER_PERMUTATION_BOOL("USE_ROUGH_REFRACTION");
	using FPermutationDomain = TShaderPermutationDomain<FUseMSAADim, FUseRoughRefractionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DMS<float4>, RoughnessScatterMSAATexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DMS<float4>, DistortionMSAATexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DMS<float4>, SceneColorMSAATexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RoughnessScatterTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DistortionTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, RoughnessScatterSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, DistortionTextureSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorTextureSampler)
		SHADER_PARAMETER(float, RefractionRoughnessToMipLevelFactor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		return !PermutationVector.Get<FUseMSAADim>() || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FDistortionScreenPS() = default;
	FDistortionScreenPS(const FGlobalShaderType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

/** A pixel shader for rendering the full screen refraction pass */
class FDistortionApplyScreenPS : public FDistortionScreenPS
{
public:
	DECLARE_GLOBAL_SHADER(FDistortionApplyScreenPS);
	SHADER_USE_PARAMETER_STRUCT(FDistortionApplyScreenPS, FDistortionScreenPS);
};

IMPLEMENT_GLOBAL_SHADER(FDistortionApplyScreenPS, "/Engine/Private/DistortApplyScreenPS.usf", "Main", SF_Pixel);

/** A pixel shader that applies the distorted image to the scene */
class FDistortionMergeScreenPS : public FDistortionScreenPS
{
public:
	DECLARE_GLOBAL_SHADER(FDistortionMergeScreenPS);
	SHADER_USE_PARAMETER_STRUCT(FDistortionMergeScreenPS, FDistortionScreenPS);
};

IMPLEMENT_GLOBAL_SHADER(FDistortionMergeScreenPS, "/Engine/Private/DistortApplyScreenPS.usf", "Merge", SF_Pixel);

class FDistortionMeshVS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FDistortionMeshVS,MeshMaterial);

	FDistortionMeshVS() = default;

	FDistortionMeshVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsTranslucentBlendMode(Parameters.MaterialParameters.BlendMode) && Parameters.MaterialParameters.bIsDistorted;
	}
};

class FDistortionMeshPS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FDistortionMeshPS,MeshMaterial);

	FDistortionMeshPS() = default;

	FDistortionMeshPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsTranslucentBlendMode(Parameters.MaterialParameters.BlendMode) && Parameters.MaterialParameters.bIsDistorted;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		if (IsMobilePlatform(Parameters.Platform))
		{
			// use same path for scene textures as post-process material
			OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_MOBILE"), 1);
		}

		// Skip the material clip if depth test should not be done
		OutEnvironment.SetDefine(TEXT("MATERIAL_SHOULD_DISABLE_DEPTH_TEST"), Parameters.MaterialParameters.bShouldDisableDepthTest ? 1 : 0);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FDistortionMeshVS, TEXT("/Engine/Private/DistortAccumulateVS.usf"), TEXT("Main"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FDistortionMeshPS,TEXT("/Engine/Private/DistortAccumulatePS.usf"),TEXT("Main"),SF_Pixel);

bool FDeferredShadingSceneRenderer::ShouldRenderDistortion() const
{
	static const auto DisableDistortionCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DisableDistortion"));
	const bool bAllowDistortion = DisableDistortionCVar->GetValueOnAnyThread() != 1;

	if (GetRefractionQuality(ViewFamily) <= 0 || !bAllowDistortion)
	{
		return false;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];

		if (View.bHasDistortionPrimitives && View.ShouldRenderView() && View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].HasAnyDraw())
		{
			return true;
		}
	}
	return false;
}

BEGIN_SHADER_PARAMETER_STRUCT(FDistortionPassParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDistortionPassUniformParameters, Pass)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

//////////////////////////////////////////////////////////////////////////

class FCopySceneColorTexturePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCopySceneColorTexturePS);
	SHADER_USE_PARAMETER_STRUCT(FCopySceneColorTexturePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COPYSCENECOLORTEXTUREPS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCopySceneColorTexturePS, "/Engine/Private/DistortFiltering.usf", "CopySceneColorTexturePS", SF_Pixel);

static void AddCopySceneColorPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef SceneColorTexture, FRDGTextureRef SceneColorCopyTexture)
{
	const FScreenPassTextureViewport Viewport(SceneColorCopyTexture, View.ViewRect);

	TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FCopySceneColorTexturePS> PixelShader(View.ShaderMap);

	auto* PassParameters = GraphBuilder.AllocParameters<FCopySceneColorTexturePS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->SceneColorTexture = SceneColorTexture;
	PassParameters->SceneColorSampler = TStaticSamplerState<SF_Point>::GetRHI();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorCopyTexture, ERenderTargetLoadAction::ENoAction);

	// The scene color is copied into from multi-view-rect layout into a single-rect layout.
	FIntRect ViewRect;
	ViewRect.Min = FIntPoint(0, 0);
	ViewRect.Max = FIntPoint(View.ViewRect.Width(), View.ViewRect.Height());

	const FScreenPassTextureViewport InputViewport(SceneColorTexture, ViewRect);
	const FScreenPassTextureViewport OutputViewport(SceneColorCopyTexture);

	AddDrawScreenPass(GraphBuilder, {}, View, InputViewport, OutputViewport, VertexShader, PixelShader, PassParameters);
}

//////////////////////////////////////////////////////////////////////////

class FDownsampleSceneColorCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDownsampleSceneColorCS);
	SHADER_USE_PARAMETER_STRUCT(FDownsampleSceneColorCS, FGlobalShader);

	static const uint32 ThreadGroupSize = 8;

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SrcMipIndex)
		SHADER_PARAMETER(FIntPoint, SrcMipResolution)
		SHADER_PARAMETER(FIntPoint, DstMipResolution)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTextureMipColor)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5; }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLECOLORCS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FDownsampleSceneColorCS, "/Engine/Private/DistortFiltering.usf", "DownsampleColorCS", SF_Compute);

static void AddDownsampleSceneColorPass(FRDGBuilder& GraphBuilder, FDownsampleSceneColorCS::FParameters* PassParameters, const FViewInfo& View)
{
	FIntVector NumGroups = FIntVector::DivideAndRoundUp(
		FIntVector(PassParameters->DstMipResolution.X, PassParameters->DstMipResolution.Y, 1), 
		FIntVector(FDownsampleSceneColorCS::ThreadGroupSize, FDownsampleSceneColorCS::ThreadGroupSize, 1));

	FDownsampleSceneColorCS::FPermutationDomain PermutationVector;
	TShaderMapRef<FDownsampleSceneColorCS> ComputeShader(View.ShaderMap, PermutationVector);

	// Dispatch with GenerateMips: reading from a slice through SRV and writing into lower mip through UAV.
	ClearUnusedGraphResources(ComputeShader, PassParameters);
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("DistoMipGen"), ComputeShader, PassParameters, NumGroups);
}

//////////////////////////////////////////////////////////////////////////

// STARTA_TODO make common with the function in PostProcessWeightedSampleSum.cpp
class FGaussianFiltering
{
public:
	// Evaluates an unnormalized normal distribution PDF around 0 at given X with Variance.
	static float NormalDistributionUnscaled(float X, float Sigma, float CrossCenterWeight)
	{
		const float DX = FMath::Abs(X);

		const float ClampedOneMinusDX = FMath::Max(0.0f, 1.0f - DX);

		// Tweak the gaussian shape e.g. "r.Bloom.Cross 3.5"
		if (CrossCenterWeight > 1.0f)
		{
			return FMath::Pow(ClampedOneMinusDX, CrossCenterWeight);
		}
		else
		{
			// Constant is tweaked give a similar look to UE before we fixed the scale bug (Some content tweaking might be needed).
			// The value defines how much of the Gaussian clipped by the sample window.
			// r.Filter.SizeScale allows to tweak that for performance/quality.
			const float LegacyCompatibilityConstant = -16.7f;

			const float Gaussian = FMath::Exp(LegacyCompatibilityConstant * FMath::Square(DX / Sigma));

			return FMath::Lerp(Gaussian, ClampedOneMinusDX, CrossCenterWeight);
		}
	}

	static float GetClampedKernelRadius(uint32 SampleCountMax, float KernelRadius)
	{
		return FMath::Clamp<float>(KernelRadius, DELTA, SampleCountMax - 1);
	}

	static int GetIntegerKernelRadius(uint32 SampleCountMax, float KernelRadius)
	{
		// Smallest radius will be 1.
		return FMath::Min<int32>(FMath::CeilToInt(GetClampedKernelRadius(SampleCountMax, KernelRadius)), SampleCountMax - 1);
	}

	static uint32 Compute1DGaussianFilterKernel(FVector2D* OutOffsetAndWeight, uint32 SampleCountMax, float KernelRadius, float CrossCenterWeight, float FilterSizeScale)
	{
		const float ClampedKernelRadius = GetClampedKernelRadius(SampleCountMax, KernelRadius);

		const int32 IntegerKernelRadius = GetIntegerKernelRadius(SampleCountMax, KernelRadius * FilterSizeScale);

		uint32 SampleCount = 0;
		float WeightSum = 0.0f;

		for (int32 SampleIndex = -IntegerKernelRadius; SampleIndex <= IntegerKernelRadius; SampleIndex += 2)
		{
			float Weight0 = NormalDistributionUnscaled(SampleIndex, ClampedKernelRadius, CrossCenterWeight);
			float Weight1 = 0.0f;

			// We use the bilinear filter optimization for gaussian blur. However, we don't want to bias the
			// last sample off the edge of the filter kernel, so the very last tap just is on the pixel center.
			if (SampleIndex != IntegerKernelRadius)
			{
				Weight1 = NormalDistributionUnscaled(SampleIndex + 1, ClampedKernelRadius, CrossCenterWeight);
			}

			const float TotalWeight = Weight0 + Weight1;
			OutOffsetAndWeight[SampleCount].X = SampleIndex + (Weight1 / TotalWeight);
			OutOffsetAndWeight[SampleCount].Y = TotalWeight;
			WeightSum += TotalWeight;
			SampleCount++;
		}

		// Normalize blur weights.
		const float WeightSumInverse = 1.0f / WeightSum;
		for (uint32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
		{
			OutOffsetAndWeight[SampleIndex].Y *= WeightSumInverse;
		}

		return SampleCount;
	}
};

// If this is update, please update DistortFiltering.usf
#define MAX_FILTER_SAMPLE_COUNT	128

class FFilterSceneColorCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFilterSceneColorCS);
	SHADER_USE_PARAMETER_STRUCT(FFilterSceneColorCS, FGlobalShader);

	static const uint32 ThreadGroupSize = 8;

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SampleCount)
		SHADER_PARAMETER(uint32, SrcMipIndex)
		SHADER_PARAMETER(FIntPoint, MipResolution)
		SHADER_PARAMETER(FVector4f, BlurDirection)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTextureMipColor)
		SHADER_PARAMETER_ARRAY(FVector4f, SampleOffsetsWeights, [MAX_FILTER_SAMPLE_COUNT])
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5; }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("FILTERCOLORCS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FFilterSceneColorCS, "/Engine/Private/DistortFiltering.usf", "FilterColorCS", SF_Compute);

static void AddFilterSceneColorPass(FRDGBuilder& GraphBuilder, FFilterSceneColorCS::FParameters* PassParameters, const FViewInfo& View)
{
	FIntVector NumGroups = FIntVector::DivideAndRoundUp(
		FIntVector(PassParameters->MipResolution.X, PassParameters->MipResolution.Y, 1),
		FIntVector(FFilterSceneColorCS::ThreadGroupSize, FFilterSceneColorCS::ThreadGroupSize, 1));

	FFilterSceneColorCS::FPermutationDomain PermutationVector;
	TShaderMapRef<FFilterSceneColorCS> ComputeShader(View.ShaderMap, PermutationVector);

	// Dispatch with GenerateMips: reading from a slice through SRV and writing into lower mip through UAV.
	ClearUnusedGraphResources(ComputeShader, PassParameters);
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("FilterMipGen"), ComputeShader, PassParameters, NumGroups);
}

//////////////////////////////////////////////////////////////////////////

void FDeferredShadingSceneRenderer::RenderDistortion(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneColorTexture, FRDGTextureRef SceneDepthTexture)
{
	check(SceneDepthTexture);
	check(SceneColorTexture);

	if (!ShouldRenderDistortion())
	{
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion);
	RDG_EVENT_SCOPE(GraphBuilder, "Distortion");
	RDG_GPU_STAT_SCOPE(GraphBuilder, Distortion);

	const FDepthStencilBinding StencilReadBinding(SceneDepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilRead);
	FDepthStencilBinding StencilWriteBinding(SceneDepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite);

	FRDGTextureRef DistortionTexture = nullptr;
	FRDGTextureRef RoughnessScatterTexture = nullptr;
	FRDGTextureRef TempSceneColorMipchainTexture = nullptr;
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(FeatureLevel);

	// Create a texture of the scene color with MIP levels, unfiltered
	// NOTE: we cannot use mips on the scene color render target it self because multiple views are continuous in there and they would leak color one onto another.
	TArray<FRDGTextureRef> ViewsSceneColorMipchain;
	ViewsSceneColorMipchain.SetNum(Views.Num());
	const bool bUseRoughRefraction = GetUseRoughRefraction();
	if(bUseRoughRefraction)
	{
		for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "Rough Refraction View%d", ViewIndex);

			const FViewInfo& View = Views[ViewIndex];
			const FIntPoint SceneColorMip0Resolution = View.ViewRect.Size();

			// We do not use max and do not add 1 to the result, this to not go down to the lowest mip level as this is not required.
			const int32 MipCount = FMath::Max((int32)1, (int32)FMath::CeilLogTwo(FMath::Min(SceneColorMip0Resolution.X, SceneColorMip0Resolution.Y)));

			FRDGTextureDesc SceneColorMipchainDesc = FRDGTextureDesc::Create2D(SceneColorMip0Resolution, PF_FloatR11G11B10, FClearValueBinding::None,
				TexCreate_TargetArraySlicesIndependently | TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable, MipCount);

			FRDGTextureRef SceneColorMipchainTexture = GraphBuilder.CreateTexture(SceneColorMipchainDesc, TEXT("SceneColorMipchain"));
			ViewsSceneColorMipchain[ViewIndex] = SceneColorMipchainTexture;

			if (!TempSceneColorMipchainTexture || TempSceneColorMipchainTexture->Desc.Extent != SceneColorMipchainTexture->Desc.Extent)
			{
				// Only allocate a new temporary if it needs to have another resolution.
				TempSceneColorMipchainTexture = GraphBuilder.CreateTexture(SceneColorMipchainDesc, TEXT("TempSceneColorMipchain"));
			}

			// Copy scene color into the first mip level

			{
				RDG_EVENT_SCOPE(GraphBuilder, "CopySceneColor");
				AddCopySceneColorPass(GraphBuilder, View, SceneColorTexture, SceneColorMipchainTexture);
			}

			// Now render the mip chain
			// STRATA_TODO we could optimize that pass by doing one pass with a tile of 16x16 writing out the 8x8, 4x4, 2x2 and 1x1 down sampled output

			{
				RDG_EVENT_SCOPE(GraphBuilder, "SceneColorMipChain");

				FIntPoint SrcMipResolution = SceneColorMip0Resolution;
				FIntPoint DstMipResolution = SceneColorMip0Resolution / 2;
				for (int32 DstMipIndex = 1; DstMipIndex < MipCount; DstMipIndex++)
				{
					FDownsampleSceneColorCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsampleSceneColorCS::FParameters>();
					PassParameters->SrcMipIndex = DstMipIndex - 1;
					PassParameters->SrcMipResolution = SrcMipResolution;
					PassParameters->DstMipResolution = DstMipResolution;
					PassParameters->SourceSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

					PassParameters->SourceTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SceneColorMipchainTexture, DstMipIndex - 1));
					PassParameters->OutTextureMipColor = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorMipchainTexture, DstMipIndex));

					AddDownsampleSceneColorPass(GraphBuilder, PassParameters, View);

					SrcMipResolution = DstMipResolution;
					DstMipResolution = DstMipResolution / 2;
				}
			}

			// Now the horizontal blur
			// STATA_TODO: check that compute overlap is working as all horizontal filtering steps can happen in parallel

			const float KernelRadius = 16.0f; // The maximum sample count we will run. This is scaled down by FilterSizeScale.
			const float FilterSizeScale = CVarRefractionBlurScale.GetValueOnRenderThread();
			const float CrossCenterWeight = CVarRefractionBlurCenterWeight.GetValueOnRenderThread(); // This must be 0 for the sharp image to not affect the output, the blurring dominates.

			{
				RDG_EVENT_SCOPE(GraphBuilder, "SceneColorMipHBlur");

				FIntPoint MipResolution = SceneColorMip0Resolution / 2;
				for (int32 MipIndex = 1; MipIndex < MipCount; MipIndex++)
				{
					FVector2D OffsetAndWeight[MAX_FILTER_SAMPLE_COUNT];
					uint32 SampleCount = FGaussianFiltering::Compute1DGaussianFilterKernel(OffsetAndWeight, MAX_FILTER_SAMPLE_COUNT, KernelRadius, CrossCenterWeight, FilterSizeScale);
					const FVector2D InverseFilterTextureExtent(1.0f / static_cast<float>(MipResolution.X), 1.0f / static_cast<float>(MipResolution.Y));

					FFilterSceneColorCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilterSceneColorCS::FParameters>();
					PassParameters->SrcMipIndex = MipIndex;
					PassParameters->MipResolution = MipResolution;
					PassParameters->SourceSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

					PassParameters->SourceTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SceneColorMipchainTexture, MipIndex));
					PassParameters->OutTextureMipColor = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TempSceneColorMipchainTexture, MipIndex));

					PassParameters->SampleCount = SampleCount;
					PassParameters->BlurDirection = FVector4f(1.0f, 0.0f, 0.0f, 0.0f);
					for (uint32 i = 0; i < SampleCount; ++i)
					{
						PassParameters->SampleOffsetsWeights[i] = FVector4f(InverseFilterTextureExtent.X * OffsetAndWeight[i].X, OffsetAndWeight[i].Y);
					}

					AddFilterSceneColorPass(GraphBuilder, PassParameters, View);

					MipResolution = MipResolution / 2;
				}
			}

			// Now the vertical blur
			// STATA_TODO: check that compute overlap is working as all vertical filtering steps can happen in parallel

			{
				RDG_EVENT_SCOPE(GraphBuilder, "SceneColorMipVBlur");

				FIntPoint MipResolution = SceneColorMip0Resolution / 2;
				for (int32 MipIndex = 1; MipIndex < MipCount; MipIndex++)
				{
					FVector2D OffsetAndWeight[MAX_FILTER_SAMPLE_COUNT];
					uint32 SampleCount = FGaussianFiltering::Compute1DGaussianFilterKernel(OffsetAndWeight, MAX_FILTER_SAMPLE_COUNT, KernelRadius, CrossCenterWeight, FilterSizeScale);
					const FVector2D InverseFilterTextureExtent(1.0f / static_cast<float>(MipResolution.X), 1.0f / static_cast<float>(MipResolution.Y));

					FFilterSceneColorCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilterSceneColorCS::FParameters>();
					PassParameters->SrcMipIndex = MipIndex;
					PassParameters->MipResolution = MipResolution;
					PassParameters->SourceSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

					PassParameters->SourceTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(TempSceneColorMipchainTexture, MipIndex));
					PassParameters->OutTextureMipColor = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorMipchainTexture, MipIndex));

					PassParameters->SampleCount = SampleCount;
					PassParameters->BlurDirection = FVector4f(0.0f, 1.0f, 0.0f, 0.0f);
					for (uint32 i = 0; i < SampleCount; ++i)
					{
						PassParameters->SampleOffsetsWeights[i] = FVector4f(InverseFilterTextureExtent.Y * OffsetAndWeight[i].X, OffsetAndWeight[i].Y);
					}

					AddFilterSceneColorPass(GraphBuilder, PassParameters, View);

					MipResolution = MipResolution / 2;
				}
			}
		}
	}


	// Use stencil mask to optimize cases with lower screen coverage.
	// Note: This adds an extra pass which is actually slower as distortion tends towards full-screen.
	//       It could be worth testing object screen bounds then reverting to a target flip and single pass.

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Accumulate);
		RDG_EVENT_SCOPE(GraphBuilder, "Accumulate");

		// Use RGBA8 light target for accumulating distortion offsets.
		// R = positive X offset
		// G = positive Y offset
		// B = negative X offset
		// A = negative Y offset

		DistortionTexture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(
				SceneDepthTexture->Desc.Extent,
				CVarRefractionOffsetQuality.GetValueOnRenderThread() > 0 ? PF_FloatRGBA : PF_B8G8R8A8,
				FClearValueBinding::Transparent,
				GFastVRamConfig.Distortion | TexCreate_RenderTargetable | TexCreate_ShaderResource,
				1,
				SceneDepthTexture->Desc.NumSamples),
			TEXT("Distortion"));

		if (bUseRoughRefraction)
		{
			// This is the texture containing information about the surface back scattering process
			RoughnessScatterTexture = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					SceneDepthTexture->Desc.Extent,
					PF_R16F,
					FClearValueBinding::Transparent,
					GFastVRamConfig.Distortion | TexCreate_RenderTargetable | TexCreate_ShaderResource,
					1,
					SceneDepthTexture->Desc.NumSamples),
				TEXT("DistortionRoughnessScatter"));
		}

		ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::EClear;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			const ETranslucencyView TranslucencyView = GetTranslucencyView(View);

			if (!View.ShouldRenderView() && !EnumHasAnyFlags(TranslucencyView, ETranslucencyView::RayTracing))
			{
				continue;
			}

			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);
			View.BeginRenderView();

			auto* PassParameters = GraphBuilder.AllocParameters<FDistortionPassParameters>();
			PassParameters->View = View.GetShaderParameters();
			PassParameters->Pass = CreateDistortionPassUniformBuffer(GraphBuilder, View);
			PassParameters->RenderTargets[0] = FRenderTargetBinding(DistortionTexture, LoadAction);
			if (bUseRoughRefraction)
			{
				PassParameters->RenderTargets[1] = FRenderTargetBinding(RoughnessScatterTexture, LoadAction);
			}
			PassParameters->RenderTargets.DepthStencil = StencilWriteBinding;

			View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

			GraphBuilder.AddPass(
				{},
				PassParameters,
				ERDGPassFlags::Raster,
				[this, &View, PassParameters](FRHICommandList& RHICmdList)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRender_RenderDistortion_Accumulate_Meshes);
				SetStereoViewport(RHICmdList, View);
				View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
			});

			LoadAction = ERenderTargetLoadAction::ELoad;
		}
	}

	FRDGTextureDesc DistortedSceneColorDesc = SceneColorTexture->Desc;
	EnumRemoveFlags(DistortedSceneColorDesc.Flags, TexCreate_FastVRAM);

	FRDGTextureRef DistortionSceneColorTexture = GraphBuilder.CreateTexture(DistortedSceneColorDesc, TEXT("DistortedSceneColor"));

	FDistortionScreenPS::FParameters CommonParameters;
	CommonParameters.DistortionMSAATexture = DistortionTexture;
	CommonParameters.DistortionTexture = DistortionTexture;
	if (bUseRoughRefraction)
	{
		CommonParameters.RoughnessScatterMSAATexture = RoughnessScatterTexture;
		CommonParameters.RoughnessScatterTexture = RoughnessScatterTexture;
	}
	CommonParameters.SceneColorTextureSampler = bUseRoughRefraction ? TStaticSamplerState<SF_Trilinear>::GetRHI() : TStaticSamplerState<>::GetRHI();
	CommonParameters.DistortionTextureSampler = TStaticSamplerState<>::GetRHI();
	CommonParameters.RoughnessScatterSampler = TStaticSamplerState<>::GetRHI();
	CommonParameters.RefractionRoughnessToMipLevelFactor = FMath::Max(0.0f, CVarRefractionRoughnessToMipLevelFactor.GetValueOnRenderThread());

	FDistortionScreenPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FDistortionScreenPS::FUseMSAADim>(SceneColorTexture->Desc.NumSamples > 1);
	PermutationVector.Set<FDistortionScreenPS::FUseRoughRefractionDim>(bUseRoughRefraction);

	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);
	TShaderMapRef<FDistortionApplyScreenPS> ApplyPixelShader(ShaderMap, PermutationVector);
	TShaderMapRef<FDistortionMergeScreenPS> MergePixelShader(ShaderMap, PermutationVector);

	FScreenPassPipelineState PipelineState(VertexShader, {});
	FScreenPassTextureViewport Viewport(SceneColorTexture);

	// Apply distortion and store off-screen.
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Apply);
		RDG_EVENT_SCOPE(GraphBuilder, "Apply");
		CommonParameters.SceneColorMSAATexture = SceneColorTexture;
		CommonParameters.SceneColorTexture = SceneColorTexture;
		CommonParameters.RenderTargets.DepthStencil = StencilReadBinding;
		PipelineState.PixelShader = ApplyPixelShader;

		// Test against stencil mask but don't clear.
		PipelineState.DepthStencilState = TStaticDepthStencilState<
			false, CF_Always,
			true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			kStencilMaskBit, kStencilMaskBit>::GetRHI();
		PipelineState.StencilRef = kStencilMaskBit;

		ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::ENoAction;

		for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

			auto* PassParameters = GraphBuilder.AllocParameters<FDistortionScreenPS::FParameters>();
			*PassParameters = CommonParameters;
			if (bUseRoughRefraction)
			{
				PassParameters->SceneColorMSAATexture = ViewsSceneColorMipchain[ViewIndex];
				PassParameters->SceneColorTexture = ViewsSceneColorMipchain[ViewIndex];
			}
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(DistortionSceneColorTexture, LoadAction);

			Viewport.Rect = View.ViewRect;

			ClearUnusedGraphResources(ApplyPixelShader, PassParameters);
			AddDrawScreenPass(GraphBuilder, {}, View, Viewport, Viewport, PipelineState, PassParameters,
				[ApplyPixelShader, PassParameters](FRHICommandList& RHICmdList)
			{
				SetShaderParameters(RHICmdList, ApplyPixelShader, ApplyPixelShader.GetPixelShader(), *PassParameters);
			});

			LoadAction = ERenderTargetLoadAction::ELoad;
		}
	}

	// Merge distortion back to scene color.
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Merge);
		RDG_EVENT_SCOPE(GraphBuilder, "Merge");
		CommonParameters.SceneColorMSAATexture = DistortionSceneColorTexture;
		CommonParameters.SceneColorTexture = DistortionSceneColorTexture;
		CommonParameters.RenderTargets.DepthStencil = StencilWriteBinding;
		PipelineState.PixelShader = MergePixelShader;

		// Test against stencil mask and clear it.
		PipelineState.DepthStencilState = TStaticDepthStencilState<
			false, CF_Always,
			true, CF_Equal, SO_Keep, SO_Keep, SO_Zero,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			kStencilMaskBit, kStencilMaskBit>::GetRHI();
		PipelineState.StencilRef = kStencilMaskBit;

		for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

			auto* PassParameters = GraphBuilder.AllocParameters<FDistortionScreenPS::FParameters>();
			*PassParameters = CommonParameters;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);

			Viewport.Rect = View.ViewRect;

			ClearUnusedGraphResources(MergePixelShader, PassParameters);
			AddDrawScreenPass(GraphBuilder, {}, View, Viewport, Viewport, PipelineState, PassParameters,
				[MergePixelShader, PassParameters](FRHICommandList& RHICmdList)
			{
				SetShaderParameters(RHICmdList, MergePixelShader, MergePixelShader.GetPixelShader(), *PassParameters);
			});
		}
	}
}

bool GetDistortionPassShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	TShaderRef<FDistortionMeshVS>& VertexShader,
	TShaderRef<FDistortionMeshPS>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FDistortionMeshVS>();
	ShaderTypes.AddShaderType<FDistortionMeshPS>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

void FDistortionMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material)
			{
				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			}

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

static bool ShouldDraw(const FMaterial& Material)
{
	const EBlendMode BlendMode = Material.GetBlendMode();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	return (bIsTranslucent
		&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain())
		&& Material.IsDistorted());
}

void FDistortionMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FVertexFactoryType* VertexFactoryType, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	if (!ShouldDraw(Material))
	{
		return;
	}

	TMeshProcessorShaders<
		FDistortionMeshVS,
		FDistortionMeshPS> DistortionPassShaders;
	if (!GetDistortionPassShaders(
		Material,
		VertexFactoryType,
		FeatureLevel,
		DistortionPassShaders.VertexShader,
		DistortionPassShaders.PixelShader))
	{
		return;
	}

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);
		
	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	RenderTargetsInfo.NumSamples = 1;
	AddRenderTargetInfo(CVarRefractionOffsetQuality.GetValueOnAnyThread() > 0 ? PF_FloatRGBA : PF_B8G8R8A8, GFastVRamConfig.Distortion | TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
	if (GetUseRoughRefraction())
	{
		AddRenderTargetInfo(PF_R16F, GFastVRamConfig.Distortion | TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
	}
	ETextureCreateFlags DepthStencilCreateFlags = SceneTexturesConfig.DepthCreateFlags;
	SetupDepthStencilInfo(PF_DepthStencil, DepthStencilCreateFlags, ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite, RenderTargetsInfo);

	AddGraphicsPipelineStateInitializer(
		VertexFactoryType,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		DistortionPassShaders,
		MeshFillMode,
		MeshCullMode,
		(EPrimitiveType)PreCacheParams.PrimitiveType,
		EMeshPassFeatures::Default,
		PSOInitializers);
}

bool FDistortionMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	bool bResult = true;
	if (ShouldDraw(Material)
		&& (!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass()))
	{
		const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

		bResult = Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, MaterialRenderProxy, Material, MeshFillMode, MeshCullMode);
	}

	return bResult;
}

bool FDistortionMeshProcessor::Process(
	const FMeshBatch& MeshBatch, 
	uint64 BatchElementMask, 
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FDistortionMeshVS,
		FDistortionMeshPS> DistortionPassShaders;

	if (!GetDistortionPassShaders(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		DistortionPassShaders.VertexShader,
		DistortionPassShaders.PixelShader))
	{
		return false;
	}

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(DistortionPassShaders.VertexShader, DistortionPassShaders.PixelShader);

	const bool bDisableDepthTest = MaterialResource.ShouldDisableDepthTest();

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		bDisableDepthTest ? PassDrawRenderStateNoDepthTest : PassDrawRenderState,
		DistortionPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

FDistortionMeshProcessor::FDistortionMeshProcessor(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	const FMeshPassProcessorRenderState& InDistortionPassStateNoDepthTest,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::Distortion, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
	, PassDrawRenderStateNoDepthTest(InDistortionPassStateNoDepthTest)
{}

FMeshPassProcessor* CreateDistortionPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState DistortionPassState;
	
	// test against depth and write stencil mask
	DistortionPassState.SetDepthStencilState(TStaticDepthStencilState<
		false, CF_DepthNearOrEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		kStencilMaskBit, kStencilMaskBit>::GetRHI());

	DistortionPassState.SetStencilRef(kStencilMaskBit);

	if (GetUseRoughRefraction())
	{
		DistortionPassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
															CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
	}
	else
	{
		// additive blending of offsets (or complexity if the shader complexity viewmode is enabled)
		DistortionPassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
	}

	FMeshPassProcessorRenderState DistortionPassStateNoDepthTest = DistortionPassState;
	DistortionPassStateNoDepthTest.SetDepthStencilState(TStaticDepthStencilState<
		false, CF_Always,
		true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		kStencilMaskBit, kStencilMaskBit>::GetRHI());
	DistortionPassStateNoDepthTest.SetStencilRef(kStencilMaskBit);

	return new FDistortionMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, DistortionPassState, DistortionPassStateNoDepthTest, InDrawListContext);
}

FMeshPassProcessor* CreateMobileDistortionPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState DistortionPassState;

	// We don't have depth, render all pixels, pixel shader will sample SceneDepth from SceneColor.A and discard if occluded
	DistortionPassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	if (GetUseRoughRefraction())
	{
		DistortionPassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
															CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
	}
	else
	{
		// additive blending of offsets
		DistortionPassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
	}

	return new FDistortionMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, DistortionPassState, DistortionPassState, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(DistortionPass, CreateDistortionPassProcessor, EShadingPath::Deferred, EMeshPass::Distortion, EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileDistortionPass(&CreateMobileDistortionPassProcessor, EShadingPath::Mobile, EMeshPass::Distortion, EMeshPassFlags::MainView);