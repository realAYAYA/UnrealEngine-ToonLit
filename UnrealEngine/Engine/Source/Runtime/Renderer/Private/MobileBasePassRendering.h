// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.h: base pass rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "HitProxies.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "PrimitiveSceneInfo.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightMapRendering.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "FogRendering.h"
#include "PlanarReflectionRendering.h"
#include "BasePassRendering.h"
#include "SkyAtmosphereRendering.h"
#include "RenderUtils.h"
#include "DebugViewModeRendering.h"
#include "LocalFogVolumeRendering.h"
#include "DBufferTextures.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"

bool MobileLocalLightsBufferEnabled(const FStaticShaderPlatform Platform);
bool MobileMergeLocalLightsInPrepassEnabled(const FStaticShaderPlatform Platform);
bool MobileMergeLocalLightsInBasepassEnabled(const FStaticShaderPlatform Platform);

struct FMobileBasePassTextures
{
	FRDGTextureRef ScreenSpaceAO = nullptr;
	FDBufferTextures DBufferTextures = {};
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FMobileBasePassUniformParameters, )
	SHADER_PARAMETER(float, AmbientOcclusionStaticFraction)
	SHADER_PARAMETER_STRUCT(FFogUniformParameters, Fog)
	SHADER_PARAMETER_STRUCT(FLocalFogVolumeUniformParameters, LFV)
	SHADER_PARAMETER_STRUCT(FForwardLightData, Forward)
	SHADER_PARAMETER_STRUCT(FForwardLightData, ForwardMMV)
	SHADER_PARAMETER_STRUCT(FPlanarReflectionUniformParameters, PlanarReflection) // Single global planar reflection for the forward pass.
	SHADER_PARAMETER_STRUCT(FMobileSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_STRUCT(FSubstrateMobileForwardPassUniformParameters, Substrate)
	SHADER_PARAMETER_STRUCT(FDebugViewModeUniformParameters, DebugViewMode)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, QuadOverdraw)
	SHADER_PARAMETER_STRUCT(FReflectionUniformParameters, ReflectionsParameters)
	SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGFTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWOcclusionBufferUAV)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AmbientOcclusionTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, AmbientOcclusionSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenSpaceShadowMaskTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, ScreenSpaceShadowMaskSampler)
	SHADER_PARAMETER_STRUCT_INCLUDE(FDBufferParameters, DBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

enum class EMobileBasePass
{
	Opaque,
	Translucent
};

extern void SetupMobileBasePassUniformParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	EMobileBasePass BasePass,
	const FMobileBasePassTextures& MobileBasePassTextures,
	FMobileBasePassUniformParameters& BasePassParameters);

extern TRDGUniformBufferRef<FMobileBasePassUniformParameters> CreateMobileBasePassUniformBuffer(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	EMobileBasePass BasePass,
	EMobileSceneTextureSetupMode SetupMode,
	const FMobileBasePassTextures& MobileBasePassTextures = {});

extern void SetupMobileDirectionalLightUniformParameters(
	const FScene& Scene,
	const FViewInfo& View,
	const TArray<FVisibleLightInfo,SceneRenderingAllocator>& VisibleLightInfos,
	int32 ChannelIdx,
	bool bDynamicShadows,
	FMobileDirectionalLightShaderParameters& Parameters);

extern void SetupMobileSkyReflectionUniformParameters(
	class FSkyLightSceneProxy* SkyLight,
	FMobileReflectionCaptureShaderParameters& Parameters);



class FPlanarReflectionSceneProxy;
class FScene;

enum EOutputFormat
{
	LDR_GAMMA_32,
	HDR_LINEAR_64,
};

EMobileLocalLightSetting GetMobileForwardLocalLightSetting(EShaderPlatform ShaderPlatform);

enum class EMobileTranslucentColorTransmittanceMode
{
	DEFAULT,
	DUAL_SRC_BLENDING,
	PROGRAMMABLE_BLENDING,
	SINGLE_SRC_BLENDING, // Grey transmittance
};

EMobileTranslucentColorTransmittanceMode MobileDefaultTranslucentColorTransmittanceMode(EShaderPlatform Platform);
EMobileTranslucentColorTransmittanceMode MobileActiveTranslucentColorTransmittanceMode(EShaderPlatform Platform, bool bExplicitDefaultMode);
bool MaterialRequiresColorTransmittanceBlending(const FMaterialShaderParameters& MaterialParameters);
bool ShouldCacheShaderForColorTransmittanceFallback(const FMaterialShaderPermutationParameters& Parameters, EMobileTranslucentColorTransmittanceMode TranslucentColorTransmittanceFallback);

bool ShouldCacheShaderByPlatformAndOutputFormat(EShaderPlatform Platform, EOutputFormat OutputFormat);
// shared defines for mobile base pass VS and PS
void MobileBasePassModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment, EOutputFormat OutputFormat);

template<typename LightMapPolicyType>
class TMobileBasePassShaderElementData : public FMeshMaterialShaderElementData
{
public:
	TMobileBasePassShaderElementData(const typename LightMapPolicyType::ElementDataType& InLightMapPolicyElementData, bool bInCanReceiveCSM)
		: LightMapPolicyElementData(InLightMapPolicyElementData)
		, bCanReceiveCSM(bInCanReceiveCSM)
	{}

	typename LightMapPolicyType::ElementDataType LightMapPolicyElementData;

	const bool bCanReceiveCSM;
};

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename LightMapPolicyType>
class TMobileBasePassVSPolicyParamType : public FMeshMaterialShader, public LightMapPolicyType::VertexParametersType
{
	DECLARE_INLINE_TYPE_LAYOUT_EXPLICIT_BASES(TMobileBasePassVSPolicyParamType, NonVirtual, FMeshMaterialShader, typename LightMapPolicyType::VertexParametersType);
protected:

	TMobileBasePassVSPolicyParamType() {}
	TMobileBasePassVSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileBasePassUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
	}

public:

	// static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	void GetShaderBindings(
		const FScene* Scene,
		const FStaticFeatureLevel FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const TMobileBasePassShaderElementData<LightMapPolicyType>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

		LightMapPolicyType::GetVertexShaderBindings(
			PrimitiveSceneProxy,
			ShaderElementData.LightMapPolicyElementData,
			this,
			ShaderBindings);
	}
};

template<typename LightMapPolicyType>
class TMobileBasePassVSBaseType : public TMobileBasePassVSPolicyParamType<LightMapPolicyType>
{
	typedef TMobileBasePassVSPolicyParamType<LightMapPolicyType> Super;
	DECLARE_INLINE_TYPE_LAYOUT(TMobileBasePassVSBaseType, NonVirtual);
protected:

	TMobileBasePassVSBaseType() {}
	TMobileBasePassVSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform) && LightMapPolicyType::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

template< typename LightMapPolicyType, EOutputFormat OutputFormat >
class TMobileBasePassVS : public TMobileBasePassVSBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TMobileBasePassVS,MeshMaterial);
public:
	
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{		
		return TMobileBasePassVSBaseType<LightMapPolicyType>::ShouldCompilePermutation(Parameters) && ShouldCacheShaderByPlatformAndOutputFormat(Parameters.Platform, OutputFormat);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		MobileBasePassModifyCompilationEnvironment(Parameters, OutEnvironment, OutputFormat);
		TMobileBasePassVSBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
	
	/** Initialization constructor. */
	TMobileBasePassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TMobileBasePassVSBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TMobileBasePassVS() {}
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename LightMapPolicyType>
class TMobileBasePassPSPolicyParamType : public FMeshMaterialShader, public LightMapPolicyType::PixelParametersType
{
	DECLARE_INLINE_TYPE_LAYOUT_EXPLICIT_BASES(TMobileBasePassPSPolicyParamType, NonVirtual, FMeshMaterialShader, typename LightMapPolicyType::PixelParametersType);
public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		// Modify compilation environment depending upon material shader quality level settings.
		ModifyCompilationEnvironmentForQualityLevel(Parameters.Platform, Parameters.MaterialParameters.QualityLevel, OutEnvironment);
	}

	/** Initialization constructor. */
	TMobileBasePassPSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileBasePassUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
		
		MobileDirectionLightBufferParam.Bind(Initializer.ParameterMap, FMobileDirectionalLightShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
		ReflectionParameter.Bind(Initializer.ParameterMap, FMobileReflectionCaptureShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
				
		UseCSMParameter.Bind(Initializer.ParameterMap, TEXT("UseCSM"));
	}

	TMobileBasePassPSPolicyParamType() {}

private:
	LAYOUT_FIELD(FShaderUniformBufferParameter, MobileDirectionLightBufferParam);
	LAYOUT_FIELD(FShaderUniformBufferParameter, ReflectionParameter);
	LAYOUT_FIELD(FShaderParameter, UseCSMParameter);
	
public:
	void GetShaderBindings(
		const FScene* Scene,
		const FStaticFeatureLevel FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const TMobileBasePassShaderElementData<LightMapPolicyType>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const;

private:
	static bool ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment);
};

template<typename LightMapPolicyType>
class TMobileBasePassPSBaseType : public TMobileBasePassPSPolicyParamType<LightMapPolicyType>
{
	typedef TMobileBasePassPSPolicyParamType<LightMapPolicyType> Super;
	DECLARE_INLINE_TYPE_LAYOUT(TMobileBasePassPSBaseType, NonVirtual);
public:
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return LightMapPolicyType::ShouldCompilePermutation(Parameters)
			&& Super::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	/** Initialization constructor. */
	TMobileBasePassPSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}
	TMobileBasePassPSBaseType() {}
};


namespace MobileBasePass
{
	bool IsUsingDirectionalLightForLighmapPolicySelection(const FScene* Scene);

	ELightMapPolicyType SelectMeshLightmapPolicy(
		const FScene* Scene, 
		const FMeshBatch& MeshBatch, 
		const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
		bool bPrimReceivesCSM, 
		bool bUsedDeferredShading,
		bool bIsLitMaterial,
		bool bIsTranslucent);

	bool GetShaders(
		ELightMapPolicyType LightMapPolicyType,
		EMobileLocalLightSetting LocalLightSetting,
		const FMaterial& MaterialResource,
		const FVertexFactoryType* VertexFactoryType,
		bool bEnableSkyLight, 
		TShaderRef<TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>>& VertexShader,
		TShaderRef<TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>>& PixelShader);

	const FLightSceneInfo* GetDirectionalLightInfo(const FScene* Scene, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

	bool StaticCanReceiveCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

	void SetOpaqueRenderState(FMeshPassProcessorRenderState& DrawRenderState, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterial& Material, FMaterialShadingModelField ShadingModels, bool bEnableReceiveDecalOutput, bool bUsesDeferredShading);
	void SetTranslucentRenderState(FMeshPassProcessorRenderState& DrawRenderState, const FMaterial& Material, FMaterialShadingModelField ShadingModels);

	inline bool UseSkylightPermutation(bool bEnableSkyLight, int32 MobileSkyLightPermutationOptions)
	{
		if (bEnableSkyLight)
		{
			return MobileSkyLightPermutationOptions == 0 || MobileSkyLightPermutationOptions == 2;
		}
		else
		{
			return MobileSkyLightPermutationOptions == 0 || MobileSkyLightPermutationOptions == 1;
		}
	}
};

template< typename LightMapPolicyType, EOutputFormat OutputFormat, bool bEnableSkyLight, EMobileLocalLightSetting LocalLightSetting, EMobileTranslucentColorTransmittanceMode TranslucentColorTransmittanceFallback = EMobileTranslucentColorTransmittanceMode::DEFAULT>
class TMobileBasePassPS : public TMobileBasePassPSBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TMobileBasePassPS,MeshMaterial);
public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{		
		// We compile the point light shader combinations based on the project settings
		static auto* MobileSkyLightPermutationCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.SkyLightPermutation"));
		const int32 MobileSkyLightPermutationOptions = MobileSkyLightPermutationCVar->GetValueOnAnyThread();
		const bool bIsLit = Parameters.MaterialParameters.ShadingModels.IsLit();
		// Only compile skylight version for lit materials
		const bool bShouldCacheBySkylight = !bEnableSkyLight || bIsLit;
		// Only compile skylight permutations when they are enabled
		if (bIsLit && !MobileBasePass::UseSkylightPermutation(bEnableSkyLight, MobileSkyLightPermutationOptions))
		{
			return false;
		}
		
		const bool bDeferredShadingEnabled = IsMobileDeferredShadingEnabled(Parameters.Platform);
		const bool bIsTranslucent = IsTranslucentBlendMode(Parameters.MaterialParameters) || Parameters.MaterialParameters.ShadingModels.HasShadingModel(MSM_SingleLayerWater);
		const bool bMaterialUsesForwardShading = bIsLit && bIsTranslucent;
		// Translucent materials always support clustered shading on mobile deferred
		const bool bForwardShading = !bDeferredShadingEnabled || bMaterialUsesForwardShading;

		EMobileLocalLightSetting SupportedLocalLightsType = EMobileLocalLightSetting::LOCAL_LIGHTS_DISABLED;
		if (bForwardShading && bIsLit)
		{
			SupportedLocalLightsType = GetMobileForwardLocalLightSetting(Parameters.Platform);
		}
		// Deferred shading does not need SkyLight and LocalLight permutations
		// TODO: skip skylight permutations for deferred	
		bool bEnableLocalLights = LocalLightSetting != EMobileLocalLightSetting::LOCAL_LIGHTS_DISABLED;
		const bool bShouldCacheByLocalLights = !bEnableLocalLights || (bIsLit && (SupportedLocalLightsType == LocalLightSetting));

		return TMobileBasePassPSBaseType<LightMapPolicyType>::ShouldCompilePermutation(Parameters) && 
				ShouldCacheShaderByPlatformAndOutputFormat(Parameters.Platform, OutputFormat) && 
				bShouldCacheBySkylight && 
				bShouldCacheByLocalLights && 
				ShouldCacheShaderForColorTransmittanceFallback(Parameters, TranslucentColorTransmittanceFallback);
	}
	
	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{		
		MobileBasePassModifyCompilationEnvironment(Parameters, OutEnvironment, OutputFormat);

		const bool bMobileUsesShadowMaskTexture = MobileUsesShadowMaskTexture(Parameters.Platform);
		const bool bEnableClusteredReflections = MobileForwardEnableClusteredReflections(Parameters.Platform);
		const bool bDeferredShadingEnabled = IsMobileDeferredShadingEnabled(Parameters.Platform);
		const bool bTranslucentMaterial = IsTranslucentBlendMode(Parameters.MaterialParameters) || Parameters.MaterialParameters.ShadingModels.HasShadingModel(MSM_SingleLayerWater);
		const bool bIsLit = Parameters.MaterialParameters.ShadingModels.IsLit();
		const bool bMaterialUsesForwardShading = bIsLit && bTranslucentMaterial;
		// Translucent materials always support clustered shading on mobile deferred
		const bool bForwardShading = !bDeferredShadingEnabled || bMaterialUsesForwardShading;
		// Only stationary skylights contribute into basepass with deferred shading materials. Has to do this test as on some project configurations we dont have a separate permutation for no-skylight
		const bool bEnableSkylightInBasePass = bEnableSkyLight && (bForwardShading || FReadOnlyCVARCache::EnableStationarySkylight());

		TMobileBasePassPSBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), bEnableSkylightInBasePass);
		OutEnvironment.SetDefine(TEXT("ENABLE_AMBIENT_OCCLUSION"), IsMobileAmbientOcclusionEnabled(Parameters.Platform) ? 1u : 0u);
		
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_CLUSTERED_LIGHTS"), (LocalLightSetting == EMobileLocalLightSetting::LOCAL_LIGHTS_ENABLED) ? 1u : 0u);

		// Translucent materials don't write to depth so cannot use prepass
		uint32 MergedLocalLights = 0u;
		if (LocalLightSetting == EMobileLocalLightSetting::LOCAL_LIGHTS_BUFFER)
		{
			if (MobileMergeLocalLightsInPrepassEnabled(Parameters.Platform) && !bTranslucentMaterial)
			{
				MergedLocalLights = 1u;
			}
			else if (MobileMergeLocalLightsInBasepassEnabled(Parameters.Platform) || bTranslucentMaterial)
			{
				MergedLocalLights = 2u;
			}
		}
		OutEnvironment.SetDefine(TEXT("MERGED_LOCAL_LIGHTS_MOBILE"), MergedLocalLights);
		OutEnvironment.SetDefine(TEXT("ENABLE_CLUSTERED_REFLECTION"), bEnableClusteredReflections ? 1u : 0u);
		OutEnvironment.SetDefine(TEXT("USE_SHADOWMASKTEXTURE"), bMobileUsesShadowMaskTexture && !bTranslucentMaterial ? 1u : 0u);
		OutEnvironment.SetDefine(TEXT("ENABLE_DBUFFER_TEXTURES"), Parameters.MaterialParameters.MaterialDomain == MD_Surface ? 1u : 0u);
		EMobileTranslucentColorTransmittanceMode TranslucentColorTransmittanceMode = EMobileTranslucentColorTransmittanceMode::DEFAULT;
		if (Parameters.MaterialParameters.ShadingModels.HasShadingModel(MSM_ThinTranslucent))
		{
			TranslucentColorTransmittanceMode = (TranslucentColorTransmittanceFallback == EMobileTranslucentColorTransmittanceMode::DEFAULT) ? MobileDefaultTranslucentColorTransmittanceMode(Parameters.Platform) : TranslucentColorTransmittanceFallback;
		}
		OutEnvironment.SetDefine(TEXT("MOBILE_TRANSLUCENT_COLOR_TRANSMITTANCE_DUAL_SRC_BLENDING"), TranslucentColorTransmittanceMode == EMobileTranslucentColorTransmittanceMode::DUAL_SRC_BLENDING ? 1u : 0u);
		OutEnvironment.SetDefine(TEXT("MOBILE_TRANSLUCENT_COLOR_TRANSMITTANCE_PROGRAMMABLE_BLENDING"), TranslucentColorTransmittanceMode == EMobileTranslucentColorTransmittanceMode::PROGRAMMABLE_BLENDING ? 1u : 0u);
		OutEnvironment.SetDefine(TEXT("MOBILE_TRANSLUCENT_COLOR_TRANSMITTANCE_SINGLE_SRC_BLENDING"), TranslucentColorTransmittanceMode == EMobileTranslucentColorTransmittanceMode::SINGLE_SRC_BLENDING ? 1u : 0u);
	}
	
	/** Initialization constructor. */
	TMobileBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TMobileBasePassPSBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TMobileBasePassPS() {}
};

class FMobileBasePassMeshProcessor : public FSceneRenderingAllocatorObject<FMobileBasePassMeshProcessor>, public FMeshPassProcessor
{
public:
	enum class EFlags
	{
		None = 0,

		// Informs the processor whether a depth-stencil target is bound when processed draw commands are issued.
		CanUseDepthStencil = (1 << 0),

		// Informs the processor whether primitives can receive shadows from cascade shadow maps.
		CanReceiveCSM = (1 << 1),

		// Informs the processor to use PassDrawRenderState for all mesh commands
		ForcePassDrawRenderState = (1 << 2),

		// Informs the processor to not cache any mesh commands
		DoNotCache = (1 << 3)
	};

	FMobileBasePassMeshProcessor(
		EMeshPass::Type InMeshPassType,
		const FScene* InScene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext,
		EFlags Flags,
		ETranslucencyPass::Type InTranslucencyPassType = ETranslucencyPass::TPT_MAX);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

	FMeshPassProcessorRenderState PassDrawRenderState;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override;

	void CollectPSOInitializersForLMPolicy(
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
		const FGraphicsPipelineRenderTargetsInfo& RESTRICT RenderTargetsInfo,
		const FMaterial& RESTRICT MaterialResource,
		const bool bRenderSkylight,
		EMobileLocalLightSetting LocalLightSetting,
		const ELightMapPolicyType LightMapPolicyType,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		EPrimitiveType PrimitiveType,
		TArray<FPSOPrecacheData>& PSOInitializers);

private:
	FMaterialShadingModelField FilterShadingModelsMask(const FMaterialShadingModelField& ShadingModels) const;
	bool ShouldDraw(const class FMaterial& Material) const;

	bool TryAddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material);

	bool Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		const bool bIsMasked,
		const bool bIsTranslucent,
		FMaterialShadingModelField ShadingModels,
		const ELightMapPolicyType LightMapPolicyType,
		const bool bCanReceiveCSM,
		const FUniformLightMapPolicy::ElementDataType& RESTRICT LightMapElementData);

	const ETranslucencyPass::Type TranslucencyPassType;
	const EFlags Flags;
	const bool bTranslucentBasePass;
	// Whether renderer uses deferred shading
	const bool bDeferredShading; 
	// Whether this pass uses deferred shading
	const bool bPassUsesDeferredShading; 
};

ENUM_CLASS_FLAGS(FMobileBasePassMeshProcessor::EFlags);
