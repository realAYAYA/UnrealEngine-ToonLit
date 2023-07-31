// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMaterial.cpp: Post processing Material implementation.
=============================================================================*/

#include "PostProcess/PostProcessMaterial.h"
#include "RendererModule.h"
#include "Materials/Material.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
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
#include "Strata/Strata.h"
#include "SingleLayerWaterRendering.h"

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

bool IsPostProcessStencilTestAllowed()
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

EMaterialCustomDepthPolicy GetMaterialCustomDepthPolicy(const FMaterial* Material)
{
	check(Material);

	// Material requesting stencil test and post processing CVar allows it.
	if (Material->IsStencilTestEnabled() && IsPostProcessStencilTestAllowed())
	{
		// Custom stencil texture allocated and available.
		if (GetCustomDepthMode() == ECustomDepthMode::EnabledWithStencil)
		{
			return EMaterialCustomDepthPolicy::Enabled;
		}
		else
		{
			UE_LOG(LogRenderer, Warning, TEXT("PostProcessMaterial uses stencil test, but stencil not allocated. Set r.CustomDepth to 3 to allocate custom stencil."));
		}
	}

	return EMaterialCustomDepthPolicy::Disabled;
}

FRHIDepthStencilState* GetMaterialStencilState(const FMaterial* Material)
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

bool IsMaterialBlendEnabled(const FMaterial* Material)
{
	check(Material);

	return Material->GetBlendableOutputAlpha() && CVarPostProcessAllowBlendModes.GetValueOnRenderThread() != 0;
}

FRHIBlendState* GetMaterialBlendState(const FMaterial* Material)
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
	};
	static_assert(EBlendMode::BLEND_MAX == UE_ARRAY_COUNT(BlendStates), "Ensure that all EBlendMode values are accounted for.");

	check(Material);

	if (Strata::IsStrataEnabled())
	{
		switch (Material->GetStrataBlendMode())
		{
		case EStrataBlendMode::SBM_Opaque:
		case EStrataBlendMode::SBM_Masked:
			return TStaticBlendState<>::GetRHI();
		case EStrataBlendMode::SBM_TranslucentColoredTransmittance: // A platform may not support dual source blending so we always only use grey scale transmittance
		case EStrataBlendMode::SBM_TranslucentGreyTransmittance:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		case EStrataBlendMode::SBM_ColoredTransmittanceOnly:
			return TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI();
		case EStrataBlendMode::SBM_AlphaHoldout:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
		default:
			check(false);
			return TStaticBlendState<>::GetRHI();
		}
	}

	return BlendStates[Material->GetBlendMode()];
}

bool PostProcessStencilTest(const uint32 StencilValue, const uint32 StencilComp, const uint32 StencilRef)
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
		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Location == BL_AfterTonemapping || Location == BL_ReplacingTonemapper) ? 0 : 1);

		if (IsMobilePlatform(Parameters.Platform))
		{
			OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Parameters.MaterialParameters.BlendableLocation != BL_AfterTonemapping) ? 1 : 0);
		}

		// PostProcessMaterial can both read & write Strata data
		OutEnvironment.SetDefine(TEXT("STRATA_INLINE_SHADING"), 1);
		OutEnvironment.SetDefine(TEXT("STRATA_DEFERRED_SHADING"), 1);
	}

protected:
	template <typename TRHIShader>
	static void SetParameters(FRHICommandList& RHICmdList, const TShaderRef<FPostProcessMaterialShader> & Shader, TRHIShader* ShaderRHI, const FViewInfo& View, const FMaterialRenderProxy* Proxy, const FMaterial& Material, const FParameters& Parameters)
	{
		FMaterialShader* MaterialShader = Shader.GetShader();
		MaterialShader->SetParameters(RHICmdList, ShaderRHI, Proxy, Material, View);
		SetShaderParameters(RHICmdList, Shader, ShaderRHI, Parameters);
	}
};

class FPostProcessMaterialVS : public FPostProcessMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPostProcessMaterialVS, Material);

	static void SetParameters(FRHICommandList& RHICmdList, const TShaderRef<FPostProcessMaterialVS>& Shader, const FViewInfo& View, const FMaterialRenderProxy* Proxy, const FMaterial& Material, const FParameters& Parameters)
	{
		FPostProcessMaterialShader::SetParameters(RHICmdList, Shader, Shader.GetVertexShader(), View, Proxy, Material, Parameters);
	}

	FPostProcessMaterialVS() = default;
	FPostProcessMaterialVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPostProcessMaterialShader(Initializer)
	{}
};

class FPostProcessMaterialPS : public FPostProcessMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPostProcessMaterialPS, Material);

	static void SetParameters(FRHICommandList& RHICmdList, const TShaderRef<FPostProcessMaterialPS>& Shader, const FViewInfo& View, const FMaterialRenderProxy* Proxy, const FMaterial& Material, const FParameters& Parameters)
	{
		FPostProcessMaterialShader::SetParameters(RHICmdList, Shader, Shader.GetPixelShader(), View, Proxy, Material, Parameters);
	}

	FPostProcessMaterialPS() = default;
	FPostProcessMaterialPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPostProcessMaterialShader(Initializer)
	{}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPostProcessMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessMaterialVS, TEXT("/Engine/Private/PostProcessMaterialShaders.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FPostProcessMaterialPS, TEXT("/Engine/Private/PostProcessMaterialShaders.usf"), TEXT("MainPS"), SF_Pixel);

class FPostProcessMaterialVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	void InitRHI() override
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

void GetMaterialInfo(
	const UMaterialInterface* InMaterialInterface,
	ERHIFeatureLevel::Type InFeatureLevel,
	EPixelFormat InOutputFormat,
	const FMaterial*& OutMaterial,
	const FMaterialRenderProxy*& OutMaterialProxy,
	const FMaterialShaderMap*& OutMaterialShaderMap,
	TShaderRef<FPostProcessMaterialVS>& OutVertexShader,
	TShaderRef<FPostProcessMaterialPS>& OutPixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	{
		ShaderTypes.AddShaderType< FPostProcessMaterialVS>();
		ShaderTypes.AddShaderType< FPostProcessMaterialPS>();
	}

	const FMaterialRenderProxy* MaterialProxy = InMaterialInterface->GetRenderProxy();
	check(MaterialProxy);

	const FMaterial* Material = nullptr;
	FMaterialShaders Shaders;
	while (MaterialProxy)
	{
		Material = MaterialProxy->GetMaterialNoFallback(InFeatureLevel);
		if (Material && Material->GetMaterialDomain() == MD_PostProcess)
		{
			if (Material->TryGetShaders(ShaderTypes, nullptr, Shaders))
			{
				break;
			}
		}
		MaterialProxy = MaterialProxy->GetFallback(InFeatureLevel);
	}

	check(Material);

	if (Material->IsStencilTestEnabled() || Material->GetBlendableOutputAlpha())
	{
		// Only allowed to have blend/stencil test if output format is compatible with ePId_Input0. 
		// PF_Unknown implies output format is that of EPId_Input0
		ensure(InOutputFormat == PF_Unknown);
	}

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

FScreenPassTexture AddPostProcessMaterialPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FPostProcessMaterialInputs& Inputs,
	const UMaterialInterface* MaterialInterface)
{
	Inputs.Validate();

	const FScreenPassTexture SceneColor = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);

	const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();

	const FMaterial* Material = nullptr;
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	const FMaterialShaderMap* MaterialShaderMap = nullptr;
	TShaderRef<FPostProcessMaterialVS> VertexShader;
	TShaderRef<FPostProcessMaterialPS> PixelShader;
	GetMaterialInfo(MaterialInterface, FeatureLevel, Inputs.OutputFormat, Material, MaterialRenderProxy, MaterialShaderMap, VertexShader, PixelShader);

	FRHIDepthStencilState* DefaultDepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();
	FRHIDepthStencilState* DepthStencilState = DefaultDepthStencilState;

	FRDGTextureRef DepthStencilTexture = nullptr;

	// Allocate custom depth stencil texture(s) and depth stencil state.
	const EMaterialCustomDepthPolicy CustomStencilPolicy = GetMaterialCustomDepthPolicy(Material);

	if (CustomStencilPolicy == EMaterialCustomDepthPolicy::Enabled && HasBeenProduced(Inputs.CustomDepthTexture))
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

	// Blend / Depth Stencil usage requires that the render target have primed color data.
	const bool bCompositeWithInput = DepthStencilState != DefaultDepthStencilState || BlendState != DefaultBlendState;

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
	const bool bValidShaderPlatform = (GMaxRHIShaderPlatform != SP_PCD3D_ES3_1) && (GMaxRHIShaderPlatform != SP_D3D_ES3_1_HOLOLENS); // This might actually work with Hololens GPUs, but we haven't enabled it before
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

	FPostProcessMaterialParameters* PostProcessMaterialParameters = GraphBuilder.AllocParameters<FPostProcessMaterialParameters>();
	PostProcessMaterialParameters->SceneTextures = Inputs.SceneTextures;
	PostProcessMaterialParameters->View = View.ViewUniformBuffer;
	if (bMobilePlatform)
	{
		PostProcessMaterialParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View), PF_A32B32G32R32F);
	}
	else
	{
		PostProcessMaterialParameters->EyeAdaptationTexture = GetEyeAdaptationTexture(GraphBuilder, View);
	}
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

	PostProcessMaterialParameters->PostProcessInput_BilinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();;

	const FScreenPassTexture BlackDummy(GSystemTextures.GetBlackDummy(GraphBuilder));

    // This gets passed in whether or not it's used.
	GraphBuilder.RemoveUnusedTextureWarning(BlackDummy.Texture);

	FRHISamplerState* PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	for (uint32 InputIndex = 0; InputIndex < kPostProcessMaterialInputCountMax; ++InputIndex)
	{
		FScreenPassTexture Input = Inputs.GetInput((EPostProcessMaterialInput)InputIndex);

		// Need to provide valid textures for when shader compilation doesn't cull unused parameters.
		if (!Input.Texture || !MaterialShaderMap->UsesSceneTexture(PPI_PostProcessInput0 + InputIndex))
		{
			Input = BlackDummy;
		}

		PostProcessMaterialParameters->PostProcessInput[InputIndex] = GetScreenPassTextureInput(Input, PointClampSampler);
	}

	PostProcessMaterialParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);

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

	ClearUnusedGraphResources(VertexShader, PixelShader, PostProcessMaterialParameters);

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
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("PostProcessMaterial"),
			View,
			OutputViewport,
			SceneColorViewport,
			// Uses default depth stencil on mobile since the stencil test is done in pixel shader.
			FScreenPassPipelineState(VertexShader, PixelShader, BlendState, DepthStencilState, MaterialStencilRef),
			PostProcessMaterialParameters,
			ScreenPassFlags,
			[&View, VertexShader, PixelShader, MaterialRenderProxy, Material, PostProcessMaterialParameters](FRHICommandList& RHICmdList)
		{
			FPostProcessMaterialVS::SetParameters(RHICmdList, VertexShader, View, MaterialRenderProxy, *Material, *PostProcessMaterialParameters);
			FPostProcessMaterialPS::SetParameters(RHICmdList, PixelShader, View, MaterialRenderProxy, *Material, *PostProcessMaterialParameters);
		});

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
			AddDrawTexturePass(GraphBuilder, View, Output.Texture, Inputs.OverrideOutput.Texture);
			Output = Inputs.OverrideOutput;
		}
	}

	return MoveTemp(Output);
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

		if (!DataPtr || DataPtr->GetLocation() == Location)
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

		if (Material && Material->BlendableLocation == Location)
		{
			Nodes.Add(FPostProcessMaterialNode(Material, Location, Material->BlendablePriority, Material->bIsBlendable));
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

	::Sort(Nodes.GetData(), Nodes.Num(), FPostProcessMaterialNode::FCompare());

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
	const FPostProcessMaterialChain& Materials)
{
	FScreenPassTexture Outputs = InputsTemplate.GetInput(EPostProcessMaterialInput::SceneColor);

	bool bFirstMaterialInChain = true;
	for (const UMaterialInterface* MaterialInterface : Materials)
	{
		FPostProcessMaterialInputs Inputs = InputsTemplate;
		Inputs.SetInput(EPostProcessMaterialInput::SceneColor, Outputs);
		
		// Only the first material in the chain needs to decode the input color
		Inputs.bMetalMSAAHDRDecode = Inputs.bMetalMSAAHDRDecode && bFirstMaterialInChain;
		bFirstMaterialInChain = false;

		// Certain inputs are only respected by the final post process material in the chain.
		if (MaterialInterface != Materials.Last())
		{
			Inputs.OverrideOutput = FScreenPassRenderTarget();
		}

		Outputs = AddPostProcessMaterialPass(GraphBuilder, View, Inputs, MaterialInterface);
	}

	return Outputs;
}

extern void AddDumpToColorArrayPass(FRDGBuilder& GraphBuilder, FScreenPassTexture Input, TArray<FColor>* OutputColorArray, FIntPoint* OutputExtents);

bool IsHighResolutionScreenshotMaskEnabled(const FViewInfo& View)
{
	return View.Family->EngineShowFlags.HighResScreenshotMask || View.FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial;
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
		PassInputs.SetInput(EPostProcessMaterialInput::SceneColor, Output);
		PassInputs.SceneTextures = Inputs.SceneTextures;

		Output = AddPostProcessMaterialPass(GraphBuilder, View, PassInputs, Inputs.Material);
	}

	if (PassSequence.IsEnabled(EPass::MaskMaterial))
	{
		PassSequence.AcceptPass(EPass::MaskMaterial);

		FPostProcessMaterialInputs PassInputs;
		PassInputs.SetInput(EPostProcessMaterialInput::SceneColor, Output);
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
		PassInputs.SetInput(EPostProcessMaterialInput::SceneColor, Output);
		PassInputs.SceneTextures = Inputs.SceneTextures;

		Output = AddPostProcessMaterialPass(GraphBuilder, View, PassInputs, Inputs.CaptureRegionMaterial);
	}

	return Output;
}
