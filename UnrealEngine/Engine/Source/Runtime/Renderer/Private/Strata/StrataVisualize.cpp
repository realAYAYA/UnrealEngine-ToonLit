// Copyright Epic Games, Inc. All Rights Reserved.

#include "Strata.h"
#include "SceneTextureParameters.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "ShaderCompiler.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"
#include "IndirectLightRendering.h"


static TAutoConsoleVariable<int32> CVarStrataClassificationDebug(
	TEXT("r.Strata.Classification.Debug"),
	0,
	TEXT("Enable strata classification visualization: 1 shows simple material tiles in green and complex material tiles in red."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarStrataDebugVisualizeMode(
	TEXT("r.Strata.Debug.VisualizeMode"),
	1,
	TEXT("Strata debug view mode."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarStrataDebugAdvancedVisualizationShaders(
	TEXT("r.Strata.Debug.AdvancedVisualizationShaders"),
	0,
	TEXT("Enable advanced strata material debug visualization shaders. Base pass shaders can output such advanced data."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

namespace Strata
{
// Forward declarations
void AddStrataInternalClassificationTilePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FRDGTextureRef* DepthTexture,
	const FRDGTextureRef* ColorTexture,
	EStrataTileType TileMaterialType,
	const bool bDebug);

bool IsStrataAdvancedVisualizationShadersEnabled()
{
	return CVarStrataDebugAdvancedVisualizationShaders.GetValueOnRenderThread() > 0;
}

static bool StrataDebugVisualizationCanRunOnPlatform(EShaderPlatform Platform)
{
	// On some consoles, this ALU heavy shader (and with optimisation disables for the sake of low compilation time) would spill registers. So only keep it for the editor.
	return GetMaxSupportedFeatureLevel(Platform) >= ERHIFeatureLevel::SM5 && IsPCPlatform(Platform);
}

class FMaterialPrintInfoCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMaterialPrintInfoCS);
	SHADER_USE_PARAMETER_STRUCT(FMaterialPrintInfoCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, BSDFIndex)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWPositionOffsetBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Strata::IsStrataEnabled() && StrataDebugVisualizationCanRunOnPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_MATERIALPRINT"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMaterialPrintInfoCS, "/Engine/Private/Strata/StrataVisualize.usf", "MaterialPrintInfoCS", SF_Compute);

class FVisualizeMaterialPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeMaterialPS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeMaterialPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, ViewMode)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Strata::IsStrataEnabled() && StrataDebugVisualizationCanRunOnPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_MATERIALVISUALIZE"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVisualizeMaterialPS, "/Engine/Private/Strata/StrataVisualize.usf", "VisualizeMaterialPS", SF_Pixel);

class FMaterialDebugStrataTreeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMaterialDebugStrataTreeCS);
	SHADER_USE_PARAMETER_STRUCT(FMaterialDebugStrataTreeCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Strata::IsStrataEnabled() && StrataDebugVisualizationCanRunOnPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_DEBUGSTRATATREE_CS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMaterialDebugStrataTreeCS, "/Engine/Private/Strata/StrataVisualize.usf", "MaterialDebugStrataTreeCS", SF_Compute);

class FMaterialDebugStrataTreePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMaterialDebugStrataTreePS);
	SHADER_USE_PARAMETER_STRUCT(FMaterialDebugStrataTreePS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		SHADER_PARAMETER_STRUCT_REF(FReflectionUniformParameters, ReflectionStruct)
		SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSkyDiffuseLightingParameters, SkyDiffuseLighting)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Strata::IsStrataEnabled() && StrataDebugVisualizationCanRunOnPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_DEBUGSTRATATREE_PS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMaterialDebugStrataTreePS, "/Engine/Private/Strata/StrataVisualize.usf", "MaterialDebugStrataTreePS", SF_Pixel);

static void AddVisualizeMaterialPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, EShaderPlatform Platform)
{
	FRDGTextureRef SceneColorTexture = ScreenPassSceneColor.Texture;
	FRHIBlendState* PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
	if (View.Family->EngineShowFlags.VisualizeStrataMaterial)
	{
		// Force ShaderPrint on.
		ShaderPrint::SetEnabled(true);
		ShaderPrint::RequestSpaceForLines(1024);
		ShaderPrint::RequestSpaceForCharacters(1024);

		const uint32 ViewMode = FMath::Max(0, CVarStrataDebugVisualizeMode.GetValueOnRenderThread());

		FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, View);

		// Print Material info
		if(ViewMode >= 1 && ViewMode != 3)
		{
			FRDGBufferUAVRef PrintOffsetBufferUAV = nullptr;

			FRDGBufferRef PrintOffsetBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, 2), TEXT("Strata.DebugPrintPositionOffset"));
			PrintOffsetBufferUAV = GraphBuilder.CreateUAV(PrintOffsetBuffer, PF_R32_UINT);
			AddClearUAVPass(GraphBuilder, PrintOffsetBufferUAV, 50u);
			const uint32 MaxBSDFCount = 8;

			for (uint32 BSDFIndex=0; BSDFIndex < MaxBSDFCount; ++BSDFIndex)
			{
				FMaterialPrintInfoCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialPrintInfoCS::FParameters>();
				PassParameters->BSDFIndex = BSDFIndex;
				PassParameters->RWPositionOffsetBuffer = PrintOffsetBufferUAV;
				PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
				PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
				PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, View);
				ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

				TShaderMapRef<FMaterialPrintInfoCS> ComputeShader(View.ShaderMap);
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Strata::VisualizeMaterial(Print, BSDF=%d)", BSDFIndex), ComputeShader, PassParameters, FIntVector(1, 1, 1));
			}
		}

		// Draw material debug
		if (ViewMode >= 2 && ViewMode != 3)
		{
			FVisualizeMaterialPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeMaterialPS::FParameters>();
			PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
			PassParameters->ViewMode = ViewMode;
			PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
			PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, View);
			PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

			FVisualizeMaterialPS::FPermutationDomain PermutationVector;
			TShaderMapRef<FVisualizeMaterialPS> PixelShader(View.ShaderMap, PermutationVector);

			FPixelShaderUtils::AddFullscreenPass<FVisualizeMaterialPS>(GraphBuilder, View.ShaderMap, RDG_EVENT_NAME("Strata::VisualizeMaterial(Draw)"), PixelShader, PassParameters, ScreenPassSceneColor.ViewRect, PreMultipliedColorTransmittanceBlend);
		}

		// Draw each material layer independently
		if (ViewMode == 3)
		{
			if (IsStrataAdvancedVisualizationShadersEnabled())
			{
				{
					FMaterialDebugStrataTreeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialDebugStrataTreeCS::FParameters>();
					PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
					PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
					ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

					TShaderMapRef<FMaterialDebugStrataTreeCS> ComputeShader(View.ShaderMap);
					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Strata::StrataAdvancedVisualization(Print)"), ComputeShader, PassParameters, FIntVector(1, 1, 1));
				}

				{
					FMaterialDebugStrataTreePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialDebugStrataTreePS::FParameters>();
					PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
					PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
					PassParameters->ReflectionStruct = CreateReflectionUniformBuffer(View, UniformBuffer_SingleFrame);
					PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
					PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
					PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);

					const float DynamicBentNormalAO = 0.0f;
					FSkyLightSceneProxy* NullSkyLight = nullptr;
					PassParameters->SkyDiffuseLighting = GetSkyDiffuseLightingParameters(NullSkyLight, DynamicBentNormalAO);

					FMaterialDebugStrataTreePS::FPermutationDomain PermutationVector;
					TShaderMapRef<FMaterialDebugStrataTreePS> PixelShader(View.ShaderMap, PermutationVector);

					FPixelShaderUtils::AddFullscreenPass<FMaterialDebugStrataTreePS>(GraphBuilder, View.ShaderMap, RDG_EVENT_NAME("Strata::StrataAdvancedVisualization(Draw)"), PixelShader, PassParameters, ScreenPassSceneColor.ViewRect, PreMultipliedColorTransmittanceBlend);
				}
			}
			else
			{
				// TODO warn the user that this mode is not available with delegate SceneRenderer.OnGetOnScreenMessages.AddLambda([](FScreenMessageWriter& ScreenMessageWriter)->void
			}
		}
	}
}

bool ShouldRenderStrataDebugPasses(const FViewInfo& View)
{
	return IsStrataEnabled() && (
		(StrataDebugVisualizationCanRunOnPlatform(View.GetShaderPlatform()) && ( (View.Family && View.Family->EngineShowFlags.VisualizeStrataMaterial) || CVarStrataClassificationDebug.GetValueOnAnyThread() > 0 ))
		|| ShouldRenderStrataRoughRefractionRnD()
		);
}

FScreenPassTexture AddStrataDebugPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor)
{
	check(IsStrataEnabled());
	EShaderPlatform Platform = View.GetShaderPlatform();

	if (StrataDebugVisualizationCanRunOnPlatform(Platform))
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Strata::VisualizeMaterial");
		AddVisualizeMaterialPasses(GraphBuilder, View, ScreenPassSceneColor, Platform);
	}

	const int32 StrataClassificationDebug = CVarStrataClassificationDebug.GetValueOnAnyThread();
	if (StrataClassificationDebug > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Strata::VisualizeClassification");
		const bool bDebugPass = true;
		if (StrataClassificationDebug > 1)
		{
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EOpaqueRoughRefraction, bDebugPass);
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::ESSSWithoutOpaqueRoughRefraction, bDebugPass);
		}
		else
		{
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EComplex, bDebugPass);
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::ESingle, bDebugPass);
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::ESimple, bDebugPass);
		}
	}

	StrataRoughRefractionRnD(GraphBuilder, View, ScreenPassSceneColor);

	return MoveTemp(ScreenPassSceneColor);
}

} // namespace Strata
