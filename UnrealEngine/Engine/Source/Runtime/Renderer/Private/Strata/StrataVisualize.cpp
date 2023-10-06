// Copyright Epic Games, Inc. All Rights Reserved.

#include "Strata.h"
#include "SceneTextureParameters.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "ShaderCompiler.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"
#include "IndirectLightRendering.h"
#include "StrataVisualizationData.h"

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

class FVisualizeMaterialCountPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeMaterialCountPS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeMaterialCountPS, FGlobalShader);

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
		OutEnvironment.SetDefine(TEXT("SHADER_MATERIALCOUNT"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVisualizeMaterialCountPS, "/Engine/Private/Strata/StrataVisualize.usf", "VisualizeMaterialPS", SF_Pixel);


class FStrataSystemInfoCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataSystemInfoCS);
	SHADER_USE_PARAMETER_STRUCT(FStrataSystemInfoCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, bAdvancedDebugEnabled)
		SHADER_PARAMETER(uint32, bEnergyConservation)
		SHADER_PARAMETER(uint32, bEnergyPreservation)
		SHADER_PARAMETER(uint32, bDbufferPass)
		SHADER_PARAMETER(uint32, ClassificationCMask)
		SHADER_PARAMETER(uint32, ClassificationAsync)
		SHADER_PARAMETER(uint32, Classification8bits)
		SHADER_PARAMETER(uint32, bRoughRefraction)
		SHADER_PARAMETER(uint32, bTileOverflowUseMaterialData)
		SHADER_PARAMETER(uint32, ProjectMaxBytesPerPixel)
		SHADER_PARAMETER(uint32, ViewsMaxBytesPerPixel)
		SHADER_PARAMETER(uint32, MaterialBufferAllocationInBytes)
		SHADER_PARAMETER(uint32, MaterialBufferAllocationMode)
		SHADER_PARAMETER(float, TileOverflowRatio)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ClassificationTileDrawIndirectBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
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
		OutEnvironment.SetDefine(TEXT("SHADER_SYSTEMINFO"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FStrataSystemInfoCS, "/Engine/Private/Strata/StrataVisualize.usf", "MainCS", SF_Compute);

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
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FReflectionUniformParameters, ReflectionStruct)
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

static void AddVisualizeMaterialPropertiesPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, EShaderPlatform Platform)
{
	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);
	FRDGBufferUAVRef PrintOffsetBufferUAV = nullptr;

	FRDGBufferRef PrintOffsetBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, 2), TEXT("Substrate.DebugPrintPositionOffset"));
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
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Substrate::VisualizeMaterial(Print, BSDF=%d)", BSDFIndex), ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}
}

static void AddVisualizeMaterialCountPasses(FRDGBuilder & GraphBuilder, const FViewInfo & View, FScreenPassTexture & ScreenPassSceneColor, EShaderPlatform Platform, uint32 ViewMode)
{
	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);

	FRDGTextureRef SceneColorTexture = ScreenPassSceneColor.Texture;
	FRHIBlendState* PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();

	FVisualizeMaterialCountPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeMaterialCountPS::FParameters>();
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->ViewMode = FMath::Clamp(ViewMode, 2, 3);
	PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
	PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, View);
	PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);
	ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

	FVisualizeMaterialCountPS::FPermutationDomain PermutationVector;
	TShaderMapRef<FVisualizeMaterialCountPS> PixelShader(View.ShaderMap, PermutationVector);

	FPixelShaderUtils::AddFullscreenPass<FVisualizeMaterialCountPS>(GraphBuilder, View.ShaderMap, RDG_EVENT_NAME("Substrate::VisualizeMaterial(Draw)"), PixelShader, PassParameters, ScreenPassSceneColor.ViewRect, PreMultipliedColorTransmittanceBlend);
}

float GetStrataTileOverflowRatio(const FViewInfo& View);
bool IsClassificationAsync();
bool SupportsCMask(const FStaticShaderPlatform InPlatform);
bool DoesStrataTileOverflowUseMaterialData();
uint32 GetMaterialBufferAllocationMode();

static void AddVisualizeSystemInfoPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, EShaderPlatform Platform)
{
	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);

	const FRDGTextureDesc MaterialBufferDesc = View.StrataViewData.SceneData->MaterialTextureArray->Desc;

	FStrataSystemInfoCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStrataSystemInfoCS::FParameters>();
	PassParameters->bAdvancedDebugEnabled = IsAdvancedVisualizationEnabled() ? 1u : 0u;
	PassParameters->bEnergyConservation = View.ViewState ? View.ViewState->ShadingEnergyConservationData.bEnergyConservation : false;;
	PassParameters->bEnergyPreservation = View.ViewState ? View.ViewState->ShadingEnergyConservationData.bEnergyPreservation : false;;
	PassParameters->bDbufferPass = IsDBufferPassEnabled(View.GetShaderPlatform()) ? 1 : 0;
	PassParameters->ClassificationCMask = SupportsCMask(View.GetShaderPlatform()) ? 1 : 0;
	PassParameters->ClassificationAsync = IsClassificationAsync() ? 1 : 0;
	PassParameters->Classification8bits = Is8bitTileCoordEnabled() ? 1 : 0;
	PassParameters->TileOverflowRatio = GetStrataTileOverflowRatio(View);
	PassParameters->bTileOverflowUseMaterialData = DoesStrataTileOverflowUseMaterialData() ? 1 : 0;
	PassParameters->bRoughRefraction = IsOpaqueRoughRefractionEnabled() ? 1 : 0;
	PassParameters->ClassificationTileDrawIndirectBuffer = GraphBuilder.CreateSRV(View.StrataViewData.ClassificationTileDrawIndirectBuffer, PF_R32_UINT);
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
	PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, View);
	PassParameters->ProjectMaxBytesPerPixel = GetBytePerPixel(View.GetShaderPlatform());
	PassParameters->ViewsMaxBytesPerPixel = View.StrataViewData.SceneData->ViewsMaxBytesPerPixel;
	PassParameters->MaterialBufferAllocationMode = GetMaterialBufferAllocationMode();
	PassParameters->MaterialBufferAllocationInBytes = MaterialBufferDesc.Extent.X * MaterialBufferDesc.Extent.Y * MaterialBufferDesc.ArraySize * sizeof(uint32);
	ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

	TShaderMapRef<FStrataSystemInfoCS> ComputeShader(View.ShaderMap);
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Substrate::VisualizeSystemInfo"), ComputeShader, PassParameters, FIntVector(1, 1, 1));
}

// Draw each material layer independently
static void AddVisualizeAdvancedMaterialPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, EShaderPlatform Platform)
{
	if (!IsAdvancedVisualizationEnabled())
	{
		return;
	}

	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);

	FRDGTextureRef SceneColorTexture = ScreenPassSceneColor.Texture;
	FRHIBlendState* PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
	{
		FMaterialDebugStrataTreeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialDebugStrataTreeCS::FParameters>();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

		TShaderMapRef<FMaterialDebugStrataTreeCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Substrate::StrataAdvancedVisualization(Print)"), ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}

	{
		FMaterialDebugStrataTreePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialDebugStrataTreePS::FParameters>();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);
		PassParameters->ReflectionStruct = CreateReflectionUniformBuffer(GraphBuilder, View);
		PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
		PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);

		const float DynamicBentNormalAO = 0.0f;
		FSkyLightSceneProxy* NullSkyLight = nullptr;
		PassParameters->SkyDiffuseLighting = GetSkyDiffuseLightingParameters(NullSkyLight, DynamicBentNormalAO);

		FMaterialDebugStrataTreePS::FPermutationDomain PermutationVector;
		TShaderMapRef<FMaterialDebugStrataTreePS> PixelShader(View.ShaderMap, PermutationVector);

		FPixelShaderUtils::AddFullscreenPass<FMaterialDebugStrataTreePS>(GraphBuilder, View.ShaderMap, RDG_EVENT_NAME("Substrate::StrataAdvancedVisualization(Draw)"), PixelShader, PassParameters, ScreenPassSceneColor.ViewRect, PreMultipliedColorTransmittanceBlend);
	}
}

bool ShouldRenderStrataRoughRefractionRnD();
void StrataRoughRefractionRnD(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor);

static FStrataViewMode GetStrataVisualizeMode(const FViewInfo & View)
{
	FStrataViewMode Out = FStrataViewMode::None;
	if (IsStrataEnabled() && StrataDebugVisualizationCanRunOnPlatform(View.GetShaderPlatform()))
	{
		// Variable defined in StrataVisualizationData.h/.cpp
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(FStrataVisualizationData::GetVisualizeConsoleCommandName());
		const uint32 ViewMode = CVar && CVar->AsVariableInt() ? CVar->AsVariableInt()->GetValueOnRenderThread() : 0;
		switch (ViewMode)
		{
			case 1: return FStrataViewMode::MaterialProperties;
			case 2: return FStrataViewMode::MaterialCount;
			case 3: return FStrataViewMode::AdvancedMaterialProperties;
			case 4: return FStrataViewMode::MaterialClassification;
			case 5: return FStrataViewMode::DecalClassification;
			case 6: return FStrataViewMode::RoughRefractionClassification;
			case 7: return FStrataViewMode::StrataInfo;
			case 8: return FStrataViewMode::MaterialByteCount;
		}

		const FStrataVisualizationData& VisualizationData = GetStrataVisualizationData();
		if (View.Family && View.Family->EngineShowFlags.VisualizeSubstrate)
		{
			Out = VisualizationData.GetViewMode(View.CurrentStrataVisualizationMode);
		}
	}
	return Out;
}

bool ShouldRenderStrataDebugPasses(const FViewInfo& View)
{
	return GetStrataVisualizeMode(View) != FStrataViewMode::None || ShouldRenderStrataRoughRefractionRnD();
}

FScreenPassTexture AddStrataDebugPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor)
{
	check(IsStrataEnabled());

	const FStrataViewMode DebugMode = GetStrataVisualizeMode(View);
	if (DebugMode != FStrataViewMode::None)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Substrate::VisualizeMaterial");

		const bool bDebugPass = true;
		if (DebugMode == FStrataViewMode::MaterialProperties)
		{
			AddVisualizeMaterialPropertiesPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform());
		}
		if (DebugMode == FStrataViewMode::MaterialCount)
		{
			AddVisualizeMaterialCountPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform(), 2);
		}
		if (DebugMode == FStrataViewMode::MaterialByteCount)
		{
			AddVisualizeMaterialCountPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform(), 3);
		}
		if (DebugMode == FStrataViewMode::AdvancedMaterialProperties)
		{
			AddVisualizeAdvancedMaterialPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform());
		}
		else if (DebugMode == FStrataViewMode::StrataInfo)
		{
			AddVisualizeSystemInfoPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform());
		}
		else if (DebugMode == FStrataViewMode::DecalClassification)
		{
			if (IsDBufferPassEnabled(View.GetShaderPlatform()))
			{
				AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EDecalSimple, bDebugPass);
				AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EDecalSingle, bDebugPass);
				AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EDecalComplex, bDebugPass);
			}
		}
		else if (DebugMode == FStrataViewMode::RoughRefractionClassification)
		{
			if (IsOpaqueRoughRefractionEnabled())
			{
				AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EOpaqueRoughRefraction, bDebugPass);
				AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EOpaqueRoughRefractionSSSWithout, bDebugPass);
			}
		}
		else if (DebugMode == FStrataViewMode::MaterialClassification)
		{
			if (GetStrataUsesComplexSpecialPath(View))
			{
				AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EComplexSpecial, bDebugPass);
			}
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::EComplex, bDebugPass);
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::ESingle, bDebugPass);
			AddStrataInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, EStrataTileType::ESimple, bDebugPass);
		}
	}

	StrataRoughRefractionRnD(GraphBuilder, View, ScreenPassSceneColor);

	return MoveTemp(ScreenPassSceneColor);
}

} // namespace Strata
