// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "MobileBasePassRendering.h"
#include "TranslucentRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "ShaderPlatformQualitySettings.h"
#include "MaterialShaderQualitySettings.h"
#include "PrimitiveSceneInfo.h"
#include "MeshPassProcessor.inl"
#include "Engine/TextureCube.h"

uint8 GetMobileShadingModelStencilValue(FMaterialShadingModelField ShadingModel)
{
	if (ShadingModel.HasOnlyShadingModel(MSM_DefaultLit))
	{
		return 1u;
	}
	else if (ShadingModel.HasOnlyShadingModel(MSM_Unlit))
	{
		return 0u;
	}
	
	// mark everyhing as MSM_DefaultLit if GBuffer CustomData is not supported
	return MobileUsesGBufferCustomData(GMaxRHIShaderPlatform) ? 2u : 1u;
}

bool MobileUsesNoLightMapPermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0);
	const bool bIsLitMaterial = Parameters.MaterialParameters.ShadingModels.IsLit();
	const bool bDeferredShading = IsMobileDeferredShadingEnabled(Parameters.Platform);

	if (!bDeferredShading && !bAllowStaticLighting && bIsLitMaterial && 
		!IsTranslucentBlendMode(Parameters.MaterialParameters.BlendMode) && 
		!Parameters.MaterialParameters.ShadingModels.HasShadingModel(MSM_SingleLayerWater))
	{
		// We don't need NoLightMap permutation if CSM shader can handle no-CSM case with a branch inside shader
		return !MobileUseCSMShaderBranch();
	}
		
	return true;
}

template <ELightMapPolicyType Policy, bool bEnableLocalLights>
bool GetUniformMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bEnableSkyLight,
	TShaderRef<TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>>& VertexShader,
	TShaderRef<TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>>& PixelShader
	)
{
	using FVertexShaderType = TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>;
	using FPixelShaderType = TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>;

	FMaterialShaderTypes ShaderTypes;
	if (IsMobileHDR())
	{
		ShaderTypes.AddShaderType<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64>>();

		if (bEnableSkyLight)
		{
			ShaderTypes.AddShaderType<TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, true, bEnableLocalLights>>();
		}
		else
		{
			ShaderTypes.AddShaderType<TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, false, bEnableLocalLights>>();
		}	
	}
	else
	{
		ShaderTypes.AddShaderType<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32>>();

		if (bEnableSkyLight)
		{
			ShaderTypes.AddShaderType<TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, true, bEnableLocalLights>>();
		}
		else
		{
			ShaderTypes.AddShaderType<TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, false, bEnableLocalLights>>();
		}			
	}

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

template <bool bEnableLocalLights>
bool GetMobileBasePassShaders(
	ELightMapPolicyType LightMapPolicyType, 
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bEnableSkyLight,
	TShaderRef<TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>>& VertexShader,
	TShaderRef<TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>>& PixelShader
	)
{
	switch (LightMapPolicyType)
	{
	case LMP_NO_LIGHTMAP:
		return GetUniformMobileBasePassShaders<LMP_NO_LIGHTMAP, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_LQ_LIGHTMAP:
		return GetUniformMobileBasePassShaders<LMP_LQ_LIGHTMAP, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_LIGHTMAP:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_LIGHTMAP, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM:
		return GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM, bEnableLocalLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
	default:										
		check(false);
		return true;
	}
}

bool MobileBasePass::GetShaders(
	ELightMapPolicyType LightMapPolicyType,
	bool bEnableLocalLights,
	const FMaterial& MaterialResource,
	FVertexFactoryType* VertexFactoryType,
	bool bEnableSkyLight, 
	TShaderRef<TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>>& VertexShader,
	TShaderRef<TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>>& PixelShader)
{
	bool bIsLit = (MaterialResource.GetShadingModels().IsLit());
	if (bIsLit && !UseSkylightPermutation(bEnableSkyLight, FReadOnlyCVARCache::Get().MobileSkyLightPermutation))	
	{
		bEnableSkyLight = !bEnableSkyLight;
	}

	if (bEnableLocalLights)
	{
		return GetMobileBasePassShaders<true>(
			LightMapPolicyType,
			MaterialResource,
			VertexFactoryType,
			bEnableSkyLight,
			VertexShader,
			PixelShader
			);
	}
	else
	{
		return GetMobileBasePassShaders<false>(
			LightMapPolicyType,
			MaterialResource,
			VertexFactoryType,
			bEnableSkyLight,
			VertexShader,
			PixelShader
			);
	}
}

static bool UseSkyReflectionCapture(const FScene* RenderScene)
{
	return RenderScene
		&& RenderScene->SkyLight
		&& RenderScene->SkyLight->ProcessedTexture
		&& RenderScene->SkyLight->ProcessedTexture->TextureRHI;
}

const FLightSceneInfo* MobileBasePass::GetDirectionalLightInfo(const FScene* Scene, const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
	const FLightSceneInfo* MobileDirectionalLight = nullptr;
	if (PrimitiveSceneProxy && Scene)
	{
		const int32 LightChannel = GetFirstLightingChannelFromMask(PrimitiveSceneProxy->GetLightingChannelMask());
		MobileDirectionalLight = LightChannel >= 0 ? Scene->MobileDirectionalLights[LightChannel] : nullptr;
	}
	return MobileDirectionalLight;
}

bool MobileBasePass::StaticCanReceiveCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
	// For movable directional lights, when CSM culling is disabled the default behavior is to receive CSM.
	static auto* CVarMobileEnableMovableLightCSMShaderCulling = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableMovableLightCSMShaderCulling"));
	if (LightSceneInfo && LightSceneInfo->Proxy->IsMovable() && CVarMobileEnableMovableLightCSMShaderCulling->GetValueOnRenderThread() == 0)
	{		
		return true;
	}

	// If culling is enabled then CSM receiving is determined during InitDynamicShadows.
	// If culling is disabled then stationary directional lights default to no CSM. 
	return false; 
}

ELightMapPolicyType MobileBasePass::SelectMeshLightmapPolicy(
	const FScene* Scene, 
	const FMeshBatch& Mesh, 
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FLightSceneInfo* MobileDirectionalLight,
	FMaterialShadingModelField ShadingModels, 
	bool bPrimReceivesCSM,
	bool bUsesDeferredShading,
	ERHIFeatureLevel::Type FeatureLevel,
	EBlendMode BlendMode)
{
	// Unlit uses NoLightmapPolicy with 0 point lights
	ELightMapPolicyType SelectedLightmapPolicy = LMP_NO_LIGHTMAP;
	
	const bool bIsLitMaterial = ShadingModels.IsLit();
	if (bIsLitMaterial)
	{
		const FReadOnlyCVARCache& ReadOnlyCVARCache = FReadOnlyCVARCache::Get();

		if (!ReadOnlyCVARCache.bAllowStaticLighting || (ReadOnlyCVARCache.bMobileEnableNoPrecomputedLightingCSMShader && Scene && Scene->GetForceNoPrecomputedLighting()))
		{
			if (!IsTranslucentBlendMode(BlendMode) &&
				!ShadingModels.HasShadingModel(MSM_SingleLayerWater))
			{
				// Whether to use a single CSM permutation with a branch in the shader
				bPrimReceivesCSM |= MobileUseCSMShaderBranch();
			}
			
			// no precomputed lighting
			if (!bPrimReceivesCSM || bUsesDeferredShading)
			{
				SelectedLightmapPolicy = LMP_NO_LIGHTMAP;
			}
			else
			{
				SelectedLightmapPolicy = LMP_MOBILE_DIRECTIONAL_LIGHT_CSM;				
			}
		}
		else
		{
			// Check for a cached light-map.
			const FLightMapInteraction LightMapInteraction = (Mesh.LCI != nullptr)
				? Mesh.LCI->GetLightMapInteraction(FeatureLevel)
				: FLightMapInteraction();
		
			const bool bUseMovableLight = MobileDirectionalLight && !MobileDirectionalLight->Proxy->HasStaticShadowing() && ReadOnlyCVARCache.bMobileAllowMovableDirectionalLights;
			const bool bUseStaticAndCSM = MobileDirectionalLight && MobileDirectionalLight->Proxy->UseCSMForDynamicObjects()
											&& bPrimReceivesCSM
											&& ReadOnlyCVARCache.bMobileEnableStaticAndCSMShadowReceivers;

			const bool bMovableWithCSM = bUseMovableLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && bPrimReceivesCSM;

			const bool bPrimitiveUsesILC = PrimitiveSceneProxy
										&& (PrimitiveSceneProxy->IsMovable() || PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting() || PrimitiveSceneProxy->GetLightmapType() == ELightmapType::ForceVolumetric)
										&& PrimitiveSceneProxy->WillEverBeLit()
										&& PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;

			const bool bHasValidVLM = Scene && Scene->VolumetricLightmapSceneData.HasData();

			const bool bHasValidILC = Scene && Scene->PrecomputedLightVolumes.Num() > 0
									&& IsIndirectLightingCacheAllowed(FeatureLevel);

			if (LightMapInteraction.GetType() == LMIT_Texture && ReadOnlyCVARCache.bEnableLowQualityLightmaps)
			{
				const FShadowMapInteraction ShadowMapInteraction = (Mesh.LCI != nullptr)
					? Mesh.LCI->GetShadowMapInteraction(FeatureLevel)
					: FShadowMapInteraction();

				if ((bUseStaticAndCSM || bMovableWithCSM) && !bUsesDeferredShading)
				{
					if (ShadowMapInteraction.GetType() == SMIT_Texture &&
						MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() &&
						ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
					{
						SelectedLightmapPolicy = LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM;
					}
					else
					{
						// Lightmap path
						if (bMovableWithCSM)
						{
							SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP;
						}
						else
						{
							SelectedLightmapPolicy = LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_LIGHTMAP;
						}
					}
				}
				else
				{
					if (ShadowMapInteraction.GetType() == SMIT_Texture &&
						ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
					{
						SelectedLightmapPolicy = LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP;
					}
					else
					{
						// Lightmap path
						if (bUseMovableLight)
						{
							if (bUsesDeferredShading)
							{
								SelectedLightmapPolicy = LMP_LQ_LIGHTMAP;
							}
							else
							{
								SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP;
							}
						}
						else
						{
							SelectedLightmapPolicy = LMP_LQ_LIGHTMAP;
						}
					}
				}
			}
			else if ((bHasValidVLM || bHasValidILC) && bPrimitiveUsesILC)
			{
				if ((bUseStaticAndCSM || bMovableWithCSM) && !bUsesDeferredShading && ReadOnlyCVARCache.bMobileEnableStaticAndCSMShadowReceivers)
				{
					SelectedLightmapPolicy = LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT;
				}
				else
				{
					SelectedLightmapPolicy = LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT;
				}
			}
		}
	}
		
	return SelectedLightmapPolicy;
}

void MobileBasePass::SetOpaqueRenderState(FMeshPassProcessorRenderState& DrawRenderState, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterial& Material, FMaterialShadingModelField ShadingModels, bool bEnableReceiveDecalOutput, bool bUsesDeferredShading)
{
	uint8 StencilValue = 0;
	if (bEnableReceiveDecalOutput)
	{
		uint8 ReceiveDecals = (PrimitiveSceneProxy && !PrimitiveSceneProxy->ReceivesDecals() ? 0x01 : 0x00);
		StencilValue |= GET_STENCIL_BIT_MASK(RECEIVE_DECAL, ReceiveDecals);
	}
	
	if (bUsesDeferredShading)
	{
		uint8 ShadingModel = GetMobileShadingModelStencilValue(ShadingModels);
		StencilValue |= GET_STENCIL_MOBILE_SM_MASK(ShadingModel);
		StencilValue |= STENCIL_LIGHTING_CHANNELS_MASK(PrimitiveSceneProxy ? PrimitiveSceneProxy->GetLightingChannelStencilValue() : 0x00);
	}
		
	if (bEnableReceiveDecalOutput || bUsesDeferredShading)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				true, CF_DepthNearOrEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				// don't use masking as it has significant performance hit on Mali GPUs (T860MP2)
				0x00, 0xff >::GetRHI());

		DrawRenderState.SetStencilRef(StencilValue); 
	}
	else
	{
		// default depth state should be already set
	}

	if (Material.GetBlendMode() == BLEND_Masked && Material.IsUsingAlphaToCoverage())
	{
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, 
														CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, 
														CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, 
														CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, 
														CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, 
														CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, 
														true>::GetRHI());
	}
}

void MobileBasePass::SetTranslucentRenderState(FMeshPassProcessorRenderState& DrawRenderState, const FMaterial& Material, FMaterialShadingModelField ShadingModels)
{
	const bool bIsUsingMobilePixelProjectedReflection = Material.IsUsingPlanarForwardReflections() && IsUsingMobilePixelProjectedReflection(GetFeatureLevelShaderPlatform(Material.GetFeatureLevel()));

	if (ShadingModels.HasShadingModel(MSM_ThinTranslucent))
	{
		// the mobile thin translucent fallback uses a similar mode as BLEND_Translucent, but multiplies color by 1 insead of SrcAlpha.
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
														CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
														CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
	}
	else
	{
		switch (Material.GetBlendMode())
		{
		case BLEND_Translucent:
			if (Material.ShouldWriteOnlyAlpha())
			{
				DrawRenderState.SetBlendState(TStaticBlendState<CW_ALPHA, BO_Add, BF_Zero, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			}
			else
			{
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			}
			break;
		case BLEND_Additive:
			// Add to the existing scene color
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			break;
		case BLEND_Modulate:
			// Modulate with the existing scene color, preserve destination alpha.
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_One,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			break;
		case BLEND_AlphaComposite:
			// Blend with existing scene color. New color is already pre-multiplied by alpha.
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			break;
		case BLEND_AlphaHoldout:
			// Blend by holding out the matte shape of the source alpha
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
															CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			break;
		default:
			if (ShadingModels.HasShadingModel(MSM_SingleLayerWater))
			{
				// Single layer water is an opaque marerial rendered as translucent on Mobile. We force pre-multiplied alpha to achieve water depth based transmittance.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
																CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			}
			else
			{
				check(0);
			}
		};
	}

	if (Material.ShouldDisableDepthTest())
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	}
}

static FMeshDrawCommandSortKey GetBasePassStaticSortKey(EBlendMode BlendMode, bool bBackground)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.PackedData = (BlendMode == EBlendMode::BLEND_Masked ? 1 : 0);
	SortKey.PackedData|= (bBackground ? 2 : 0); // background flag in second bit
	return SortKey;
}

template<>
void TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>::GetShaderBindings(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const FMeshPassProcessorRenderState& DrawRenderState,
	const TMobileBasePassShaderElementData<FUniformLightMapPolicy>& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings) const
{
	FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

	FUniformLightMapPolicy::GetPixelShaderBindings(
		PrimitiveSceneProxy,
		ShaderElementData.LightMapPolicyElementData,
		this,
		ShaderBindings);

	if (Scene)
	{
		if (ReflectionParameter.IsBound())
		{
			FRHIUniformBuffer* ReflectionUB = GDefaultMobileReflectionCaptureUniformBuffer.GetUniformBufferRHI();
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy ? PrimitiveSceneProxy->GetPrimitiveSceneInfo() : nullptr;
			if (PrimitiveSceneInfo && PrimitiveSceneInfo->CachedReflectionCaptureProxy)
			{
				ReflectionUB = PrimitiveSceneInfo->CachedReflectionCaptureProxy->MobileUniformBuffer;
			}
			// If no reflection captures are available then attempt to use sky light's texture.
			else if (UseSkyReflectionCapture(Scene))
			{
				ReflectionUB = Scene->UniformBuffers.MobileSkyReflectionUniformBuffer;
			}
			ShaderBindings.Add(ReflectionParameter, ReflectionUB);
		}
	}
	else
	{
		ensure(!ReflectionParameter.IsBound());
	}

	// Set directional light UB
	if (MobileDirectionLightBufferParam.IsBound() && Scene)
	{
		int32 UniformBufferIndex = PrimitiveSceneProxy ? GetFirstLightingChannelFromMask(PrimitiveSceneProxy->GetLightingChannelMask()) + 1 : 0;
		ShaderBindings.Add(MobileDirectionLightBufferParam, Scene->UniformBuffers.MobileDirectionalLightUniformBuffers[UniformBufferIndex]);
	}

	if (UseCSMParameter.IsBound())
	{
		ShaderBindings.Add(UseCSMParameter, ShaderElementData.bCanReceiveCSM ? 1 : 0);
	}
}

FMobileBasePassMeshProcessor::FMobileBasePassMeshProcessor(
	EMeshPass::Type InMeshPassType,
	const FScene* Scene,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext,
	EFlags InFlags,
	ETranslucencyPass::Type InTranslucencyPassType)
	: FMeshPassProcessor(InMeshPassType, Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InDrawRenderState)
	, TranslucencyPassType(InTranslucencyPassType)
	, Flags(InFlags)
	, bTranslucentBasePass(InTranslucencyPassType != ETranslucencyPass::TPT_MAX)
	, bUsesDeferredShading(!bTranslucentBasePass && IsMobileDeferredShadingEnabled(GetFeatureLevelShaderPlatform(InFeatureLevel)))
{
}

bool FMobileBasePassMeshProcessor::TryAddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material)
{
	const EBlendMode BlendMode = Material.GetBlendMode();
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	const bool bUsesWaterMaterial = ShadingModels.HasShadingModel(MSM_SingleLayerWater); // Water goes into the translucent pass
	const bool bCanReceiveCSM = ((Flags & EFlags::CanReceiveCSM) == EFlags::CanReceiveCSM);

	bool bResult = true;
	if (bTranslucentBasePass)
	{
		// Skipping TPT_TranslucencyAfterDOFModulate. That pass is only needed for Dual Blending, which is not supported on Mobile.
		bool bShouldDraw = (bIsTranslucent || bUsesWaterMaterial) &&
		(TranslucencyPassType == ETranslucencyPass::TPT_AllTranslucency
		|| (TranslucencyPassType == ETranslucencyPass::TPT_StandardTranslucency && !Material.IsMobileSeparateTranslucencyEnabled())
		|| (TranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOF && Material.IsMobileSeparateTranslucencyEnabled()));

		if (bShouldDraw)
		{
			check(bCanReceiveCSM == false);
			const FLightSceneInfo* MobileDirectionalLight = MobileBasePass::GetDirectionalLightInfo(Scene, PrimitiveSceneProxy);
			// Opaque meshes used for mobile pixel projected reflection could receive CSM in translucent pass.
			ELightMapPolicyType LightmapPolicyType = MobileBasePass::SelectMeshLightmapPolicy(Scene, MeshBatch, PrimitiveSceneProxy, MobileDirectionalLight, ShadingModels, bCanReceiveCSM, false, FeatureLevel, BlendMode);
			bResult = Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, BlendMode, ShadingModels, LightmapPolicyType, bCanReceiveCSM, MeshBatch.LCI);
		}
	}
	else
	{
		// opaque materials.
		if (!bIsTranslucent && !bUsesWaterMaterial)
		{
			const FLightSceneInfo* MobileDirectionalLight = MobileBasePass::GetDirectionalLightInfo(Scene, PrimitiveSceneProxy);
			ELightMapPolicyType LightmapPolicyType = MobileBasePass::SelectMeshLightmapPolicy(Scene, MeshBatch, PrimitiveSceneProxy, MobileDirectionalLight, ShadingModels, bCanReceiveCSM, bUsesDeferredShading, FeatureLevel, BlendMode);
			bResult = Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, BlendMode, ShadingModels, LightmapPolicyType, bCanReceiveCSM, MeshBatch.LCI);
		}
	}

	return bResult;
}

void FMobileBasePassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (!MeshBatch.bUseForMaterial || 
		(Flags & FMobileBasePassMeshProcessor::EFlags::DoNotCache) == FMobileBasePassMeshProcessor::EFlags::DoNotCache ||
		(PrimitiveSceneProxy && !PrimitiveSceneProxy->ShouldRenderInMainPass()))
	{
		return;
	}
	
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material && Material->GetRenderingThreadShaderMap())
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
			{
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}
}

bool FMobileBasePassMeshProcessor::Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		EBlendMode BlendMode,
		FMaterialShadingModelField ShadingModels,
		const ELightMapPolicyType LightMapPolicyType,
		const bool bCanReceiveCSM,
		const FUniformLightMapPolicy::ElementDataType& RESTRICT LightMapElementData)
{
	TMeshProcessorShaders<
		TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>,
		TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>> BasePassShaders;
	
	bool bEnableSkyLight = false;
	
	if (Scene && Scene->SkyLight)
	{
		bEnableSkyLight = ShadingModels.IsLit() && Scene->ShouldRenderSkylightInBasePass(BlendMode);
	}

	bool bEnableLocalLights = false;
	if (Scene && PrimitiveSceneProxy && ShadingModels.IsLit())
	{
		// Whether material with a forward shading needs clustered lights
		if (!bUsesDeferredShading && MobileForwardEnableLocalLights(Scene->GetShaderPlatform()))
		{
			bEnableLocalLights = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->NumMobileDynamicLocalLights > 0;
		}
	}

	if (!MobileBasePass::GetShaders(
		LightMapPolicyType,
		bEnableLocalLights,
		MaterialResource,
		MeshBatch.VertexFactory->GetType(),
		bEnableSkyLight,
		BasePassShaders.VertexShader,
		BasePassShaders.PixelShader))
	{
		return false;
	}

	const bool bMaskedInEarlyPass = (MaterialResource.IsMasked() || MeshBatch.bDitheredLODTransition) && Scene && MaskedInEarlyPass(Scene->GetShaderPlatform());
	const bool bForcePassDrawRenderState = ((Flags & EFlags::ForcePassDrawRenderState) == EFlags::ForcePassDrawRenderState);

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);
	if (!bForcePassDrawRenderState)
	{
		if (bTranslucentBasePass)
		{
			MobileBasePass::SetTranslucentRenderState(DrawRenderState, MaterialResource, ShadingModels);
		}
		else if((MeshBatch.bUseForDepthPass && Scene->EarlyZPassMode == DDM_AllOpaque) || bMaskedInEarlyPass)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
		}
		else
		{
			const bool bEnableReceiveDecalOutput = ((Flags & EFlags::CanUseDepthStencil) == EFlags::CanUseDepthStencil);
			MobileBasePass::SetOpaqueRenderState(DrawRenderState, PrimitiveSceneProxy, MaterialResource, ShadingModels, bEnableReceiveDecalOutput && IsMobileHDR(), bUsesDeferredShading);
		}
	}

	FMeshDrawCommandSortKey SortKey; 
	if (bTranslucentBasePass)
	{
		const bool bIsUsingMobilePixelProjectedReflection = MaterialResource.IsUsingPlanarForwardReflections() 
															&& IsUsingMobilePixelProjectedReflection(GetFeatureLevelShaderPlatform(MaterialResource.GetFeatureLevel()));

		SortKey = CalculateTranslucentMeshStaticSortKey(PrimitiveSceneProxy, MeshBatch.MeshIdInPrimitive);
		// We always want water to be rendered first on mobile in order to mimic other renderers where it is opaque. We shift the other priorities by 1.
		// And we also want to render the meshes used for mobile pixel projected reflection first if it is opaque.
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
		SortKey.Translucent.Priority = ShadingModels.HasShadingModel(MSM_SingleLayerWater) || (!bIsTranslucent && bIsUsingMobilePixelProjectedReflection) ? uint16(0) : uint16(FMath::Clamp(uint32(SortKey.Translucent.Priority) + 1, 0u, uint32(USHRT_MAX)));
	}
	else
	{
		// Background primitives will be rendered last in masked/non-masked buckets
		bool bBackground = PrimitiveSceneProxy ? PrimitiveSceneProxy->TreatAsBackgroundForOcclusion() : false;
		// Default static sort key separates masked and non-masked geometry, generic mesh sorting will also sort by PSO
		// if platform wants front to back sorting, this key will be recomputed in InitViews
		SortKey = GetBasePassStaticSortKey(BlendMode, bBackground);
	}
	
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MaterialResource, OverrideSettings);
	ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MaterialResource, OverrideSettings);

	TMobileBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(LightMapElementData, bCanReceiveCSM);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		BasePassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
	return true;
}

FMeshPassProcessor* CreateMobileBasePassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(Scene->DefaultBasePassDepthStencilAccess);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

	const FMobileBasePassMeshProcessor::EFlags Flags = FMobileBasePassMeshProcessor::EFlags::CanUseDepthStencil
		| (MobileBasePassAlwaysUsesCSM(Scene->GetShaderPlatform()) ? FMobileBasePassMeshProcessor::EFlags::CanReceiveCSM : FMobileBasePassMeshProcessor::EFlags::None);

	return new FMobileBasePassMeshProcessor(EMeshPass::BasePass, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags);
}

FMeshPassProcessor* CreateMobileBasePassCSMProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(Scene->DefaultBasePassDepthStencilAccess);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

	// By default this processor will not cache anything. Only enable it when CSM culling is active
	FMobileBasePassMeshProcessor::EFlags Flags = FMobileBasePassMeshProcessor::EFlags::DoNotCache;
	if (!MobileBasePassAlwaysUsesCSM(Scene->GetShaderPlatform()))
	{
		Flags = FMobileBasePassMeshProcessor::EFlags::CanReceiveCSM | FMobileBasePassMeshProcessor::EFlags::CanUseDepthStencil;
	}
	
	return new FMobileBasePassMeshProcessor(EMeshPass::MobileBasePassCSM, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags);
}

FMeshPassProcessor* CreateMobileTranslucencyStandardPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);
	

	const FMobileBasePassMeshProcessor::EFlags Flags = FMobileBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FMobileBasePassMeshProcessor(EMeshPass::TranslucencyStandard, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_StandardTranslucency);
}

FMeshPassProcessor* CreateMobileTranslucencyAfterDOFProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);

	const FMobileBasePassMeshProcessor::EFlags Flags = FMobileBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FMobileBasePassMeshProcessor(EMeshPass::TranslucencyAfterDOF, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_TranslucencyAfterDOF);
}

FMeshPassProcessor* CreateMobileTranslucencyAllPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);

	const FMobileBasePassMeshProcessor::EFlags Flags = FMobileBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FMobileBasePassMeshProcessor(EMeshPass::TranslucencyAll, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_AllTranslucency);
}

FRegisterPassProcessorCreateFunction RegisterMobileBasePass(&CreateMobileBasePassProcessor, EShadingPath::Mobile, EMeshPass::BasePass, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileBasePassCSM(&CreateMobileBasePassCSMProcessor, EShadingPath::Mobile, EMeshPass::MobileBasePassCSM, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileTranslucencyAllPass(&CreateMobileTranslucencyAllPassProcessor, EShadingPath::Mobile, EMeshPass::TranslucencyAll, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileTranslucencyStandardPass(&CreateMobileTranslucencyStandardPassProcessor, EShadingPath::Mobile, EMeshPass::TranslucencyStandard, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileTranslucencyAfterDOFPass(&CreateMobileTranslucencyAfterDOFProcessor, EShadingPath::Mobile, EMeshPass::TranslucencyAfterDOF, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
// Skipping EMeshPass::TranslucencyAfterDOFModulate because dual blending is not supported on mobile
