// Copyright Epic Games, Inc. All Rights Reserved.

#include "Substrate.h"
#include "SceneTextureParameters.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "ShaderCompiler.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"
#include "IndirectLightRendering.h"
#include "SubstrateVisualizationData.h"

namespace Substrate
{
// Forward declarations
void AddSubstrateInternalClassificationTilePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FRDGTextureRef* DepthTexture,
	const FRDGTextureRef* ColorTexture,
	ESubstrateTileType TileMaterialType,
	const bool bDebug);

static bool IsSubstrateDebugVisualizationSupported(EShaderPlatform InPlatform, bool bIsEditorOnly, EShaderPermutationFlags Flags)
{
	return 
		Substrate::IsSubstrateEnabled() && 
		GetMaxSupportedFeatureLevel(InPlatform) >= ERHIFeatureLevel::SM5 &&
		!IsVulkanPlatform(InPlatform) && // Does not compile and fails to package games. Fixing this would require a deeper investigation.
		(bIsEditorOnly ? (IsPCPlatform(InPlatform) || EnumHasAllFlags(Flags, EShaderPermutationFlags::HasEditorOnlyData)) : true);
}

static bool SubstrateDebugVisualizationCanRunOnPlatform(EShaderPlatform InPlatform)
{
	return IsSubstrateDebugVisualizationSupported(InPlatform, false, EShaderPermutationFlags::None);
}

class FMaterialPrintInfoCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMaterialPrintInfoCS);
	SHADER_USE_PARAMETER_STRUCT(FMaterialPrintInfoCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, ClosureIndex)
		SHADER_PARAMETER(uint32, bOverrideCursorPosition)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWPositionOffsetBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool IsSupported(EShaderPlatform InPlatform, EShaderPermutationFlags InFlags=EShaderPermutationFlags::None)
	{
		return IsSubstrateDebugVisualizationSupported(InPlatform, false, InFlags);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsSupported(Parameters.Platform, Parameters.Flags);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_MATERIALPRINT"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMaterialPrintInfoCS, "/Engine/Private/Substrate/SubstrateVisualize.usf", "MaterialPrintInfoCS", SF_Compute);

class FVisualizeMaterialCountPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeMaterialCountPS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeMaterialCountPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, ViewMode)
		SHADER_PARAMETER(uint32, bOverrideCursorPosition)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool IsSupported(EShaderPlatform InPlatform, EShaderPermutationFlags InFlags=EShaderPermutationFlags::None)
	{
		return IsSubstrateDebugVisualizationSupported(InPlatform, false, InFlags);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsSupported(Parameters.Platform, Parameters.Flags);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_MATERIALCOUNT"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVisualizeMaterialCountPS, "/Engine/Private/Substrate/SubstrateVisualize.usf", "VisualizeMaterialPS", SF_Pixel);


class FSubstrateSystemInfoCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSubstrateSystemInfoCS);
	SHADER_USE_PARAMETER_STRUCT(FSubstrateSystemInfoCS, FGlobalShader);

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
		SHADER_PARAMETER(uint32, bUseClosureCountFromMaterialData)
		SHADER_PARAMETER(uint32, ProjectMaxBytesPerPixel)
		SHADER_PARAMETER(uint32, ProjectMaxClosurePerPixel)
		SHADER_PARAMETER(uint32, ViewsMaxBytesPerPixel)
		SHADER_PARAMETER(uint32, ViewsMaxClosurePerPixel)
		SHADER_PARAMETER(uint32, MaterialBufferAllocationInBytes)
		SHADER_PARAMETER(uint32, MaterialBufferAllocationMode)
		SHADER_PARAMETER(uint32, MaxClosureCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ClassificationTileDrawIndirectBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool IsSupported(EShaderPlatform InPlatform, EShaderPermutationFlags InFlags=EShaderPermutationFlags::None)
	{
		return IsSubstrateDebugVisualizationSupported(InPlatform, false, InFlags);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsSupported(Parameters.Platform, Parameters.Flags);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_SYSTEMINFO"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSubstrateSystemInfoCS, "/Engine/Private/Substrate/SubstrateVisualize.usf", "MainCS", SF_Compute);

class FMaterialDebugSubstrateTreeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMaterialDebugSubstrateTreeCS);
	SHADER_USE_PARAMETER_STRUCT(FMaterialDebugSubstrateTreeCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, bOverrideCursorPosition)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool IsSupported(EShaderPlatform InPlatform, EShaderPermutationFlags InFlags=EShaderPermutationFlags::None)
	{
		return IsSubstrateDebugVisualizationSupported(InPlatform, true, InFlags);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsSupported(Parameters.Platform, Parameters.Flags);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_DEBUGSUBSTRATETREE_CS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMaterialDebugSubstrateTreeCS, "/Engine/Private/Substrate/SubstrateVisualize.usf", "MaterialDebugSubstrateTreeCS", SF_Compute);

class FMaterialDebugSubstrateTreePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMaterialDebugSubstrateTreePS);
	SHADER_USE_PARAMETER_STRUCT(FMaterialDebugSubstrateTreePS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, bOverrideCursorPosition)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FReflectionUniformParameters, ReflectionStruct)
		SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSkyDiffuseLightingParameters, SkyDiffuseLighting)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool IsSupported(EShaderPlatform InPlatform, EShaderPermutationFlags InFlags=EShaderPermutationFlags::None)
	{
		return IsSubstrateDebugVisualizationSupported(InPlatform, true, InFlags);
	}
	
	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsSupported(Parameters.Platform, Parameters.Flags);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_DEBUGSUBSTRATETREE_PS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMaterialDebugSubstrateTreePS, "/Engine/Private/Substrate/SubstrateVisualize.usf", "MaterialDebugSubstrateTreePS", SF_Pixel);

static void AddVisualizeMaterialPropertiesPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, EShaderPlatform Platform)
{
	if (!FMaterialPrintInfoCS::IsSupported(Platform)) return;

	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);
	FRDGBufferUAVRef PrintOffsetBufferUAV = nullptr;

	FRDGBufferRef PrintOffsetBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, 2), TEXT("Substrate.DebugPrintPositionOffset"));
	PrintOffsetBufferUAV = GraphBuilder.CreateUAV(PrintOffsetBuffer, PF_R32_UINT);
	AddClearUAVPass(GraphBuilder, PrintOffsetBufferUAV, 50u);

	for (uint32 ClosureIndex=0; ClosureIndex < SUBSTRATE_MAX_CLOSURE_COUNT; ++ClosureIndex)
	{
		FMaterialPrintInfoCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialPrintInfoCS::FParameters>();
		PassParameters->ClosureIndex = ClosureIndex;
		PassParameters->bOverrideCursorPosition = WITH_EDITOR ? 0u : 1u;
		PassParameters->RWPositionOffsetBuffer = PrintOffsetBufferUAV;
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, View);
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

		TShaderMapRef<FMaterialPrintInfoCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Substrate::VisualizeMaterial(Print, Closure=%d)", ClosureIndex), ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}
}

static void AddVisualizeMaterialCountPasses(FRDGBuilder & GraphBuilder, const FViewInfo & View, FScreenPassTexture & ScreenPassSceneColor, EShaderPlatform Platform, uint32 ViewMode)
{
	if (!FVisualizeMaterialCountPS::IsSupported(Platform)) return;

	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);

	FRDGTextureRef SceneColorTexture = ScreenPassSceneColor.Texture;
	FRHIBlendState* PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();

	FVisualizeMaterialCountPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeMaterialCountPS::FParameters>();
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->ViewMode = FMath::Clamp(ViewMode, 2, 3);
	PassParameters->bOverrideCursorPosition = WITH_EDITOR ? 0u : 1u;
	PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
	PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, View);
	PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);
	ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

	FVisualizeMaterialCountPS::FPermutationDomain PermutationVector;
	TShaderMapRef<FVisualizeMaterialCountPS> PixelShader(View.ShaderMap, PermutationVector);

	FPixelShaderUtils::AddFullscreenPass<FVisualizeMaterialCountPS>(GraphBuilder, View.ShaderMap, RDG_EVENT_NAME("Substrate::VisualizeMaterial(Draw)"), PixelShader, PassParameters, ScreenPassSceneColor.ViewRect, PreMultipliedColorTransmittanceBlend);
}

bool IsClassificationAsync();
bool SupportsCMask(const FStaticShaderPlatform InPlatform);
bool UsesSubstrateClosureCountFromMaterialData();
uint32 GetMaterialBufferAllocationMode();
bool Is8bitTileCoordEnabled();

static void AddVisualizeSystemInfoPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, EShaderPlatform Platform)
{
	if (!FSubstrateSystemInfoCS::IsSupported(Platform)) return;

	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);

	const FRDGTextureDesc MaterialBufferDesc = View.SubstrateViewData.SceneData->MaterialTextureArray->Desc;

	FSubstrateSystemInfoCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSubstrateSystemInfoCS::FParameters>();
	PassParameters->bAdvancedDebugEnabled = IsAdvancedVisualizationEnabled() ? 1u : 0u;
	PassParameters->bEnergyConservation = View.ViewState ? View.ViewState->ShadingEnergyConservationData.bEnergyConservation : false;;
	PassParameters->bEnergyPreservation = View.ViewState ? View.ViewState->ShadingEnergyConservationData.bEnergyPreservation : false;;
	PassParameters->bDbufferPass = IsDBufferPassEnabled(View.GetShaderPlatform()) ? 1 : 0;
	PassParameters->ClassificationCMask = SupportsCMask(View.GetShaderPlatform()) ? 1 : 0;
	PassParameters->ClassificationAsync = IsClassificationAsync() ? 1 : 0;
	PassParameters->Classification8bits = Is8bitTileCoordEnabled() ? 1 : 0;
	PassParameters->MaxClosureCount = GetSubstrateMaxClosureCount(View);
	PassParameters->bUseClosureCountFromMaterialData = UsesSubstrateClosureCountFromMaterialData() ? 1 : 0;
	PassParameters->bRoughRefraction = IsOpaqueRoughRefractionEnabled() ? 1 : 0;
	PassParameters->ClassificationTileDrawIndirectBuffer = GraphBuilder.CreateSRV(View.SubstrateViewData.ClassificationTileDrawIndirectBuffer, PF_R32_UINT);
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
	PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, View);
	PassParameters->ProjectMaxBytesPerPixel = GetBytePerPixel(View.GetShaderPlatform());
	PassParameters->ProjectMaxClosurePerPixel = GetClosurePerPixel(View.GetShaderPlatform());
	PassParameters->ViewsMaxBytesPerPixel = View.SubstrateViewData.SceneData->ViewsMaxBytesPerPixel;
	PassParameters->ViewsMaxClosurePerPixel = View.SubstrateViewData.SceneData->ViewsMaxClosurePerPixel;
	PassParameters->MaterialBufferAllocationMode = GetMaterialBufferAllocationMode();
	PassParameters->MaterialBufferAllocationInBytes = MaterialBufferDesc.Extent.X * MaterialBufferDesc.Extent.Y * MaterialBufferDesc.ArraySize * sizeof(uint32);
	ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

	TShaderMapRef<FSubstrateSystemInfoCS> ComputeShader(View.ShaderMap);
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Substrate::VisualizeSystemInfo"), ComputeShader, PassParameters, FIntVector(1, 1, 1));
}

// Draw each material layer independently
static void AddVisualizeAdvancedMaterialPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, EShaderPlatform Platform)
{
	if (!IsAdvancedVisualizationEnabled() ||
		!FMaterialDebugSubstrateTreeCS::IsSupported(Platform) || 
		!FMaterialDebugSubstrateTreePS::IsSupported(Platform))
	{
		return;
	}

	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForCharacters(1024);

	FRDGTextureRef SceneColorTexture = ScreenPassSceneColor.Texture;
	FRHIBlendState* PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
	{
		FMaterialDebugSubstrateTreeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialDebugSubstrateTreeCS::FParameters>();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->bOverrideCursorPosition = WITH_EDITOR ? 0u : 1u;
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);

		TShaderMapRef<FMaterialDebugSubstrateTreeCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Substrate::SubstrateAdvancedVisualization(Print)"), ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}

	{
		FMaterialDebugSubstrateTreePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialDebugSubstrateTreePS::FParameters>();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->bOverrideCursorPosition = WITH_EDITOR ? 0u : 1u;
		PassParameters->ReflectionStruct = CreateReflectionUniformBuffer(GraphBuilder, View);
		PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
		PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);

		const float DynamicBentNormalAO = 0.0f;
		FSkyLightSceneProxy* NullSkyLight = nullptr;
		PassParameters->SkyDiffuseLighting = GetSkyDiffuseLightingParameters(NullSkyLight, DynamicBentNormalAO);

		FMaterialDebugSubstrateTreePS::FPermutationDomain PermutationVector;
		TShaderMapRef<FMaterialDebugSubstrateTreePS> PixelShader(View.ShaderMap, PermutationVector);

		FPixelShaderUtils::AddFullscreenPass<FMaterialDebugSubstrateTreePS>(GraphBuilder, View.ShaderMap, RDG_EVENT_NAME("Substrate::SubstrateAdvancedVisualization(Draw)"), PixelShader, PassParameters, ScreenPassSceneColor.ViewRect, PreMultipliedColorTransmittanceBlend);
	}
}

bool ShouldRenderSubstrateRoughRefractionRnD();
void SubstrateRoughRefractionRnD(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor);

static FSubstrateViewMode GetSubstrateVisualizeMode(const FViewInfo & View)
{
	FSubstrateViewMode Out = FSubstrateViewMode::None;
	if (IsSubstrateEnabled() && SubstrateDebugVisualizationCanRunOnPlatform(View.GetShaderPlatform()))
	{
		const uint32 ViewMode = FSubstrateVisualizationData::GetViewMode();
		switch (ViewMode)
		{
			case 1: return FSubstrateViewMode::MaterialProperties;
			case 2: return FSubstrateViewMode::MaterialCount;
			case 3: return FSubstrateViewMode::AdvancedMaterialProperties;
			case 4: return FSubstrateViewMode::MaterialClassification;
			case 5: return FSubstrateViewMode::DecalClassification;
			case 6: return FSubstrateViewMode::RoughRefractionClassification;
			case 7: return FSubstrateViewMode::SubstrateInfo;
			case 8: return FSubstrateViewMode::MaterialByteCount;
		}

		const FSubstrateVisualizationData& VisualizationData = GetSubstrateVisualizationData();
		if (View.Family && View.Family->EngineShowFlags.VisualizeSubstrate)
		{
			Out = VisualizationData.GetViewMode(View.CurrentSubstrateVisualizationMode);
		}
	}
	return Out;
}

bool ShouldRenderSubstrateDebugPasses(const FViewInfo& View)
{
	return GetSubstrateVisualizeMode(View) != FSubstrateViewMode::None || ShouldRenderSubstrateRoughRefractionRnD();
}

FScreenPassTexture AddSubstrateDebugPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor)
{
	check(IsSubstrateEnabled());

	const FSubstrateViewMode DebugMode = GetSubstrateVisualizeMode(View);
	if (DebugMode != FSubstrateViewMode::None)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Substrate::VisualizeMaterial");

		const bool bDebugPass = true;
		if (DebugMode == FSubstrateViewMode::MaterialProperties)
		{
			AddVisualizeMaterialPropertiesPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform());
		}
		if (DebugMode == FSubstrateViewMode::MaterialCount)
		{
			AddVisualizeMaterialCountPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform(), 2);
		}
		if (DebugMode == FSubstrateViewMode::MaterialByteCount)
		{
			AddVisualizeMaterialCountPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform(), 3);
		}
		if (DebugMode == FSubstrateViewMode::AdvancedMaterialProperties)
		{
			AddVisualizeAdvancedMaterialPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform());
		}
		else if (DebugMode == FSubstrateViewMode::SubstrateInfo)
		{
			AddVisualizeSystemInfoPasses(GraphBuilder, View, ScreenPassSceneColor, View.GetShaderPlatform());
		}
		else if (DebugMode == FSubstrateViewMode::DecalClassification)
		{
			if (IsDBufferPassEnabled(View.GetShaderPlatform()))
			{
				AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::EDecalSimple, bDebugPass);
				AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::EDecalSingle, bDebugPass);
				AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::EDecalComplex, bDebugPass);
			}
		}
		else if (DebugMode == FSubstrateViewMode::RoughRefractionClassification)
		{
			if (IsOpaqueRoughRefractionEnabled())
			{
				AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::EOpaqueRoughRefraction, bDebugPass);
				AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::EOpaqueRoughRefractionSSSWithout, bDebugPass);
			}
		}
		else if (DebugMode == FSubstrateViewMode::MaterialClassification)
		{
			if (GetSubstrateUsesComplexSpecialPath(View))
			{
				AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::EComplexSpecial, bDebugPass);
			}
			AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::EComplex, bDebugPass);
			AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::ESingle, bDebugPass);
			AddSubstrateInternalClassificationTilePass(GraphBuilder, View, nullptr, &ScreenPassSceneColor.Texture, ESubstrateTileType::ESimple, bDebugPass);
		}
	}

	SubstrateRoughRefractionRnD(GraphBuilder, View, ScreenPassSceneColor);

	return MoveTemp(ScreenPassSceneColor);
}

} // namespace Substrate
