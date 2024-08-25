// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMaterial.cpp: Post processing Material implementation.
=============================================================================*/

#include "PostProcess/PostProcessMaterial.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RendererModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialDomain.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "RenderUtils.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRendering.h"
#include "ClearQuad.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "PipelineStateCache.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMobile.h"
#include "BufferVisualizationData.h"
#include "SceneTextureParameters.h"
#include "SystemTextures.h"
#include "Substrate/Substrate.h"
#include "SingleLayerWaterRendering.h"
#include "Engine/NeuralProfile.h"

namespace
{

TAutoConsoleVariable<int32> CVarPostProcessAllowStencilTest(
	TEXT("r.PostProcessAllowStencilTest"),
	1,
	TEXT("Enables stencil testing in post process materials.\n")
	TEXT("0: disable stencil testing\n")
	TEXT("1: allow stencil testing\n")
	);

TAutoConsoleVariable<int32> CVarPostProcessAllowBlendModes(
	TEXT("r.PostProcessAllowBlendModes"),
	1,
	TEXT("Enables blend modes in post process materials.\n")
	TEXT("0: disable blend modes. Uses replace\n")
	TEXT("1: allow blend modes\n")
	);

TAutoConsoleVariable<int32> CVarPostProcessingDisableMaterials(
	TEXT("r.PostProcessing.DisableMaterials"),
	0,
	TEXT(" Allows to disable post process materials. \n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static bool IsPostProcessStencilTestAllowed()
{
	return CVarPostProcessAllowStencilTest.GetValueOnRenderThread() != 0;
}

enum class EMaterialCustomDepthPolicy : uint32
{
	// Custom depth is disabled.
	Disabled,

	// Custom Depth-Stencil is enabled; potentially simultaneous SRV / DSV usage.
	Enabled
};

static EMaterialCustomDepthPolicy GetMaterialCustomDepthPolicy(const FMaterial* Material)
{
	check(Material);

	// Material requesting stencil test and post processing CVar allows it.
	if (Material->IsStencilTestEnabled() && IsPostProcessStencilTestAllowed())
	{
		// Custom stencil texture allocated and available.
		if (GetCustomDepthMode() != ECustomDepthMode::EnabledWithStencil)
		{
			UE_LOG(LogRenderer, Warning, TEXT("PostProcessMaterial uses stencil test, but stencil not allocated. Set r.CustomDepth to 3 to allocate custom stencil."));
		}
		else if (Material->GetBlendableLocation() == BL_SceneColorAfterTonemapping)
		{
			// We can't support custom stencil after tonemapping due to target size differences
			UE_LOG(LogRenderer, Warning, TEXT("PostProcessMaterial uses stencil test, but is set to blend After Tonemapping. This is not supported."));
		}
		else
		{
			return EMaterialCustomDepthPolicy::Enabled;
		}
	}

	return EMaterialCustomDepthPolicy::Disabled;
}

static FRHIDepthStencilState* GetMaterialStencilState(const FMaterial* Material)
{
	static FRHIDepthStencilState* StencilStates[] =
	{
		TStaticDepthStencilState<false, CF_Always, true, CF_Less>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always, true, CF_LessEqual>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always, true, CF_Greater>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always, true, CF_GreaterEqual>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always, true, CF_Equal>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always, true, CF_NotEqual>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always, true, CF_Never>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always, true, CF_Always>::GetRHI(),
	};
	static_assert(EMaterialStencilCompare::MSC_Count == UE_ARRAY_COUNT(StencilStates), "Ensure that all EMaterialStencilCompare values are accounted for.");

	check(Material);

	return StencilStates[Material->GetStencilCompare()];
}

static bool IsMaterialBlendEnabled(const FMaterial* Material)
{
	check(Material);

	return Material->GetBlendableOutputAlpha() && CVarPostProcessAllowBlendModes.GetValueOnRenderThread() != 0;
}

static FRHIBlendState* GetMaterialBlendState(const FMaterial* Material)
{
	static FRHIBlendState* BlendStates[] =
	{
		TStaticBlendState<>::GetRHI(),
		TStaticBlendState<>::GetRHI(),
		TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI(),
		TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI(),
		TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI(),
		TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI(),
		TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI(),
		TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI(),
	};
	static_assert(EBlendMode::BLEND_MAX == UE_ARRAY_COUNT(BlendStates), "Ensure that all EBlendMode values are accounted for.");

	check(Material);

	if (Substrate::IsSubstrateEnabled())
	{
		switch (Material->GetBlendMode())
		{
		case EBlendMode::BLEND_Opaque:
		case EBlendMode::BLEND_Masked:
			return TStaticBlendState<>::GetRHI();
		case EBlendMode::BLEND_Additive:
			return TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
		case EBlendMode::BLEND_AlphaComposite:
		case EBlendMode::BLEND_TranslucentColoredTransmittance: // A platform may not support dual source blending so we always only use grey scale transmittance
		case EBlendMode::BLEND_TranslucentGreyTransmittance:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		case EBlendMode::BLEND_ColoredTransmittanceOnly:
			return TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI();
		case EBlendMode::BLEND_AlphaHoldout:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
		default:
			check(false);
			return TStaticBlendState<>::GetRHI();
		}
	}

	return BlendStates[Material->GetBlendMode()];
}

static bool PostProcessStencilTest(const uint32 StencilValue, const uint32 StencilComp, const uint32 StencilRef)
{
	bool bStencilTestPassed = true;

	switch (StencilComp)
	{
	case EMaterialStencilCompare::MSC_Less:
		bStencilTestPassed = (StencilRef < StencilValue);
		break;
	case EMaterialStencilCompare::MSC_LessEqual:
		bStencilTestPassed = (StencilRef <= StencilValue);
		break;
	case EMaterialStencilCompare::MSC_GreaterEqual:
		bStencilTestPassed = (StencilRef >= StencilValue);
		break;
	case EMaterialStencilCompare::MSC_Equal:
		bStencilTestPassed = (StencilRef == StencilValue);
		break;
	case EMaterialStencilCompare::MSC_Greater:
		bStencilTestPassed = (StencilRef > StencilValue);
		break;
	case EMaterialStencilCompare::MSC_NotEqual:
		bStencilTestPassed = (StencilRef != StencilValue);
		break;
	case EMaterialStencilCompare::MSC_Never:
		bStencilTestPassed = false;
		break;
	default:
		break;
	}

	return !bStencilTestPassed;
}

static uint32 GetManualStencilTestMask(uint32 StencilComp)
{
	// These enum values must match their #define counterparts in PostProcessMaterialShaders.ush
	enum StencilTestMask
	{
		Equal	= (1 << 0),
		Less	= (1 << 1),
		Greater	= (1 << 2)
	};
	uint32 Mask = 0;

	switch (StencilComp)
	{
	case EMaterialStencilCompare::MSC_Less:
		return Less;
	case EMaterialStencilCompare::MSC_LessEqual:
		return Less | Equal;
	case EMaterialStencilCompare::MSC_GreaterEqual:
		return Greater | Equal;
	case EMaterialStencilCompare::MSC_Equal:
		return Equal;
	case EMaterialStencilCompare::MSC_Greater:
		return Greater;
	case EMaterialStencilCompare::MSC_NotEqual:
		return Less | Greater;
	case EMaterialStencilCompare::MSC_Never:
		return 0;
	case EMaterialStencilCompare::MSC_Always:
	default:
		return Less | Equal | Greater;
	}
}

class FPostProcessMaterialShader : public FMaterialShader
{
public:
	using FParameters = FPostProcessMaterialParameters;
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FPostProcessMaterialShader, FMaterialShader);

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (Parameters.MaterialParameters.MaterialDomain == MD_PostProcess)
		{
			return !IsMobilePlatform(Parameters.Platform) || IsMobileHDR();
		}
		return false;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL"), 1);

		EBlendableLocation Location = EBlendableLocation(Parameters.MaterialParameters.BlendableLocation);
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Location == BL_SceneColorAfterTonemapping || Location == BL_ReplacingTonemapper) ? 0 : 1);
		// Post process SSR is always rendered at native resolution as if it was after tone mapping, so we need to account for the fact that it is independent from DRS.
		// SSR input should not be affected by exposure so it should be specified separately from POST_PROCESS_MATERIAL_BEFORE_TONEMAP 
		// in order to be able to make DRS independent CameraVector and WorldPosition nodes.
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_SSRINPUT"), (Location == BL_SSRInput) ? 1 : 0);

		if (IsMobilePlatform(Parameters.Platform))
		{
			OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Parameters.MaterialParameters.BlendableLocation != BL_SceneColorAfterTonemapping) ? 1 : 0);
		}

		// PostProcessMaterial can both read & write Substrate data
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_DEFERRED_SHADING"), 1);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* Proxy, const FMaterial& Material)
	{
		FMaterialShader::SetParameters(BatchedParameters, Proxy, Material, View);
	}
};

class FPostProcessMaterialVS : public FPostProcessMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPostProcessMaterialVS, Material);

	FPostProcessMaterialVS() = default;
	FPostProcessMaterialVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPostProcessMaterialShader(Initializer)
	{}
};

class FPostProcessMaterialPS : public FPostProcessMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPostProcessMaterialPS, Material);

	class FManualStencilTestDim : SHADER_PERMUTATION_BOOL("MANUAL_STENCIL_TEST");
	class FNeuralPostProcessPrePass : SHADER_PERMUTATION_BOOL("NEURAL_POSTPROCESS_PREPASS");
	using FPermutationDomain = TShaderPermutationDomain<FManualStencilTestDim,FNeuralPostProcessPrePass>;

	FPostProcessMaterialPS() = default;
	FPostProcessMaterialPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPostProcessMaterialShader(Initializer)
	{}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (!FPostProcessMaterialShader::ShouldCompilePermutation(Parameters))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Currently, we only need the manual stencil test permutations if stencil test is enabled and Nanite is supported.
		// See comments in CustomDepthRendering.h for more details.
		if (PermutationVector.Get<FManualStencilTestDim>())
		{
			return Parameters.MaterialParameters.bIsStencilTestEnabled && DoesPlatformSupportNanite(Parameters.Platform);
		}

		return true;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessMaterialVS, TEXT("/Engine/Private/PostProcessMaterialShaders.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FPostProcessMaterialPS, TEXT("/Engine/Private/PostProcessMaterialShaders.usf"), TEXT("MainPS"), SF_Pixel);

class FPostProcessMaterialVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FFilterVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FFilterVertex, Position), VET_Float4, 0, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

static void GetMaterialInfo(
	const UMaterialInterface* InMaterialInterface,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FPostProcessMaterialInputs& Inputs,
	const FMaterial*& OutMaterial,
	const FMaterialRenderProxy*& OutMaterialProxy,
	const FMaterialShaderMap*& OutMaterialShaderMap,
	TShaderRef<FPostProcessMaterialVS>& OutVertexShader,
	TShaderRef<FPostProcessMaterialPS>& OutPixelShader,
	bool bNeuralPostProcessPrepass = false)
{
	const FMaterialRenderProxy* MaterialProxy = InMaterialInterface->GetRenderProxy();
	check(MaterialProxy);

	const FMaterial* Material = nullptr;
	FMaterialShaders Shaders;
	while (MaterialProxy)
	{
		Material = MaterialProxy->GetMaterialNoFallback(InFeatureLevel);
		if (Material && Material->GetMaterialDomain() == MD_PostProcess)
		{
			FMaterialShaderTypes ShaderTypes;
			{
				const bool bManualStencilTest = Inputs.bManualStencilTest && Material->IsStencilTestEnabled();

				FPostProcessMaterialPS::FPermutationDomain PermutationVectorPS;
				PermutationVectorPS.Set<FPostProcessMaterialPS::FManualStencilTestDim>(bManualStencilTest);
				PermutationVectorPS.Set<FPostProcessMaterialPS::FNeuralPostProcessPrePass>(bNeuralPostProcessPrepass);

				ShaderTypes.AddShaderType<FPostProcessMaterialVS>();
				ShaderTypes.AddShaderType<FPostProcessMaterialPS>(PermutationVectorPS.ToDimensionValueId());
			}

			if (Material->TryGetShaders(ShaderTypes, nullptr, Shaders))
			{
				break;
			}
		}
		MaterialProxy = MaterialProxy->GetFallback(InFeatureLevel);
	}

	check(Material);

	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
	check(MaterialShaderMap);

	OutMaterial = Material;
	OutMaterialProxy = MaterialProxy;
	OutMaterialShaderMap = MaterialShaderMap;
	Shaders.TryGetVertexShader(OutVertexShader);
	Shaders.TryGetPixelShader(OutPixelShader);
}

TGlobalResource<FPostProcessMaterialVertexDeclaration> GPostProcessMaterialVertexDeclaration;

} //! namespace

void AddMobileMSAADecodeAndDrawTexturePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTexture Input,
	FScreenPassRenderTarget Output)
{
	const FScreenPassTextureViewport InputViewport(Input);
	const FScreenPassTextureViewport OutputViewport(Output);

	TShaderMapRef<FMSAADecodeAndCopyRectPS_Mobile> PixelShader(View.ShaderMap);

	FMSAADecodeAndCopyRectPS_Mobile::FParameters* Parameters = GraphBuilder.AllocParameters<FMSAADecodeAndCopyRectPS_Mobile::FParameters>();
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("MobileMSAADecodeAndDrawTexture"), View, OutputViewport, InputViewport, PixelShader, Parameters);
}

FPostProcessMaterialParameters* GetPostProcessMaterialParameters(
	FRDGBuilder& GraphBuilder, 
	const FPostProcessMaterialInputs& Inputs, 
	const FViewInfo& View,
	const FScreenPassTextureViewport& OutputViewport,
	FScreenPassRenderTarget& Output, 
	FRDGTextureRef DepthStencilTexture, 
	const uint32 MaterialStencilRef, 
	const FMaterial* Material, 
	const FMaterialShaderMap* MaterialShaderMap)
{
	FPostProcessMaterialParameters* PostProcessMaterialParameters = GraphBuilder.AllocParameters<FPostProcessMaterialParameters>();
	PostProcessMaterialParameters->SceneTextures = Inputs.SceneTextures;
	PostProcessMaterialParameters->View = View.ViewUniformBuffer;
	PostProcessMaterialParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));
	PostProcessMaterialParameters->PostProcessOutput = GetScreenPassTextureViewportParameters(OutputViewport);
	PostProcessMaterialParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	// The target color will be decoded if bForceIntermediateTarget is true in any case, but we might still need to decode the input color
	PostProcessMaterialParameters->bMetalMSAAHDRDecode = Inputs.bMetalMSAAHDRDecode ? 1 : 0;

	if (DepthStencilTexture)
	{
		PostProcessMaterialParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthStencilTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthRead_StencilRead);
	}
	PostProcessMaterialParameters->ManualStencilReferenceValue = MaterialStencilRef;
	PostProcessMaterialParameters->ManualStencilTestMask = GetManualStencilTestMask(Material->GetStencilCompare());

	PostProcessMaterialParameters->PostProcessInput_BilinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();;

	const FScreenPassTexture BlackDummy(GSystemTextures.GetBlackDummy(GraphBuilder));

	// This gets passed in whether or not it's used.
	GraphBuilder.RemoveUnusedTextureWarning(BlackDummy.Texture);

	FRHISamplerState* PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	for (uint32 InputIndex = 0; InputIndex < kPostProcessMaterialInputCountMax; ++InputIndex)
	{
		FScreenPassTextureSlice Input = Inputs.GetInput((EPostProcessMaterialInput)InputIndex);

		// Need to provide valid textures for when shader compilation doesn't cull unused parameters.
		if (!Input.IsValid() || !MaterialShaderMap->UsesSceneTexture(PPI_PostProcessInput0 + InputIndex))
		{
			Input = FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, BlackDummy);
		}

		PostProcessMaterialParameters->PostProcessInput[InputIndex] = GetScreenPassTextureInput(Input, PointClampSampler);
	}

	// Path tracing buffer textures
	for (uint32 InputIndex = 0; InputIndex < kPathTracingPostProcessMaterialInputCountMax; ++InputIndex)
	{
		FScreenPassTexture Input = Inputs.GetPathTracingInput((EPathTracingPostProcessMaterialInput)InputIndex);

		if (!Input.Texture || !MaterialShaderMap->UsesPathTracingBufferTexture(InputIndex))
		{
			Input = BlackDummy;
		}

		PostProcessMaterialParameters->PathTracingPostProcessInput[InputIndex] = GetScreenPassTextureInput(Input, PointClampSampler);
	}

	PostProcessMaterialParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

	// SceneDepthWithoutWater
	const bool bHasValidSceneDepthWithoutWater = Inputs.SceneWithoutWaterTextures && Inputs.SceneWithoutWaterTextures->DepthTexture;
	const bool bShouldUseBilinearSamplerForDepth = bHasValidSceneDepthWithoutWater && ShouldUseBilinearSamplerForDepthWithoutSingleLayerWater(Inputs.SceneWithoutWaterTextures->DepthTexture->Desc.Format);
	PostProcessMaterialParameters->bSceneDepthWithoutWaterTextureAvailable = bHasValidSceneDepthWithoutWater;
	PostProcessMaterialParameters->SceneDepthWithoutSingleLayerWaterSampler = bShouldUseBilinearSamplerForDepth ? TStaticSamplerState<SF_Bilinear>::GetRHI() : TStaticSamplerState<SF_Point>::GetRHI();
	PostProcessMaterialParameters->SceneDepthWithoutSingleLayerWaterTexture = FRDGSystemTextures::Get(GraphBuilder).Black;
	PostProcessMaterialParameters->SceneWithoutSingleLayerWaterMinMaxUV = FVector4f(0.0f, 0.0f, 1.0f, 1.0f);
	PostProcessMaterialParameters->SceneWithoutSingleLayerWaterTextureSize = FVector2f(0.0f, 0.0f);
	PostProcessMaterialParameters->SceneWithoutSingleLayerWaterInvTextureSize = FVector2f(0.0f, 0.0f);
	if (bHasValidSceneDepthWithoutWater)
	{
		const bool bIsInstancedStereoSideBySide = View.bIsInstancedStereoEnabled && !View.bIsMobileMultiViewEnabled && IStereoRendering::IsStereoEyeView(View);
		int32 WaterViewIndex = INDEX_NONE;
		if (bIsInstancedStereoSideBySide)
		{
			WaterViewIndex = View.PrimaryViewIndex; // The instanced view does not have MinMaxUV initialized, instead the primary view MinMaxUV covers both eyes
		}
		else
		{
			verify(View.Family->Views.Find(&View, WaterViewIndex));
		}

		PostProcessMaterialParameters->SceneDepthWithoutSingleLayerWaterTexture = Inputs.SceneWithoutWaterTextures->DepthTexture;
		PostProcessMaterialParameters->SceneWithoutSingleLayerWaterMinMaxUV = Inputs.SceneWithoutWaterTextures->Views[WaterViewIndex].MinMaxUV;

		const FIntVector DepthTextureSize = Inputs.SceneWithoutWaterTextures->DepthTexture->Desc.GetSize();
		PostProcessMaterialParameters->SceneWithoutSingleLayerWaterTextureSize = FVector2f(DepthTextureSize.X, DepthTextureSize.Y);
		PostProcessMaterialParameters->SceneWithoutSingleLayerWaterInvTextureSize = FVector2f(1.0f / DepthTextureSize.X, 1.0f / DepthTextureSize.Y);
	}

	PostProcessMaterialParameters->NeuralPostProcessParameters = GetDefaultNeuralPostProcessShaderParameters(GraphBuilder);

	return PostProcessMaterialParameters;
}

void AddNeuralPostProcessPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FPostProcessMaterialInputs& Inputs,
	const UMaterialInterface* MaterialInterface,
	FNeuralPostProcessResource& NeuralPostProcessResource)
{
	Inputs.Validate();

	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));

	const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();

	const FMaterial* Material = nullptr;
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	const FMaterialShaderMap* MaterialShaderMap = nullptr;
	TShaderRef<FPostProcessMaterialVS> NeuralPostProcessPassVertexShader;
	TShaderRef<FPostProcessMaterialPS> NeuralPostProcessPassPixelShader;
	GetMaterialInfo(MaterialInterface, FeatureLevel, Inputs, Material, MaterialRenderProxy, MaterialShaderMap, NeuralPostProcessPassVertexShader, NeuralPostProcessPassPixelShader, true);
	
	check(NeuralPostProcessPassVertexShader.IsValid());
	check(NeuralPostProcessPassPixelShader.IsValid());

	int32 NeuralProfileId = Material->GetNeuralProfileId();

	FRHIDepthStencilState* DefaultDepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();
	FRHIDepthStencilState* DepthStencilState = DefaultDepthStencilState;

	FRDGTextureRef DepthStencilTexture = nullptr;

	// Allocate custom depth stencil texture(s) and depth stencil state.
	const EMaterialCustomDepthPolicy CustomStencilPolicy = GetMaterialCustomDepthPolicy(Material);

	if (CustomStencilPolicy == EMaterialCustomDepthPolicy::Enabled &&
		!Inputs.bManualStencilTest &&
		HasBeenProduced(Inputs.CustomDepthTexture))
	{
		check(Inputs.CustomDepthTexture);
		DepthStencilTexture = Inputs.CustomDepthTexture;
		DepthStencilState = GetMaterialStencilState(Material);
	}

	FRHIBlendState* DefaultBlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
	FRHIBlendState* BlendState = DefaultBlendState;

	if (IsMaterialBlendEnabled(Material))
	{
		BlendState = GetMaterialBlendState(Material);
	}

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	// Create a new texture instead of reusing the scene color output in the pre pass. Should not pollute the scene color texture.
	{
		// Allocate new transient output texture.
		{
			FRDGTextureDesc OutputDesc = SceneColor.Texture->Desc;
			OutputDesc.Reset();
			if (Inputs.OutputFormat != PF_Unknown)
			{
				OutputDesc.Format = Inputs.OutputFormat;
			}
			OutputDesc.ClearValue = FClearValueBinding(FLinearColor::Black);
			OutputDesc.Flags &= (~ETextureCreateFlags::FastVRAM);
			OutputDesc.Flags |= GFastVRamConfig.PostProcessMaterial;

			Output = FScreenPassRenderTarget(GraphBuilder.CreateTexture(OutputDesc, TEXT("PostProcessTempOutput")), SceneColor.ViewRect, View.GetOverwriteLoadAction());
		}
	}

	const FScreenPassTextureViewport SceneColorViewport(SceneColor);
	const FScreenPassTextureViewport OutputViewport(Output);

	RDG_EVENT_SCOPE(GraphBuilder, "PostProcessMaterial::NeuralPass");

	const uint32 MaterialStencilRef = Material->GetStencilRefValue();

	const bool bMobilePlatform = IsMobilePlatform(View.GetShaderPlatform());


	EScreenPassDrawFlags ScreenPassFlags = EScreenPassDrawFlags::AllowHMDHiddenAreaMask;

	// check if we can skip that draw call in case if all pixels will fail the stencil test of the material
	bool bSkipPostProcess = false;

	if (Material->IsStencilTestEnabled() && IsPostProcessStencilTestAllowed())
	{
		bool bFailStencil = true;

		const uint32 StencilComp = Material->GetStencilCompare();

		// Always check against clear value, since a material might want to perform operations against that value
		const uint32 StencilClearValue = Inputs.CustomDepthTexture ? Inputs.CustomDepthTexture->Desc.ClearValue.Value.DSValue.Stencil : 0;
		bFailStencil &= PostProcessStencilTest(StencilClearValue, StencilComp, MaterialStencilRef);


		for (const uint32& Value : View.CustomDepthStencilValues)
		{
			bFailStencil &= PostProcessStencilTest(Value, StencilComp, MaterialStencilRef);

			if (!bFailStencil)
			{
				break;
			}
		}

		bSkipPostProcess = bFailStencil;
	}

	if (!bSkipPostProcess)
	{
		NeuralPostProcessResource = AllocateNeuralPostProcessingResourcesIfNeeded(
			GraphBuilder, OutputViewport, NeuralProfileId, Material->IsUsedWithNeuralNetworks());

		if (NeuralPostProcessResource.IsValid())
		{ 
			// Prepass to extract the input to the NNE Engine
			FPostProcessMaterialParameters* PostProcessMaterialParameters =
				GetPostProcessMaterialParameters(GraphBuilder, Inputs, View, OutputViewport, Output, DepthStencilTexture, MaterialStencilRef, Material, MaterialShaderMap);

			SetupNeuralPostProcessShaderParametersForWrite(PostProcessMaterialParameters->NeuralPostProcessParameters, GraphBuilder, NeuralPostProcessResource);

			ClearUnusedGraphResources(NeuralPostProcessPassVertexShader, NeuralPostProcessPassPixelShader, PostProcessMaterialParameters);

			//Only call the neural network when the shader resource is actually used.
			if (IsNeuralPostProcessShaderParameterUsed(PostProcessMaterialParameters->NeuralPostProcessParameters))
			{
				AddDrawScreenPass(
					GraphBuilder,
#if RDG_EVENTS != RDG_EVENTS_STRING_COPY
					RDG_EVENT_NAME("PostProcessMaterial(Neural Prepass)"),
#else
					FRDGEventName(*Material->GetAssetName()),
#endif
					View,
					OutputViewport,
					SceneColorViewport,
					// Uses default depth stencil on mobile since the stencil test is done in pixel shader.
					FScreenPassPipelineState(NeuralPostProcessPassVertexShader, NeuralPostProcessPassPixelShader, BlendState, DepthStencilState, MaterialStencilRef),
					PostProcessMaterialParameters,
					ScreenPassFlags,
					[&View, NeuralPostProcessPassVertexShader, NeuralPostProcessPassPixelShader, MaterialRenderProxy, Material, PostProcessMaterialParameters](FRHICommandList& RHICmdList)
					{
						SetShaderParametersMixedVS(RHICmdList, NeuralPostProcessPassVertexShader, *PostProcessMaterialParameters, View, MaterialRenderProxy, *Material);
						SetShaderParametersMixedPS(RHICmdList, NeuralPostProcessPassPixelShader, *PostProcessMaterialParameters, View, MaterialRenderProxy, *Material);
					});

				ApplyNeuralPostProcess(GraphBuilder, View, Output.ViewRect, NeuralPostProcessResource);
			}
		}
	}
}

FScreenPassTexture AddPostProcessMaterialPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FPostProcessMaterialInputs& Inputs,
	const UMaterialInterface* MaterialInterface)
{
	Inputs.Validate();

	const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();

	const FMaterial* Material = nullptr;
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	const FMaterialShaderMap* MaterialShaderMap = nullptr;
	TShaderRef<FPostProcessMaterialVS> VertexShader;
	TShaderRef<FPostProcessMaterialPS> PixelShader;
	GetMaterialInfo(MaterialInterface, FeatureLevel, Inputs, Material, MaterialRenderProxy, MaterialShaderMap, VertexShader, PixelShader);

	EBlendableLocation BlendableLocation = EBlendableLocation(Material->GetBlendableLocation());
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(BlendableLocation == BL_TranslucencyAfterDOF ? EPostProcessMaterialInput::SeparateTranslucency : EPostProcessMaterialInput::SceneColor));

	check(VertexShader.IsValid());
	check(PixelShader.IsValid());

	FRHIDepthStencilState* DefaultDepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();
	FRHIDepthStencilState* DepthStencilState = DefaultDepthStencilState;

	FRDGTextureRef DepthStencilTexture = nullptr;

	// Allocate custom depth stencil texture(s) and depth stencil state.
	const EMaterialCustomDepthPolicy CustomStencilPolicy = GetMaterialCustomDepthPolicy(Material);

	if (CustomStencilPolicy == EMaterialCustomDepthPolicy::Enabled &&
		!Inputs.bManualStencilTest &&
		HasBeenProduced(Inputs.CustomDepthTexture))
	{
		check(Inputs.CustomDepthTexture);
		DepthStencilTexture = Inputs.CustomDepthTexture;
		DepthStencilState = GetMaterialStencilState(Material);
	}

	FRHIBlendState* DefaultBlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
	FRHIBlendState* BlendState = DefaultBlendState;
	
	if (IsMaterialBlendEnabled(Material))
	{
		BlendState = GetMaterialBlendState(Material);
	}

	// Determine if the pixel shader may discard, requiring us to initialize the output texture
	const bool bMayDiscard = CustomStencilPolicy == EMaterialCustomDepthPolicy::Enabled && Inputs.bManualStencilTest;

	// Blend / Depth Stencil usage requires that the render target have primed color data.
	const bool bCompositeWithInput = DepthStencilState != DefaultDepthStencilState ||
									 BlendState != DefaultBlendState ||
									 bMayDiscard;

	// We only prime color on the output texture if we are using fixed function Blend / Depth-Stencil,
	// or we need to retain previously rendered views.
	const bool bPrimeOutputColor = bCompositeWithInput || !View.IsFirstInFamily();

	// Inputs.OverrideOutput is used to force drawing directly to the backbuffer. OpenGL doesn't support using the backbuffer color target with a custom depth/stencil
	// buffer, so in that case we must draw to an intermediate target and copy to the backbuffer at the end. Ideally, we would test if Inputs.OverrideOutput.Texture
	// is actually the backbuffer, but it's not worth doing all the plumbing and increasing the RHI surface area just for this hack.
	const bool bBackbufferWithDepthStencil = (DepthStencilTexture != nullptr && !GRHISupportsBackBufferWithCustomDepthStencil && Inputs.OverrideOutput.IsValid());

	// We need to decode the target color for blending material, force it rendering to an intermediate render target and decode the color.
	const bool bCompositeWithInputAndDecode = Inputs.bMetalMSAAHDRDecode && bCompositeWithInput;

	const bool bForceIntermediateTarget = bBackbufferWithDepthStencil || bCompositeWithInputAndDecode;

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;

	// We can re-use the scene color texture as the render target if we're not simultaneously reading from it.
	// This is only necessary to do if we're going to be priming content from the render target since it avoids
	// the copy. Otherwise, we just allocate a new render target.
	const bool bValidShaderPlatform = (GMaxRHIShaderPlatform != SP_PCD3D_ES3_1);
	if (!Output.IsValid() && !MaterialShaderMap->UsesSceneTexture(PPI_PostProcessInput0) && bPrimeOutputColor && !bForceIntermediateTarget && Inputs.bAllowSceneColorInputAsOutput && bValidShaderPlatform)
	{
		Output = FScreenPassRenderTarget(SceneColor, ERenderTargetLoadAction::ELoad);
	}
	else
	{
		// Allocate new transient output texture if none exists.
		if (!Output.IsValid() || bForceIntermediateTarget)
		{
			FRDGTextureDesc OutputDesc = SceneColor.Texture->Desc;
			OutputDesc.Reset();
			if (Inputs.OutputFormat != PF_Unknown)
			{
				OutputDesc.Format = Inputs.OutputFormat;
			}
			OutputDesc.ClearValue = FClearValueBinding(FLinearColor::Black);
			OutputDesc.Flags &= (~ETextureCreateFlags::FastVRAM);
			OutputDesc.Flags |= GFastVRamConfig.PostProcessMaterial;

			Output = FScreenPassRenderTarget(GraphBuilder.CreateTexture(OutputDesc, TEXT("PostProcessMaterial")), SceneColor.ViewRect, View.GetOverwriteLoadAction());
		}

		if (bPrimeOutputColor || bForceIntermediateTarget)
		{
			// Copy existing contents to new output and use load-action to preserve untouched pixels.
			if (Inputs.bMetalMSAAHDRDecode)
			{
				AddMobileMSAADecodeAndDrawTexturePass(GraphBuilder, View, SceneColor, Output);
			}
			else
			{
				AddDrawTexturePass(GraphBuilder, View, SceneColor, Output);
			}
			Output.LoadAction = ERenderTargetLoadAction::ELoad;
		}
	}

	const FScreenPassTextureViewport SceneColorViewport(SceneColor);
	const FScreenPassTextureViewport OutputViewport(Output);

	RDG_EVENT_SCOPE(GraphBuilder, "PostProcessMaterial %dx%d Material=%s", SceneColorViewport.Rect.Width(), SceneColorViewport.Rect.Height(), *Material->GetAssetName());

	const uint32 MaterialStencilRef = Material->GetStencilRefValue();

	const bool bMobilePlatform = IsMobilePlatform(View.GetShaderPlatform());

	EScreenPassDrawFlags ScreenPassFlags = EScreenPassDrawFlags::AllowHMDHiddenAreaMask;

	// check if we can skip that draw call in case if all pixels will fail the stencil test of the material
	bool bSkipPostProcess = false;

	if (Material->IsStencilTestEnabled() && IsPostProcessStencilTestAllowed())
	{
		bool bFailStencil = true;

		const uint32 StencilComp = Material->GetStencilCompare();

		// Always check against clear value, since a material might want to perform operations against that value
		const uint32 StencilClearValue = Inputs.CustomDepthTexture ? Inputs.CustomDepthTexture->Desc.ClearValue.Value.DSValue.Stencil : 0;
		bFailStencil &= PostProcessStencilTest(StencilClearValue, StencilComp, MaterialStencilRef);


		for (const uint32& Value : View.CustomDepthStencilValues)
		{
			bFailStencil &= PostProcessStencilTest(Value, StencilComp, MaterialStencilRef);

			if (!bFailStencil)
			{
				break;
			}
		}

		bSkipPostProcess = bFailStencil;
	}

	if (!bSkipPostProcess)
	{

		FNeuralPostProcessResource NeuralPostProcessResource;
		const bool bShouldApplyNeuralPostProcessing = ShouldApplyNeuralPostProcessForMaterial(Material);

		if (bShouldApplyNeuralPostProcessing)
		{
			AddNeuralPostProcessPass(GraphBuilder, View, Inputs, MaterialInterface, NeuralPostProcessResource);
		}

		{
			FPostProcessMaterialParameters* PostProcessMaterialParameters =
				GetPostProcessMaterialParameters(GraphBuilder, Inputs, View, OutputViewport, Output, DepthStencilTexture, MaterialStencilRef, Material, MaterialShaderMap);

			if (bShouldApplyNeuralPostProcessing)
			{
				SetupNeuralPostProcessShaderParametersForRead(PostProcessMaterialParameters->NeuralPostProcessParameters, GraphBuilder, NeuralPostProcessResource);
			}

			ClearUnusedGraphResources(VertexShader, PixelShader, PostProcessMaterialParameters);

			AddDrawScreenPass(
				GraphBuilder,
#if RDG_EVENTS != RDG_EVENTS_STRING_COPY
				RDG_EVENT_NAME("PostProcessMaterial"),
#else
				FRDGEventName(*Material->GetAssetName()),
#endif
				View,
				OutputViewport,
				SceneColorViewport,
				// Uses default depth stencil on mobile since the stencil test is done in pixel shader.
				FScreenPassPipelineState(VertexShader, PixelShader, BlendState, DepthStencilState, MaterialStencilRef),
				PostProcessMaterialParameters,
				ScreenPassFlags,
				[&View, VertexShader, PixelShader, MaterialRenderProxy, Material, PostProcessMaterialParameters](FRHICommandList& RHICmdList)
				{
					SetShaderParametersMixedVS(RHICmdList, VertexShader, *PostProcessMaterialParameters, View, MaterialRenderProxy, *Material);
					SetShaderParametersMixedPS(RHICmdList, PixelShader, *PostProcessMaterialParameters, View, MaterialRenderProxy, *Material);
				});
		}

		if (bForceIntermediateTarget && !bCompositeWithInputAndDecode)
		{
			// We shouldn't get here unless we had an override target.
			check(Inputs.OverrideOutput.IsValid());
			AddDrawTexturePass(GraphBuilder, View, Output.Texture, Inputs.OverrideOutput.Texture);
			Output = Inputs.OverrideOutput;
		}
	}
	else
	{
		// When skipping the pass, we still need to output a valid FScreenPassRenderTarget
		Output = FScreenPassRenderTarget(SceneColor, ERenderTargetLoadAction::ENoAction);

		// If there is override output, we need to output to that
		if (Inputs.OverrideOutput.IsValid())
		{
			const FIntPoint SrcPoint = View.ViewRect.Min;
			const FIntPoint DstPoint = OutputViewport.Rect.Min;
			const FIntPoint Size = OutputViewport.Rect.Max - DstPoint;
			AddDrawTexturePass(GraphBuilder, View, Output.Texture, Inputs.OverrideOutput.Texture, SrcPoint, DstPoint, Size);
			Output = Inputs.OverrideOutput;
		}
	}

	return MoveTemp(Output);
}

FScreenPassTexture AddPostProcessMaterialPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs,
	const UMaterialInterface* MaterialInterface)
{
	if (!ensureMsgf(View.bIsViewInfo, TEXT("AddPostProcessMaterialPass requires that its View parameter is an FViewInfo.")))
	{
		return FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	}

	return AddPostProcessMaterialPass(GraphBuilder, static_cast<const FViewInfo&>(View), Inputs, MaterialInterface);
}

static bool IsPostProcessMaterialsEnabledForView(const FViewInfo& View)
{
	if (!View.Family->EngineShowFlags.PostProcessing ||
		!View.Family->EngineShowFlags.PostProcessMaterial ||
		View.Family->EngineShowFlags.VisualizeShadingModels ||
		CVarPostProcessingDisableMaterials.GetValueOnRenderThread() != 0)
	{
		return false;
	}

	return true;
}

static FPostProcessMaterialNode* IteratePostProcessMaterialNodes(const FFinalPostProcessSettings& Dest, EBlendableLocation Location, FBlendableEntry*& Iterator)
{
	for (;;)
	{
		FPostProcessMaterialNode* DataPtr = Dest.BlendableManager.IterateBlendables<FPostProcessMaterialNode>(Iterator);

		if (!DataPtr || DataPtr->GetLocation() == Location || Location == EBlendableLocation::BL_MAX)
		{
			return DataPtr;
		}
	}
}

FPostProcessMaterialChain GetPostProcessMaterialChain(const FViewInfo& View, EBlendableLocation Location)
{
	if (!IsPostProcessMaterialsEnabledForView(View))
	{
		return {};
	}

	const FSceneViewFamily& ViewFamily = *View.Family;

	TArray<FPostProcessMaterialNode, TInlineAllocator<10>> Nodes;
	FBlendableEntry* Iterator = nullptr;

	if (ViewFamily.EngineShowFlags.VisualizeBuffer)
	{
		UMaterialInterface* VisMaterial = GetBufferVisualizationData().GetMaterial(View.CurrentBufferVisualizationMode);
		UMaterial* Material = VisMaterial ? VisMaterial->GetMaterial() : nullptr;

		if (Material && (Material->BlendableLocation == Location || Location == EBlendableLocation::BL_MAX))
		{
			Nodes.Add(FPostProcessMaterialNode(Material, Material->BlendableLocation, Material->BlendablePriority, Material->bIsBlendable));
		}
	}

	while (FPostProcessMaterialNode* Data = IteratePostProcessMaterialNodes(View.FinalPostProcessSettings, Location, Iterator))
	{
		check(Data->GetMaterialInterface());
		Nodes.Add(*Data);
	}

	if (!Nodes.Num())
	{
		return {};
	}

	Algo::Sort(Nodes, FPostProcessMaterialNode::FCompare());

	FPostProcessMaterialChain OutputChain;
	OutputChain.Reserve(Nodes.Num());

	for (const FPostProcessMaterialNode& Node : Nodes)
	{
		OutputChain.Add(Node.GetMaterialInterface());
	}

	return OutputChain;
}

FScreenPassTexture AddPostProcessMaterialChain(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FPostProcessMaterialInputs& InputsTemplate,
	const FPostProcessMaterialChain& Materials,
	EPostProcessMaterialInput MaterialInput)
{
	FScreenPassTextureSlice CurrentInput = InputsTemplate.GetInput(MaterialInput);
	FScreenPassTexture Outputs;

	bool bFirstMaterialInChain = true;
	for (const UMaterialInterface* MaterialInterface : Materials)
	{
		FPostProcessMaterialInputs Inputs = InputsTemplate;
		Inputs.SetInput(MaterialInput, CurrentInput);
		
		// Only the first material in the chain needs to decode the input color
		Inputs.bMetalMSAAHDRDecode = Inputs.bMetalMSAAHDRDecode && bFirstMaterialInChain;
		bFirstMaterialInChain = false;

		// Certain inputs are only respected by the final post process material in the chain.
		if (MaterialInterface != Materials.Last())
		{
			Inputs.OverrideOutput = FScreenPassRenderTarget();
		}

		Outputs = AddPostProcessMaterialPass(GraphBuilder, View, Inputs, MaterialInterface);

		// Don't create the CurrentInput out of Outputs of the last material as this could possibly be the back buffer for AfterTonemap post process material
		if (MaterialInterface != Materials.Last())
		{
			CurrentInput = FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, Outputs);
		}
	}

	if (!Outputs.IsValid())
	{
		Outputs = FScreenPassTexture::CopyFromSlice(GraphBuilder, CurrentInput);
	}

	return Outputs;
}

extern void AddDumpToColorArrayPass(FRDGBuilder& GraphBuilder, FScreenPassTexture Input, TArray<FColor>* OutputColorArray, FIntPoint* OutputExtents);

bool IsHighResolutionScreenshotMaskEnabled(const FViewInfo& View)
{
	return View.Family->EngineShowFlags.HighResScreenshotMask || View.FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial;
}

bool IsPathTracingVarianceTextureRequiredInPostProcessMaterial(const FViewInfo& View)
{
	// query the post process material to check if any variance texture has been used
	bool bIsPathTracingVarianceTextureRequired = false;

	auto CheckIfPathTracingVarianceTextureIsRequried = [&](const UMaterialInterface* MaterialInterface) {
		
		if (MaterialInterface)
		{
			// Get the RenderProxy of the material.
			const FMaterialRenderProxy* MaterialProxy = MaterialInterface->GetRenderProxy();

			if (MaterialProxy)
			{

				// Get the Shadermap for the view's feature level
				const FMaterial* Material = MaterialProxy->GetMaterialNoFallback(View.FeatureLevel);
				if (Material && Material->GetMaterialDomain() == MD_PostProcess)
				{
					const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();

					if (MaterialShaderMap &&
						MaterialShaderMap->UsesPathTracingBufferTexture(static_cast<uint32>(EPathTracingPostProcessMaterialInput::Variance)))
					{
						return true;
					}
				}
			}
		}

		return false;
	};

	FPostProcessMaterialChain PostProcessMaterialChain = GetPostProcessMaterialChain(View, EBlendableLocation::BL_MAX);
	for (const UMaterialInterface* MaterialInterface : PostProcessMaterialChain)
	{
		if (CheckIfPathTracingVarianceTextureIsRequried(MaterialInterface))
		{
			bIsPathTracingVarianceTextureRequired = true;
			break;
		}
	}

	// Check buffer visualization pipes
	const FFinalPostProcessSettings& PostProcessSettings = View.FinalPostProcessSettings;
	for (const UMaterialInterface* MaterialInterface : PostProcessSettings.BufferVisualizationOverviewMaterials)
	{
		if (CheckIfPathTracingVarianceTextureIsRequried(MaterialInterface))
		{
			bIsPathTracingVarianceTextureRequired = true;
			break;
		}
	}

	return bIsPathTracingVarianceTextureRequired;
}

FScreenPassTexture AddHighResolutionScreenshotMaskPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FHighResolutionScreenshotMaskInputs& Inputs)
{
	check(Inputs.Material || Inputs.MaskMaterial || Inputs.CaptureRegionMaterial);

	enum class EPass
	{
		Material,
		MaskMaterial,
		CaptureRegionMaterial,
		MAX
	};

	const TCHAR* PassNames[]
	{
		TEXT("Material"),
		TEXT("MaskMaterial"),
		TEXT("CaptureRegionMaterial")
	};

	static_assert(UE_ARRAY_COUNT(PassNames) == static_cast<uint32>(EPass::MAX), "Pass names array doesn't match pass enum");

	const bool bHighResScreenshotMask = View.Family->EngineShowFlags.HighResScreenshotMask != 0;

	TOverridePassSequence<EPass> PassSequence(Inputs.OverrideOutput);
	PassSequence.SetEnabled(EPass::Material, bHighResScreenshotMask && Inputs.Material != nullptr);
	PassSequence.SetEnabled(EPass::MaskMaterial, bHighResScreenshotMask && Inputs.MaskMaterial != nullptr && GIsHighResScreenshot);
	PassSequence.SetEnabled(EPass::CaptureRegionMaterial, Inputs.CaptureRegionMaterial != nullptr);
	PassSequence.Finalize();

	FScreenPassTexture Output = Inputs.SceneColor;

	if (PassSequence.IsEnabled(EPass::Material))
	{
		FPostProcessMaterialInputs PassInputs;
		PassSequence.AcceptOverrideIfLastPass(EPass::Material, PassInputs.OverrideOutput);
		PassInputs.SetInput(GraphBuilder, EPostProcessMaterialInput::SceneColor, Output);
		PassInputs.SceneTextures = Inputs.SceneTextures;

		Output = AddPostProcessMaterialPass(GraphBuilder, View, PassInputs, Inputs.Material);
	}

	if (PassSequence.IsEnabled(EPass::MaskMaterial))
	{
		PassSequence.AcceptPass(EPass::MaskMaterial);

		FPostProcessMaterialInputs PassInputs;
		PassInputs.SetInput(GraphBuilder, EPostProcessMaterialInput::SceneColor, Output);
		PassInputs.SceneTextures = Inputs.SceneTextures;

		// Explicitly allocate the render target to match the FSceneView extents and rect, so the output pixel arrangement matches
		FRDGTextureDesc MaskOutputDesc = Output.Texture->Desc;
		MaskOutputDesc.Reset();
		MaskOutputDesc.ClearValue = FClearValueBinding(FLinearColor::Black);
		MaskOutputDesc.Flags |= GFastVRamConfig.PostProcessMaterial;
		MaskOutputDesc.Extent = View.UnconstrainedViewRect.Size();

		PassInputs.OverrideOutput = FScreenPassRenderTarget(
			GraphBuilder.CreateTexture(MaskOutputDesc, TEXT("PostProcessMaterial")), View.UnscaledViewRect, View.GetOverwriteLoadAction());

		// Disallow the scene color input as output optimization since we need to not pollute the scene texture.
		PassInputs.bAllowSceneColorInputAsOutput = false;

		FScreenPassTexture MaskOutput = AddPostProcessMaterialPass(GraphBuilder, View, PassInputs, Inputs.MaskMaterial);
		AddDumpToColorArrayPass(GraphBuilder, MaskOutput, FScreenshotRequest::GetHighresScreenshotMaskColorArray(), &FScreenshotRequest::GetHighresScreenshotMaskExtents());

		// The mask material pass is actually outputting to system memory. If we're the last pass in the chain
		// and the override output is valid, we need to perform a copy of the input to the output. Since we can't
		// sample from the override output (since it might be the backbuffer), we still need to participate in
		// the pass sequence.
		if (PassSequence.IsLastPass(EPass::MaskMaterial) && Inputs.OverrideOutput.IsValid())
		{
			AddDrawTexturePass(GraphBuilder, View, Output, Inputs.OverrideOutput);
			Output = Inputs.OverrideOutput;
		}
	}

	if (PassSequence.IsEnabled(EPass::CaptureRegionMaterial))
	{
		FPostProcessMaterialInputs PassInputs;
		PassSequence.AcceptOverrideIfLastPass(EPass::CaptureRegionMaterial, PassInputs.OverrideOutput);
		PassInputs.SetInput(GraphBuilder, EPostProcessMaterialInput::SceneColor, Output);
		PassInputs.SceneTextures = Inputs.SceneTextures;

		Output = AddPostProcessMaterialPass(GraphBuilder, View, PassInputs, Inputs.CaptureRegionMaterial);
	}

	return Output;
}
