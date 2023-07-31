// Copyright Epic Games, Inc. All Rights Reserved.

#include "MobileFSRUpscaler.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "PostProcess/PostProcessing.h"

#define A_CPU 1
// ffx_a.ush will override these definitions
#ifdef A_STATIC
#undef A_STATIC
#endif
#ifdef A_RESTRICT
#undef A_RESTRICT
#endif
#include "../Shaders/Private/ffx_a.ush"
#include "../Shaders/Private/ffx_fsr1.ush"


//////////////////////////////////////////////////////////////////////////
//! FSR
//////////////////////////////////////////////////////////////////////////

// permutation domains
class FMobileFSR_UseFP16Dim : SHADER_PERMUTATION_BOOL("ENABLE_FP16");
class FMobileRCAS_DenoiseDim : SHADER_PERMUTATION_BOOL("USE_RCAS_DENOISE");

BEGIN_SHADER_PARAMETER_STRUCT(FMobileFSRPassParameters, )
SHADER_PARAMETER(FUintVector4, Const0)
SHADER_PARAMETER(FUintVector4, Const1)
SHADER_PARAMETER(FUintVector4, Const2)
SHADER_PARAMETER(FUintVector4, Const3)
SHADER_PARAMETER(FVector2f, VPColor_ExtentInverse)
SHADER_PARAMETER(FVector2f, VPColor_ViewportMin)
SHADER_PARAMETER_SAMPLER(SamplerState, samLinearClamp)
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FMobileRCASPassParameters, )
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
SHADER_PARAMETER(FUintVector4, Const0)
SHADER_PARAMETER(FVector2f, VPColor_ExtentInverse)
END_SHADER_PARAMETER_STRUCT()

static float GMobileFSRCASSharpness = 0.2; // 0.2 stops = 1 / (2^N) ~= 0.87 in linear [0-1] sharpening of CAS
static FAutoConsoleVariableRef CVarMobileFSRCASSharpness(
	TEXT("r.Mobile.FSR.RCAS.Sharpness"),
	GMobileFSRCASSharpness,
	TEXT("FSR RCAS Sharpness in stops (0: sharpest, 1: 1/2 as sharp, 2: 1/4 as sharp, 3: 1/8 as sharp, etc.). A value of 0.2 would correspond to a ~0.87 sharpness in [0-1] linear scale"),
	ECVF_Default);

static int32 GMobileFSRCASDenoise = 0;
static FAutoConsoleVariableRef CVarMobileFSRCASDenoise(
	TEXT("r.Mobile.FSR.RCAS.Denoise"),
	GMobileFSRCASDenoise,
	TEXT("FSR RCAS Denoise support for grainy input such as dithered images or input with custom film grain effects applied prior to FSR. 1:On, 0:Off"),
	ECVF_Default);

static int32 GMobileFSRDisableCompute = 0;
static FAutoConsoleVariableRef CVarMobileFSRDisableCompute(
	TEXT("r.Mobile.FSR.DisableCompute"),
	GMobileFSRDisableCompute,
	TEXT("FSR Disable compute shaders for FSR (Recommended for Adreno). 1: Compute Disabled, 0: Compute Enabled (default)"),
	ECVF_Default);

///
/// FSR COMPUTE SHADER
///
class FMobileFSRCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMobileFSRCS);
	SHADER_USE_PARAMETER_STRUCT(FMobileFSRCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<FMobileFSR_UseFP16Dim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMobileFSRPassParameters, FSR)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FMobileFSR_UseFP16Dim>())
		{
			if (IsSimulatedPlatform(Parameters.Platform) || !IsDxcEnabledForPlatform(Parameters.Platform))
			{
				return false;
			}
		}

		return IsMobilePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 1);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 64);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEZ"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileFSRCS, "/Plugin/MobileFSR/Private/PostProcessMobileFFX_FSR.usf", "MainCS", SF_Compute);

class FMobileFSRPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMobileFSRPS);
	SHADER_USE_PARAMETER_STRUCT(FMobileFSRPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<FMobileFSR_UseFP16Dim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMobileFSRPassParameters, FSR)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FMobileFSR_UseFP16Dim>())
		{
			if (IsSimulatedPlatform(Parameters.Platform) || !IsDxcEnabledForPlatform(Parameters.Platform))
			{
				return false;
			}
		}

		return IsMobilePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileFSRPS, "/Plugin/MobileFSR/Private/PostProcessMobileFFX_FSR.usf", "MainPS", SF_Pixel);

FScreenPassTexture AddEASUPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const ISpatialUpscaler::FInputs& PassInputs)
{
	FScreenPassTexture OutputTexture;
	bool bUseFP16 = !IsSimulatedPlatform(View.GetShaderPlatform()) && IsDxcEnabledForPlatform(View.GetShaderPlatform());

	FRDGTextureDesc FSROutputTextureDesc = PassInputs.SceneColor.Texture->Desc;
	FSROutputTextureDesc.Reset();
	FSROutputTextureDesc.Extent = View.UnscaledViewRect.Max;
	FSROutputTextureDesc.ClearValue = FClearValueBinding::Black;
	FSROutputTextureDesc.Flags = TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable;

	if (PassInputs.OverrideOutput.IsValid())
	{
		OutputTexture = PassInputs.OverrideOutput;
	}
	else
	{
		OutputTexture = FScreenPassTexture(PassInputs.SceneColor);
		OutputTexture.Texture = GraphBuilder.CreateTexture(FSROutputTextureDesc, TEXT("FSR-Output"), ERDGTextureFlags::MultiFrame);
	}

	OutputTexture.ViewRect = View.UnscaledViewRect;
	FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(OutputTexture);
	FScreenPassTextureViewport InputViewport = FScreenPassTextureViewport(PassInputs.SceneColor);
	FScreenPassTextureViewportParameters PassOutputViewportParams = GetScreenPassTextureViewportParameters(OutputViewport);

	AU1 const0[4];
	AU1 const1[4];
	AU1 const2[4];
	AU1 const3[4]; // Configure FSR

	FsrEasuConOffset(const0, const1, const2, const3,
		static_cast<AF1>(InputViewport.Rect.Width())
		, static_cast<AF1>(InputViewport.Rect.Height()) // current frame render resolution
		, static_cast<AF1>(PassInputs.SceneColor.Texture->Desc.Extent.X)
		, static_cast<AF1>(PassInputs.SceneColor.Texture->Desc.Extent.Y) // input container resolution (for DRS)
		, static_cast<AF1>(OutputViewport.Rect.Width())
		, static_cast<AF1>(OutputViewport.Rect.Height()) // upscaled-to resolution
		, static_cast<AF1>(InputViewport.Rect.Min.X)
		, static_cast<AF1>(InputViewport.Rect.Min.Y)
	);

	const bool bOutputSupportsUAV = (OutputTexture.Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV;

	if (!bOutputSupportsUAV || GMobileFSRDisableCompute)
	{
		FMobileFSRPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMobileFSRPS::FParameters>();
		for (int i = 0; i < 4; i++)
		{
			PassParameters->FSR.Const0[i] = const0[i];
			PassParameters->FSR.Const1[i] = const1[i];
			PassParameters->FSR.Const2[i] = const2[i];
			PassParameters->FSR.Const3[i] = const3[i];
		}
		PassParameters->FSR.InputTexture = PassInputs.SceneColor.Texture;
		PassParameters->FSR.samLinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->FSR.VPColor_ExtentInverse = PassOutputViewportParams.ExtentInverse;
		PassParameters->FSR.VPColor_ViewportMin = PassOutputViewportParams.ViewportMin;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture.Texture, ERenderTargetLoadAction::EClear);

		FMobileFSRPS::FPermutationDomain PSPermutationVector;
		PSPermutationVector.Set<FMobileFSR_UseFP16Dim>(bUseFP16);

		TShaderMapRef<FMobileFSRPS> PixelShader(View.ShaderMap, PSPermutationVector);

		AddDrawScreenPass(GraphBuilder,
			RDG_EVENT_NAME("FSR/Upscale %dx%d -> %dx%d (PS)"
				, InputViewport.Rect.Width(), InputViewport.Rect.Height()
				, OutputViewport.Rect.Width(), OutputViewport.Rect.Height()),
			View, OutputViewport, InputViewport,
			PixelShader, PassParameters,
			EScreenPassDrawFlags::None
		);
	}
	// CS
	else
	{
		FMobileFSRCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMobileFSRCS::FParameters>();

		for (int i = 0; i < 4; i++)
		{
			PassParameters->FSR.Const0[i] = const0[i];
			PassParameters->FSR.Const1[i] = const1[i];
			PassParameters->FSR.Const2[i] = const2[i];
			PassParameters->FSR.Const3[i] = const3[i];
		}
		PassParameters->FSR.InputTexture = PassInputs.SceneColor.Texture;
		PassParameters->FSR.samLinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->FSR.VPColor_ExtentInverse = PassOutputViewportParams.ExtentInverse;
		PassParameters->FSR.VPColor_ViewportMin = PassOutputViewportParams.ViewportMin;
		PassParameters->OutputTexture = GraphBuilder.CreateUAV(OutputTexture.Texture);

		FMobileFSRCS::FPermutationDomain CSPermutationVector;
		CSPermutationVector.Set<FMobileFSR_UseFP16Dim>(bUseFP16);

		TShaderMapRef<FMobileFSRCS> ComputeShaderFSR(View.ShaderMap, CSPermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FSR/Upscale %dx%d -> %dx%d (CS)"
				, InputViewport.Rect.Width(), InputViewport.Rect.Height()
				, OutputViewport.Rect.Width(), OutputViewport.Rect.Height()),
			ComputeShaderFSR,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(OutputViewport.Rect.Size(), 16));
	}

	return MoveTemp(OutputTexture);
}

///
/// RCAS COMPUTE SHADER
///
class FMobileRCASCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMobileRCASCS);
	SHADER_USE_PARAMETER_STRUCT(FMobileRCASCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<FMobileFSR_UseFP16Dim, FMobileRCAS_DenoiseDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMobileRCASPassParameters, RCAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FMobileFSR_UseFP16Dim>())
		{
			if (IsSimulatedPlatform(Parameters.Platform) || !IsDxcEnabledForPlatform(Parameters.Platform))
			{
				return false;
			}
		}

		return IsMobilePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 1);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 64);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEZ"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileRCASCS, "/Plugin/MobileFSR/Private/PostProcessMobileFFX_RCAS.usf", "MainCS", SF_Compute);


///
/// RCAS PIXEL SHADER
///
class FMobileRCASPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMobileRCASPS);
	SHADER_USE_PARAMETER_STRUCT(FMobileRCASPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<FMobileFSR_UseFP16Dim, FMobileRCAS_DenoiseDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMobileRCASPassParameters, RCAS)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FMobileFSR_UseFP16Dim>())
		{
			if (IsSimulatedPlatform(Parameters.Platform) || !IsDxcEnabledForPlatform(Parameters.Platform))
			{
				return false;
			}
		}

		return IsMobilePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileRCASPS, "/Plugin/MobileFSR/Private/PostProcessMobileFFX_RCAS.usf", "MainPS", SF_Pixel);

FScreenPassTexture AddCASPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const ISpatialUpscaler::FInputs& PassInputs)
{
	FScreenPassTexture OutputTexture;
	bool bUseFP16 = !IsSimulatedPlatform(View.GetShaderPlatform()) && IsDxcEnabledForPlatform(View.GetShaderPlatform());

	if (PassInputs.OverrideOutput.IsValid())
	{
		OutputTexture = PassInputs.OverrideOutput;
	}
	else
	{
		FRDGTextureDesc RCASOutputTextureDesc = PassInputs.SceneColor.Texture->Desc;

		OutputTexture = FScreenPassTexture(PassInputs.SceneColor);
		OutputTexture.Texture = GraphBuilder.CreateTexture(RCASOutputTextureDesc, TEXT("RCAS-Output"), ERDGTextureFlags::MultiFrame);
	}

	FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(OutputTexture);
	FScreenPassTextureViewportParameters PassOutputViewportParams = GetScreenPassTextureViewportParameters(OutputViewport);

	AU1 const0[4]; // Configure FSR
	FsrRcasCon(const0, GMobileFSRCASSharpness);

	const bool bOutputSupportsUAV = (OutputTexture.Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV;

	if (!bOutputSupportsUAV || GMobileFSRDisableCompute)
	{
		FMobileRCASPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMobileRCASPS::FParameters>();

		// set pass inputs
		for (int i = 0; i < 4; i++) { PassParameters->RCAS.Const0[i] = const0[i]; }
		PassParameters->RCAS.InputTexture = PassInputs.SceneColor.Texture;
		PassParameters->RCAS.VPColor_ExtentInverse = PassOutputViewportParams.ExtentInverse;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture.Texture, ERenderTargetLoadAction::EClear);

		// grab shaders
		FMobileRCASPS::FPermutationDomain PSPermutationVector;
		PSPermutationVector.Set<FMobileFSR_UseFP16Dim>(bUseFP16);
		PSPermutationVector.Set<FMobileRCAS_DenoiseDim>(GMobileFSRCASDenoise > 0);

		TShaderMapRef<FMobileRCASPS> PixelShader(View.ShaderMap, PSPermutationVector);

		AddDrawScreenPass(GraphBuilder,
			RDG_EVENT_NAME("FSR/RCAS Sharpness=%.2f(PS)"
				, GMobileFSRCASSharpness),
			View, OutputViewport, OutputViewport,
			PixelShader, PassParameters,
			EScreenPassDrawFlags::None
		);
	}
	// CS
	else
	{
		FMobileRCASCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMobileRCASCS::FParameters>();

		for (int i = 0; i < 4; i++)
		{
			PassParameters->RCAS.Const0[i] = const0[i];
		}
		PassParameters->RCAS.InputTexture = PassInputs.SceneColor.Texture;
		PassParameters->RCAS.VPColor_ExtentInverse = PassOutputViewportParams.ExtentInverse;
		PassParameters->OutputTexture = GraphBuilder.CreateUAV(OutputTexture.Texture);

		FMobileRCASCS::FPermutationDomain CSPermutationVector;
		CSPermutationVector.Set<FMobileFSR_UseFP16Dim>(bUseFP16);
		CSPermutationVector.Set<FMobileRCAS_DenoiseDim>(GMobileFSRCASDenoise > 0);

		TShaderMapRef<FMobileRCASCS> ComputeShaderRCASPass(View.ShaderMap, CSPermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FSR/RCAS Sharpness=%.2f (CS)", GMobileFSRCASSharpness),
			ComputeShaderRCASPass,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(OutputViewport.Rect.Size(), 16));
	}

	return MoveTemp(OutputTexture);
}

DECLARE_GPU_STAT(MobileFSRUpscaler)

FMobileFSRUpscaler::FMobileFSRUpscaler(bool bInIsEASUPass) : bIsEASUPass(bInIsEASUPass)
{
}

ISpatialUpscaler* FMobileFSRUpscaler::Fork_GameThread(const class FSceneViewFamily& ViewFamily) const
{
	return new FMobileFSRUpscaler(bIsEASUPass);
}

FScreenPassTexture FMobileFSRUpscaler::AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, MobileFSRUpscaler);
	
	FScreenPassTexture SceneColor;

	if (bIsEASUPass)
	{
		SceneColor = AddEASUPass(GraphBuilder, View, PassInputs);
	}
	else
	{
		SceneColor = AddCASPass(GraphBuilder, View, PassInputs);
	}

	return MoveTemp(SceneColor);

}