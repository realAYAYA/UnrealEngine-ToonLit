// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveSceneProxy.cpp: Primitive scene proxy implementation.
=============================================================================*/

#include "PrimitiveSceneProxy.h"
#include "Engine/Brush.h"
#include "UObject/Package.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "Components/BrushComponent.h"
#include "SceneManagement.h"
#include "PrimitiveSceneInfo.h"
#include "Materials/Material.h"
#include "SceneManagement.h"
#include "VT/RuntimeVirtualTexture.h"
#include "PrimitiveInstanceUpdateCommand.h"
#include "InstanceUniformShaderParameters.h"
#include "NaniteSceneProxy.h" // TODO: PROG_RASTER
#include "ComponentRecreateRenderStateContext.h"

#if WITH_EDITOR
#include "FoliageHelper.h"
#include "ObjectCacheEventSink.h"
#endif

static TAutoConsoleVariable<int32> CVarForceSingleSampleShadowingFromStationary(
	TEXT("r.Shadow.ForceSingleSampleShadowingFromStationary"),
	0,
	TEXT("Whether to force all components to act as if they have bSingleSampleShadowFromStationaryLights enabled.  Useful for scalability when dynamic shadows are disabled."),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

static TAutoConsoleVariable<bool> CVarOptimizedWPO(
	TEXT("r.OptimizedWPO"),
	false,
	TEXT("Special mode where primitives can explicitly indicate if WPO should be evaluated or not as an optimization.\n")
	TEXT(" False ( 0): Ignore WPO evaluation flag, and always evaluate WPO.\n")
	TEXT(" True  ( 1): Only evaluate WPO on primitives with explicit activation."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarCacheWPOPrimitives(
	TEXT("r.Shadow.CacheWPOPrimitives"),
	0,
	TEXT("Whether primitives whose materials use World Position Offset should be considered movable for cached shadowmaps.\n")
	TEXT("Enablings this gives more correct, but slower whole scene shadows from materials that use WPO."),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

static TAutoConsoleVariable<int32> CVarVelocityEnableVertexDeformation(
	TEXT("r.Velocity.EnableVertexDeformation"),
	2,
	TEXT("Enables materials with World Position Offset and/or World Displacement to output velocities during velocity pass even when the actor has not moved. \n")
	TEXT("0=Off, 1=On, 2=Auto(Default). \n")
	TEXT("Auto setting is off if r.VelocityOutputPass=2, or else on. \n")
	TEXT("When r.VelocityOutputPass=2 this can incur a performance cost due to additional draw calls."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarVelocityForceOutput(
	TEXT("r.Velocity.ForceOutput"), 0,
	TEXT("Force velocity output on all primitives.\n")
	TEXT("This can incur a performance cost unless r.VelocityOutputPass=1.\n")
	TEXT("But it can be useful for testing where velocity output isn't being enabled as expected.\n")
	TEXT("0: Disabled (default)\n")
	TEXT("1: Enabled"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
	);


bool IsOptimizedWPO()
{
	return CVarOptimizedWPO.GetValueOnAnyThread() != 0;
}

bool CacheShadowDepthsFromPrimitivesUsingWPO()
{
	return CVarCacheWPOPrimitives.GetValueOnAnyThread(true) != 0;
}

bool SupportsCachingMeshDrawCommands(const FMeshBatch& MeshBatch)
{
	return
		// Cached mesh commands only allow for a single mesh element per batch.
		(MeshBatch.Elements.Num() == 1) &&

		// Vertex factory needs to support caching.
		MeshBatch.VertexFactory->GetType()->SupportsCachingMeshDrawCommands();
}

bool SupportsCachingMeshDrawCommands(const FMeshBatch& MeshBatch, ERHIFeatureLevel::Type FeatureLevel)
{
	if (SupportsCachingMeshDrawCommands(MeshBatch))
	{
		// External textures get mapped to immutable samplers (which are part of the PSO); the mesh must go through the dynamic path, as the media player might not have
		// valid textures/samplers the first few calls; once they're available the PSO needs to get invalidated and recreated with the immutable samplers.
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
		const FMaterialShaderMap* ShaderMap = Material.GetRenderingThreadShaderMap();
		if (ShaderMap)
		{
			const FUniformExpressionSet& ExpressionSet = ShaderMap->GetUniformExpressionSet();
			if (ExpressionSet.HasExternalTextureExpressions())
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool SupportsNaniteRendering(const FVertexFactory* RESTRICT VertexFactory, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy)
{
	// Volumetric self shadow mesh commands need to be generated every frame, as they depend on single frame uniform buffers with self shadow data.
	return VertexFactory->GetType()->SupportsNaniteRendering();
}

bool SupportsNaniteRendering(const FVertexFactory* RESTRICT VertexFactory, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, const FMaterialRenderProxy* MaterialRenderProxy, ERHIFeatureLevel::Type FeatureLevel)
{
	if (FeatureLevel >= ERHIFeatureLevel::SM5 && SupportsNaniteRendering(VertexFactory, PrimitiveSceneProxy))
	{
		const FMaterial& Material = MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
		const FMaterialShaderMap* ShaderMap = Material.GetRenderingThreadShaderMap();

		return (Material.IsUsedWithNanite() || Material.IsSpecialEngineMaterial()) &&
			Nanite::IsSupportedBlendMode(Material.GetBlendMode()) &&
			Nanite::IsSupportedMaterialDomain(Material.GetMaterialDomain()) &&
			(Nanite::IsWorldPositionOffsetSupported() || !ShaderMap->UsesWorldPositionOffset());
	}

	return false;
}


bool SupportsNaniteRendering(const FVertexFactoryType* RESTRICT VertexFactoryType, const class FMaterial& Material, ERHIFeatureLevel::Type FeatureLevel)
{
	if (FeatureLevel >= ERHIFeatureLevel::SM5 && VertexFactoryType->SupportsNaniteRendering())
	{
		const FMaterialShaderMap* ShaderMap = Material.GetGameThreadShaderMap();
		return (Material.IsUsedWithNanite() || Material.IsSpecialEngineMaterial()) &&
			Nanite::IsSupportedBlendMode(Material.GetBlendMode()) &&
			Nanite::IsSupportedMaterialDomain(Material.GetMaterialDomain()) &&
			(Nanite::IsWorldPositionOffsetSupported() || !ShaderMap->UsesWorldPositionOffset());
	}

	return false;
}


static bool VertexDeformationOutputsVelocity()
{
	static const auto CVarVelocityOutputPass = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VelocityOutputPass"));
	const bool bVertexDeformationOutputsVelocityDefault = CVarVelocityOutputPass && CVarVelocityOutputPass->GetInt() != 2;
	const int32 VertexDeformationOutputsVelocity = CVarVelocityEnableVertexDeformation.GetValueOnAnyThread();
	return VertexDeformationOutputsVelocity == 1 || (VertexDeformationOutputsVelocity == 2 && bVertexDeformationOutputsVelocityDefault);
}

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, FName InResourceName)
:
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	WireframeColor(FLinearColor::White)
,	LevelColor(FLinearColor::White)
,	PropertyColor(FLinearColor::White)
,	
#endif
	CustomPrimitiveData(InComponent->GetCustomPrimitiveData())
,	TranslucencySortPriority(FMath::Clamp(InComponent->TranslucencySortPriority, SHRT_MIN, SHRT_MAX))
,	TranslucencySortDistanceOffset(InComponent->TranslucencySortDistanceOffset)
,	Mobility(InComponent->Mobility)
,	LightmapType(InComponent->LightmapType)
,	StatId()
,	DrawInGame(InComponent->IsVisible())
,	DrawInEditor(InComponent->GetVisibleFlag())
,	bReceivesDecals(InComponent->bReceivesDecals)
,	bVirtualTextureMainPassDrawAlways(true)
,	bVirtualTextureMainPassDrawNever(false)
,	bOnlyOwnerSee(InComponent->bOnlyOwnerSee)
,	bOwnerNoSee(InComponent->bOwnerNoSee)
,	bParentSelected(InComponent->ShouldRenderSelected())
,	bIndividuallySelected(InComponent->IsComponentIndividuallySelected())
,	bLevelInstanceEditingState(InComponent->GetLevelInstanceEditingState())
,	bHovered(false)
,	bUseViewOwnerDepthPriorityGroup(InComponent->bUseViewOwnerDepthPriorityGroup)
,	StaticDepthPriorityGroup((uint8)InComponent->GetStaticDepthPriorityGroup())
,	ViewOwnerDepthPriorityGroup(InComponent->ViewOwnerDepthPriorityGroup)
,	bStaticLighting(InComponent->HasStaticLighting())
,	bVisibleInReflectionCaptures(InComponent->bVisibleInReflectionCaptures)
,	bVisibleInRealTimeSkyCaptures(InComponent->bVisibleInRealTimeSkyCaptures)
,	bVisibleInRayTracing(InComponent->bVisibleInRayTracing)
,	bRenderInDepthPass(InComponent->bRenderInDepthPass)
,	bRenderInMainPass(InComponent->bRenderInMainPass)
,	bForceHidden(false)
,	bCollisionEnabled(InComponent->IsCollisionEnabled())
,	bTreatAsBackgroundForOcclusion(InComponent->bTreatAsBackgroundForOcclusion)
,	bCanSkipRedundantTransformUpdates(true)
,	bGoodCandidateForCachedShadowmap(true)
,	bNeedsUnbuiltPreviewLighting(!InComponent->IsPrecomputedLightingValid())
,	bHasValidSettingsForStaticLighting(InComponent->HasValidSettingsForStaticLighting(false))
,	bWillEverBeLit(true)
	// Disable dynamic shadow casting if the primitive only casts indirect shadows, since dynamic shadows are always shadowing direct lighting
,	bCastDynamicShadow(InComponent->bCastDynamicShadow && InComponent->CastShadow && !InComponent->GetShadowIndirectOnly())
,	bEmissiveLightSource(InComponent->bEmissiveLightSource)
,   bAffectDynamicIndirectLighting(InComponent->bAffectDynamicIndirectLighting)
,	bAffectIndirectLightingWhileHidden(InComponent->bAffectDynamicIndirectLighting && InComponent->bAffectIndirectLightingWhileHidden)
,   bAffectDistanceFieldLighting(InComponent->bAffectDistanceFieldLighting)
,	bCastStaticShadow(InComponent->CastShadow && InComponent->bCastStaticShadow)
,	bCastVolumetricTranslucentShadow(InComponent->bCastDynamicShadow && InComponent->CastShadow && InComponent->bCastVolumetricTranslucentShadow)
,	bCastContactShadow(InComponent->CastShadow && InComponent->bCastContactShadow)
,	bCastDeepShadow(false)
,	bCastCapsuleDirectShadow(false)
,	bCastsDynamicIndirectShadow(false)
,	bCastHiddenShadow(InComponent->CastShadow && InComponent->bCastHiddenShadow)
,	bCastShadowAsTwoSided(InComponent->bCastShadowAsTwoSided)
,	bSelfShadowOnly(InComponent->bSelfShadowOnly)
,	bCastInsetShadow(InComponent->bSelfShadowOnly ? true : InComponent->bCastInsetShadow)	// Assumed to be enabled if bSelfShadowOnly is enabled.
,	bCastCinematicShadow(InComponent->bCastCinematicShadow)
,	bCastFarShadow(InComponent->bCastFarShadow)
,	bLightAttachmentsAsGroup(InComponent->bLightAttachmentsAsGroup)
,	bSingleSampleShadowFromStationaryLights(InComponent->bSingleSampleShadowFromStationaryLights)
,	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer(false)
,	bVFRequiresPrimitiveUniformBuffer(true)
,	bIsNaniteMesh(false)
,	bIsHierarchicalInstancedStaticMesh(false)
,	bIsLandscapeGrass(false)
,	bSupportsGPUScene(false)
,	bHasDeformableMesh(true)
,	bSupportsInstanceDataBuffer(false)
,	bShouldUpdateGPUSceneTransforms(true)
,	bEvaluateWorldPositionOffset(true)
,	bHasWorldPositionOffsetVelocity(false)
,	bAnyMaterialHasWorldPositionOffset(false)
,	bSupportsDistanceFieldRepresentation(false)
,	bSupportsMeshCardRepresentation(false)
,	bSupportsHeightfieldRepresentation(false)
,	bSupportsSortedTriangles(false)
,	bShouldNotifyOnWorldAddRemove(false)
,	bWantsSelectionOutline(true)
,	bVerifyUsedMaterials(true)
,	bHasPerInstanceRandom(false)
,	bHasPerInstanceCustomData(false)
,	bHasPerInstanceDynamicData(false)
,	bHasPerInstanceLMSMUVBias(false)
,	bHasPerInstanceLocalBounds(false)
,	bHasPerInstanceHierarchyOffset(false)
#if WITH_EDITOR
,	bHasPerInstanceEditorData(false)
#endif
,	bUseAsOccluder(InComponent->bUseAsOccluder)
,	bAllowApproximateOcclusion(InComponent->Mobility != EComponentMobility::Movable)
,	bSelectable(InComponent->bSelectable)
,	bHasPerInstanceHitProxies(InComponent->bHasPerInstanceHitProxies)
,	bUseEditorCompositing(InComponent->bUseEditorCompositing)
,	bIsBeingMovedByEditor(InComponent->bIsBeingMovedByEditor)
,	bReceiveMobileCSMShadows(InComponent->bReceiveMobileCSMShadows)
,	bRenderCustomDepth(InComponent->bRenderCustomDepth)
,	bVisibleInSceneCaptureOnly(InComponent->bVisibleInSceneCaptureOnly)
,	bHiddenInSceneCapture(InComponent->bHiddenInSceneCapture)
,	bRayTracingFarField(InComponent->bRayTracingFarField)
,	CustomDepthStencilValue(InComponent->CustomDepthStencilValue)
,	CustomDepthStencilWriteMask(FRendererStencilMaskEvaluation::ToStencilMask(InComponent->CustomDepthStencilWriteMask))
,	LightingChannelMask(GetLightingChannelMaskForStruct(InComponent->LightingChannels))
,	RayTracingGroupId(InComponent->GetRayTracingGroupId())
,	RayTracingGroupCullingPriority((uint8)InComponent->RayTracingGroupCullingPriority)
,	IndirectLightingCacheQuality(InComponent->IndirectLightingCacheQuality)
,	VirtualTextureLodBias(InComponent->VirtualTextureLodBias)
,	VirtualTextureCullMips(InComponent->VirtualTextureCullMips)
,	VirtualTextureMinCoverage(InComponent->VirtualTextureMinCoverage)
,	DynamicIndirectShadowMinVisibility(0)
,	DistanceFieldSelfShadowBias(0.0f)
,	PrimitiveComponentId(InComponent->ComponentId)
,	Scene(InComponent->GetScene())
,	PrimitiveSceneInfo(nullptr)
,	OwnerName(InComponent->GetOwner() ? InComponent->GetOwner()->GetFName() : NAME_None)
,	ResourceName(InResourceName)
,	LevelName(InComponent->GetOwner() ? InComponent->GetOwner()->GetLevel()->GetOutermost()->GetFName() : NAME_None)
#if WITH_EDITOR
// by default we are always drawn
,	HiddenEditorViews(0)
,	DrawInAnyEditMode(0)
,   bIsFoliage(false)
#endif
,	VisibilityId(InComponent->VisibilityId)
,	MaxDrawDistance(InComponent->CachedMaxDrawDistance > 0 ? InComponent->CachedMaxDrawDistance : FLT_MAX)
,	MinDrawDistance(InComponent->MinDrawDistance)
,	BoundsScale(InComponent->BoundsScale)
,	ComponentForDebuggingOnly(InComponent)
#if WITH_EDITOR
,	NumUncachedStaticLightingInteractions(0)
#endif
{
	check(Scene);

	// Initialize ForceHidden flag based on Level's visibility (only if Level bRequireFullVisibilityToRender is set)
	if (ULevel* Level = InComponent->GetComponentLevel())
	{
		bShouldNotifyOnWorldAddRemove = Level->bRequireFullVisibilityToRender;
		if (bShouldNotifyOnWorldAddRemove)
		{
			SetForceHidden(!Level->bIsVisible);
		}
	}

#if STATS
	{
		UObject const* StatObject = InComponent->AdditionalStatObject(); // prefer the additional object, this is usually the thing related to the component
		if (!StatObject)
		{
			StatObject = InComponent;
		}
		StatId = StatObject->GetStatID(true);
	}
#endif

	if (bNeedsUnbuiltPreviewLighting && !bHasValidSettingsForStaticLighting)
	{
		// Don't use unbuilt preview lighting for static components that have an invalid lightmap UV setup
		// Otherwise they would light differently in editor and in game, even after a lighting rebuild
		bNeedsUnbuiltPreviewLighting = false;
	}
	
	if(InComponent->GetOwner())
	{
		DrawInGame &= !(InComponent->GetOwner()->IsHidden());
		#if WITH_EDITOR
			DrawInEditor &= !InComponent->GetOwner()->IsHiddenEd();
		#endif

		if(bOnlyOwnerSee || bOwnerNoSee || bUseViewOwnerDepthPriorityGroup)
		{
			// Make a list of the actors which directly or indirectly own the component.
			for(const AActor* Owner = InComponent->GetOwner();Owner;Owner = Owner->GetOwner())
			{
				Owners.Add(Owner);
			}
		}

#if WITH_EDITOR
		// cache the actor's group membership
		HiddenEditorViews = InComponent->GetHiddenEditorViews();
		DrawInAnyEditMode = InComponent->GetOwner()->IsEditorOnly();
		bIsFoliage = FFoliageHelper::IsOwnedByFoliage(InComponent->GetOwner());
#endif
	}

	// Setup the runtime virtual texture information
	if (UseVirtualTexturing(GetScene().GetFeatureLevel()))
	{
		for (URuntimeVirtualTexture* VirtualTexture : InComponent->GetRuntimeVirtualTextures())
		{
			if (VirtualTexture != nullptr)
			{
				RuntimeVirtualTextures.Add(VirtualTexture);
				RuntimeVirtualTextureMaterialTypes.AddUnique(VirtualTexture->GetMaterialType());
			}
		}
	}

	// Conditionally remove from the main passes based on the runtime virtual texture setup
	const bool bRequestVirtualTexture = InComponent->GetRuntimeVirtualTextures().Num() > 0;
	if (bRequestVirtualTexture)
	{
		ERuntimeVirtualTextureMainPassType MainPassType = InComponent->GetVirtualTextureRenderPassType();
		bVirtualTextureMainPassDrawNever = MainPassType == ERuntimeVirtualTextureMainPassType::Never;
		bVirtualTextureMainPassDrawAlways = MainPassType == ERuntimeVirtualTextureMainPassType::Always;
	}

	// Modify max draw distance for main pass if we are using virtual texturing
	const bool bUseVirtualTexture = RuntimeVirtualTextures.Num() > 0;
	if (bUseVirtualTexture && InComponent->GetVirtualTextureMainPassMaxDrawDistance() > 0.f)
	{
		MaxDrawDistance = FMath::Min(MaxDrawDistance, InComponent->GetVirtualTextureMainPassMaxDrawDistance());
	}

#if WITH_EDITOR
	const bool bGetDebugMaterials = true;
	InComponent->GetUsedMaterials(UsedMaterialsForVerification, bGetDebugMaterials);

	FObjectCacheEventSink::NotifyUsedMaterialsChanged_Concurrent(InComponent, UsedMaterialsForVerification);
#endif

	bAnyMaterialHasWorldPositionOffset = false;
	{
		// Find if we have any WPO materials.
		ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();

		TArray<UMaterialInterface*> UsedMaterials;
		InComponent->GetUsedMaterials(UsedMaterials);
		for (const UMaterialInterface* MaterialInterface : UsedMaterials)
		{
			if (MaterialInterface)
			{
				if (MaterialInterface->GetRelevance_Concurrent(FeatureLevel).bUsesWorldPositionOffset)
				{
					bAnyMaterialHasWorldPositionOffset = true;
					break;
				}
			}
		}
	}

	bAlwaysHasVelocity = CVarVelocityForceOutput.GetValueOnAnyThread();
	if (!bAlwaysHasVelocity && InComponent->SupportsWorldPositionOffsetVelocity() && VertexDeformationOutputsVelocity() && bAnyMaterialHasWorldPositionOffset)
	{
		bHasWorldPositionOffsetVelocity = true;
	}
}

bool FPrimitiveSceneProxy::OnLevelAddedToWorld_RenderThread()
{
	check(bShouldNotifyOnWorldAddRemove);
	SetForceHidden(false);
	return false;
}

void FPrimitiveSceneProxy::OnLevelRemovedFromWorld_RenderThread()
{
	check(bShouldNotifyOnWorldAddRemove);
	SetForceHidden(true);
}

#if WITH_EDITOR
void FPrimitiveSceneProxy::SetUsedMaterialForVerification(const TArray<UMaterialInterface*>& InUsedMaterialsForVerification)
{
	check(IsInRenderingThread());

	UsedMaterialsForVerification = InUsedMaterialsForVerification;
}
#endif

FPrimitiveSceneProxy::~FPrimitiveSceneProxy()
{
	check(IsInRenderingThread());
}

HHitProxy* FPrimitiveSceneProxy::CreateHitProxies(UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	if(Component->GetOwner())
	{
		HHitProxy* ActorHitProxy;

		if (Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()))
		{
			ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe);
		}
		else
		{
#if WITH_EDITORONLY_DATA
			ActorHitProxy = new HActor(Component->GetOwner(), Component, Component->HitProxyPriority);
#else
			ActorHitProxy = new HActor(Component->GetOwner(), Component);
#endif
		}
		OutHitProxies.Add(ActorHitProxy);
		return ActorHitProxy;
	}
	else
	{
		return NULL;
	}
}

FPrimitiveViewRelevance FPrimitiveSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	return FPrimitiveViewRelevance();
}

void FPrimitiveSceneProxy::UpdateUniformBuffer()
{
	// stat disabled by default due to low-value/high-frequency
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_FPrimitiveSceneProxy_UpdateUniformBuffer);

	// Skip expensive primitive uniform buffer creation for proxies whose vertex factories only use GPUScene for primitive data
	if (DoesVFRequirePrimitiveUniformBuffer())
	{
		bool bHasPrecomputedVolumetricLightmap;
		FMatrix PreviousLocalToWorld;
		int32 SingleCaptureIndex;
		bool bOutputVelocity;

		Scene->GetPrimitiveUniformShaderParameters_RenderThread(
			PrimitiveSceneInfo,
			bHasPrecomputedVolumetricLightmap,
			PreviousLocalToWorld,
			SingleCaptureIndex,
			bOutputVelocity
		);

		bOutputVelocity |= AlwaysHasVelocity();

		FBoxSphereBounds PreSkinnedLocalBounds;
		GetPreSkinnedLocalBounds(PreSkinnedLocalBounds);

		// Update the uniform shader parameters.
		FPrimitiveUniformShaderParametersBuilder Builder = FPrimitiveUniformShaderParametersBuilder{}
			.Defaults()
				.LocalToWorld(LocalToWorld)
				.PreviousLocalToWorld(PreviousLocalToWorld)
				.ActorWorldPosition(ActorPosition)
				.WorldBounds(Bounds)
				.LocalBounds(LocalBounds)
				.InstanceLocalBounds(GetInstanceLocalBounds(0))
				.PreSkinnedLocalBounds(PreSkinnedLocalBounds)
				.ReceivesDecals(bReceivesDecals)
				.CacheShadowAsStatic(PrimitiveSceneInfo ? PrimitiveSceneInfo->ShouldCacheShadowAsStatic() : false)
				.OutputVelocity(bOutputVelocity)
				.EvaluateWorldPositionOffset(EvaluateWorldPositionOffset() && AnyMaterialHasWorldPositionOffset())
				.LightingChannelMask(GetLightingChannelMask())
				.LightmapDataIndex(PrimitiveSceneInfo ? PrimitiveSceneInfo->GetLightmapDataOffset() : 0)
				.LightmapUVIndex(GetLightMapCoordinateIndex())
				.SingleCaptureIndex(SingleCaptureIndex)
				.CustomPrimitiveData(GetCustomPrimitiveData())
				.HasCapsuleRepresentation(HasDynamicIndirectShadowCasterRepresentation())
				.UseSingleSampleShadowFromStationaryLights(UseSingleSampleShadowFromStationaryLights())
				.UseVolumetricLightmap(bHasPrecomputedVolumetricLightmap)
				.CastContactShadow(CastsContactShadow())
				.CastHiddenShadow(CastsHiddenShadow())
				.CastShadow(CastsDynamicShadow())
				.InstanceSceneDataOffset(PrimitiveSceneInfo ? PrimitiveSceneInfo->GetInstanceSceneDataOffset() : INDEX_NONE)
				.NumInstanceSceneDataEntries(PrimitiveSceneInfo ? PrimitiveSceneInfo->GetNumInstanceSceneDataEntries() : 0)
				.InstancePayloadDataOffset(PrimitiveSceneInfo ? PrimitiveSceneInfo->GetInstancePayloadDataOffset() : INDEX_NONE)
				.InstancePayloadDataStride(PrimitiveSceneInfo ? PrimitiveSceneInfo->GetInstancePayloadDataStride() : 0);				

		FVector2f InstanceDrawDistanceMinMax;
		if (GetInstanceDrawDistanceMinMax(InstanceDrawDistanceMinMax))
		{
			Builder.InstanceDrawDistance(InstanceDrawDistanceMinMax);
		}

		float WPODisableDistance;
		if (GetInstanceWorldPositionOffsetDisableDistance(WPODisableDistance))
		{
			Builder.InstanceWorldPositionOffsetDisableDistance(WPODisableDistance);
		}

		FPrimitiveUniformShaderParameters PrimitiveParams = Builder.Build();

		if (UniformBuffer.GetReference())
		{
			UniformBuffer.UpdateUniformBufferImmediate(PrimitiveParams);
		}
		else
		{
			UniformBuffer = TUniformBufferRef<FPrimitiveUniformShaderParameters>::CreateUniformBufferImmediate(PrimitiveParams, UniformBuffer_MultiFrame);
		}
	}

	if (PrimitiveSceneInfo)
	{
		PrimitiveSceneInfo->SetNeedsUniformBufferUpdate(false);
	}
}

uint32 FPrimitiveSceneProxy::GetPayloadDataStride() const
{
	static_assert(sizeof(FRenderTransform) == sizeof(float) * 3 * 4); // Sanity check
	static_assert(sizeof(FRenderBounds) == sizeof(float) * 3 * 2); // Sanity check

	// This count is per instance.
	uint32 PayloadDataCount = 0;

	// Random ID is packed into scene data currently
	if (FDataDrivenShaderPlatformInfo::GetSupportSceneDataCompressedTransforms(GMaxRHIShaderPlatform))
	{
		PayloadDataCount += HasPerInstanceDynamicData() ? 2 : 0;	// Compressed transform
	}
	else
	{
		PayloadDataCount += HasPerInstanceDynamicData() ? 3 : 0;	// FRenderTransform
	}
		
	// Hierarchy is packed in with local bounds if they are both present (almost always the case)
	if (HasPerInstanceLocalBounds())
	{
		PayloadDataCount += 2; // FRenderBounds and possibly uint32 for hierarchy offset & another uint32 for EditorData
	}
	else if (HasPerInstanceHierarchyOffset() || HasPerInstanceEditorData())
	{
		PayloadDataCount += 1; // uint32 for hierarchy offset (float4 packed) & instance editor data is packed in the same float4
	}

	PayloadDataCount += HasPerInstanceLMSMUVBias() ? 1 : 0; // FVector4

	if (HasPerInstanceCustomData())
	{
		const uint32 InstanceCount   = InstanceSceneData.Num();
		const uint32 CustomDataCount = InstanceCustomData.Num();
		if (InstanceCount > 0)
		{
			PayloadDataCount += FMath::DivideAndRoundUp(CustomDataCount / InstanceCount, 4u);
		}
	}

	return PayloadDataCount;
}

void FPrimitiveSceneProxy::SetTransform(const FMatrix& InLocalToWorld, const FBoxSphereBounds& InBounds, const FBoxSphereBounds& InLocalBounds, FVector InActorPosition)
{
	check(IsInRenderingThread());

	// Update the cached transforms.
	LocalToWorld = InLocalToWorld;
	bIsLocalToWorldDeterminantNegative = LocalToWorld.Determinant() < 0.0f;

	// Update the cached bounds.
	Bounds = InBounds;
	LocalBounds = InLocalBounds;
	ActorPosition = InActorPosition;
	
	// Update cached reflection capture.
	if (PrimitiveSceneInfo)
	{
		PrimitiveSceneInfo->bNeedsCachedReflectionCaptureUpdate = true;
	}
	
	UpdateUniformBuffer();
	
	// Notify the proxy's implementation of the change.
	OnTransformChanged();
}

void FPrimitiveSceneProxy::UpdateInstances_RenderThread(const FInstanceUpdateCmdBuffer& CmdBuffer, const FBoxSphereBounds& InBounds, const FBoxSphereBounds& InLocalBounds, const FBoxSphereBounds& InStaticMeshBounds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FPrimitiveSceneProxy::UpdateInstances_RenderThread");

	check(IsInRenderingThread());

	// Update the cached bounds.
	Bounds = InBounds;
	LocalBounds = InLocalBounds;

	UpdateUniformBuffer();
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// JIT initialize only when we go through UpdateInstances.
	// This will also clear the bits so we can reset the state for this frame.
	InstanceXFormUpdatedThisFrame.Init(false, InstanceSceneData.Num());
	InstanceCustomDataUpdatedThisFrame.Init(false, InstanceSceneData.Num());
#endif

	if (UseGPUScene(GetScene().GetShaderPlatform(), GetScene().GetFeatureLevel()))
	{
		const int32 PrevNumCustomDataFloats = InstanceSceneData.Num() ? InstanceCustomData.Num() / InstanceSceneData.Num() : 0;
		const int32 NumCustomDataFloats = CmdBuffer.NumCustomDataFloats;

		const bool bPreviouslyHadCustomFloatData = PrevNumCustomDataFloats > 0;
		const bool bHasCustomFloatData = CmdBuffer.NumCustomDataFloats > 0;

		// Apply all updates.
		for (const auto& Cmd : CmdBuffer.Cmds)
		{
			switch (Cmd.Type)
			{
				case FInstanceUpdateCmdBuffer::Update:
				{
					// update transform data.
					InstanceSceneData[Cmd.InstanceIndex].LocalToPrimitive = Cmd.XForm;
					InstanceDynamicData[Cmd.InstanceIndex].PrevLocalToPrimitive = Cmd.PreviousXForm;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					InstanceXFormUpdatedThisFrame[Cmd.InstanceIndex] = true;
#endif
				}
				break;
				case FInstanceUpdateCmdBuffer::CustomData:
				{
					check(bHasCustomFloatData);

					// update custom data because it changed.
					check(PrevNumCustomDataFloats == NumCustomDataFloats);
					const int32 DstCustomDataOffset = Cmd.InstanceIndex * NumCustomDataFloats;
					FMemory::Memcpy(&InstanceCustomData[DstCustomDataOffset], &Cmd.CustomDataFloats[0], NumCustomDataFloats * sizeof(float));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					InstanceCustomDataUpdatedThisFrame[Cmd.InstanceIndex] = true;
#endif
				}
				break;
#if WITH_EDITOR
				case FInstanceUpdateCmdBuffer::EditorData:
				{
					check(bHasPerInstanceEditorData);
					InstanceEditorData[Cmd.InstanceIndex] = FInstanceUpdateCmdBuffer::PackEditorData(Cmd.HitProxyColor, Cmd.bSelected);
				}
				break;
#endif //WITH_EDITOR
				default:
				break;
			};
		}

		// Build bit array of commands to remove.
		TBitArray<> RemoveBits;
		RemoveBits.Init(false, InstanceSceneData.Num());
		for (const auto& Cmd : CmdBuffer.Cmds)
		{
			if (Cmd.Type == FInstanceUpdateCmdBuffer::Hide)
			{
				RemoveBits[Cmd.InstanceIndex] = true;
			}
		}

		// Do removes.
		for (int32 i = 0; i < RemoveBits.Num(); ++i)
		{
			if (RemoveBits[i])
			{
				InstanceSceneData.RemoveAtSwap(i, 1, false);
				InstanceDynamicData.RemoveAtSwap(i, 1, false);

				if (bHasPerInstanceRandom)
				{
					InstanceRandomID.RemoveAtSwap(i, 1, false);
				}
				if (bHasPerInstanceLMSMUVBias)
				{
					InstanceLightShadowUVBias.RemoveAtSwap(i, 1, false);
				}

#if WITH_EDITOR
				if (bHasPerInstanceEditorData)
				{
					InstanceEditorData.RemoveAtSwap(i, 1, false);
				}
#endif

				// Only remove the custom float data from this instance if it previously had it.
				if (bPreviouslyHadCustomFloatData)
				{
					InstanceCustomData.RemoveAtSwap((i * PrevNumCustomDataFloats), PrevNumCustomDataFloats, false);
					check(InstanceCustomData.Num() == (PrevNumCustomDataFloats * InstanceSceneData.Num()));
				}

				RemoveBits.RemoveAtSwap(i);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				InstanceXFormUpdatedThisFrame.RemoveAtSwap(i);
				InstanceCustomDataUpdatedThisFrame.RemoveAtSwap(i);
#endif
				i--;
			}
		}

		// Apply all adds.
		for (const auto& Cmd : CmdBuffer.Cmds)
		{
			if (Cmd.Type == FInstanceUpdateCmdBuffer::Add)
			{
				InstanceSceneData.AddDefaulted_GetRef().LocalToPrimitive = Cmd.XForm;
				InstanceDynamicData.AddDefaulted_GetRef().PrevLocalToPrimitive = Cmd.PreviousXForm;

				if (bHasPerInstanceRandom)
				{
					InstanceRandomID.AddZeroed();
				}

				if (bHasPerInstanceLMSMUVBias)
				{
					InstanceLightShadowUVBias.AddZeroed();
				}

#if WITH_EDITOR
				if (bHasPerInstanceEditorData)
				{
					InstanceEditorData.AddZeroed();
				}
#endif
				if (bHasCustomFloatData)
				{
					const int32 DstCustomDataOffset = InstanceCustomData.AddUninitialized(NumCustomDataFloats);
					FMemory::Memcpy(&InstanceCustomData[DstCustomDataOffset], &Cmd.CustomDataFloats[0], NumCustomDataFloats * sizeof(float));
				}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				InstanceXFormUpdatedThisFrame.Add(true);
				InstanceCustomDataUpdatedThisFrame.Add(true);
#endif
			}
		}

		// #todo (jnadro) Do I still need to update this?
		InstanceLocalBounds.SetNumUninitialized(1);
		InstanceLocalBounds[0] = InStaticMeshBounds;
		bHasPerInstanceRandom = InstanceRandomID.Num() > 0;
		bHasPerInstanceCustomData = InstanceCustomData.Num() > 0;
		bHasPerInstanceDynamicData = InstanceDynamicData.Num() > 0;
		bHasPerInstanceLMSMUVBias = InstanceLightShadowUVBias.Num() > 0;
#if WITH_EDITOR
		bHasPerInstanceEditorData = InstanceEditorData.Num() > 0;
#endif

		// Ensure our data is in sync.
		check(InstanceSceneData.Num() == InstanceDynamicData.Num());
		check(InstanceCustomData.Num() == (NumCustomDataFloats * InstanceSceneData.Num()));
	}
}

bool FPrimitiveSceneProxy::WouldSetTransformBeRedundant_AnyThread(const FMatrix& InLocalToWorld, const FBoxSphereBounds& InBounds, const FBoxSphereBounds& InLocalBounds, const FVector& InActorPosition) const
{
	// Order is based on cheapest tests first.
	// Actor position checks if the actor has moved in the world, and local bounds
	// then covers if the actor's size has changed. Other tests then follow suit.

	// Can be called by any thread, so be careful about modifying this.

	if (ActorPosition != InActorPosition)
	{
		return false;
	}

	if (LocalBounds != InLocalBounds)
	{
		return false;
	}

	if (Bounds != InBounds)
	{
		return false;
	}

	if (LocalToWorld != InLocalToWorld)
	{
		return false;
	}

	return true;
}

void FPrimitiveSceneProxy::ApplyWorldOffset(FVector InOffset)
{
	FBoxSphereBounds NewBounds = FBoxSphereBounds(Bounds.Origin + InOffset, Bounds.BoxExtent, Bounds.SphereRadius);
	FBoxSphereBounds NewLocalBounds = LocalBounds;
	FVector NewActorPosition = ActorPosition + InOffset;
	FMatrix NewLocalToWorld = LocalToWorld.ConcatTranslation(InOffset);
	
	SetTransform(NewLocalToWorld, NewBounds, NewLocalBounds, NewActorPosition);
}

void FPrimitiveSceneProxy::ApplyLateUpdateTransform(const FMatrix& LateUpdateTransform)
{
	const FMatrix AdjustedLocalToWorld = LocalToWorld * LateUpdateTransform;
	SetTransform(AdjustedLocalToWorld, Bounds, LocalBounds, ActorPosition);
}

bool FPrimitiveSceneProxy::UseSingleSampleShadowFromStationaryLights() const 
{ 
	return bSingleSampleShadowFromStationaryLights 
		|| CVarForceSingleSampleShadowingFromStationary.GetValueOnRenderThread() != 0
		|| LightmapType == ELightmapType::ForceVolumetric; 
}

#if ENABLE_DRAW_DEBUG
void FPrimitiveSceneProxy::SetDebugMassData(const TArray<FDebugMassData>& InDebugMassData)
{
	DebugMassData = InDebugMassData;
}
#endif

/**
 * Updates selection for the primitive proxy. This is called in the rendering thread by SetSelection_GameThread.
 * @param bInSelected - true if the parent actor is selected in the editor
 */
void FPrimitiveSceneProxy::SetSelection_RenderThread(const bool bInParentSelected, const bool bInIndividuallySelected)
{
	check(IsInRenderingThread());

	const bool bWasSelected = IsSelected();
	bParentSelected = bInParentSelected;
	bIndividuallySelected = bInIndividuallySelected;
	const bool bIsSelected = IsSelected();

	// The renderer may have cached the selected state, let it know that this primitive is updated
	if ((bWasSelected && !bIsSelected) || 
		(bIsSelected && !bWasSelected))
	{
		GetScene().UpdatePrimitiveSelectedState_RenderThread(GetPrimitiveSceneInfo(), bIsSelected);
	}
}

/**
 * Updates selection for the primitive proxy. This simply sends a message to the rendering thread to call SetSelection_RenderThread.
 * This is called in the game thread as selection is toggled.
 * @param bInSelected - true if the parent actor is selected in the editor
 */
void FPrimitiveSceneProxy::SetSelection_GameThread(const bool bInParentSelected, const bool bInIndividuallySelected)
{
	check(IsInParallelGameThread() || IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	FPrimitiveSceneProxy* PrimitiveSceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetNewSelection)(
		[PrimitiveSceneProxy, bInParentSelected, bInIndividuallySelected](FRHICommandListImmediate& RHICmdList)
		{
			PrimitiveSceneProxy->SetSelection_RenderThread(bInParentSelected, bInIndividuallySelected);
		});
}

void FPrimitiveSceneProxy::SetLevelInstanceEditingState_RenderThread(const bool bInEditingState)
{
	check(IsInRenderingThread());
	bLevelInstanceEditingState = bInEditingState;
}

void FPrimitiveSceneProxy::SetLevelInstanceEditingState_GameThread(const bool bInEditingState)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	FPrimitiveSceneProxy* PrimitiveSceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetNewLevelInstanceEditingState)(
		[PrimitiveSceneProxy, bInEditingState](FRHICommandListImmediate& RHICmdList)
		{
			PrimitiveSceneProxy->SetLevelInstanceEditingState_RenderThread(bInEditingState);
		});
}

/**
* Set the custom depth enabled flag
*
* @param the new value
*/
void FPrimitiveSceneProxy::SetCustomDepthEnabled_GameThread(const bool bInRenderCustomDepth)
{
	check(IsInGameThread());

	ENQUEUE_RENDER_COMMAND(FSetCustomDepthEnabled)(
		[this, bInRenderCustomDepth](FRHICommandList& RHICmdList)
		{
			this->SetCustomDepthEnabled_RenderThread(bInRenderCustomDepth);
	});
}

/**
* Set the custom depth enabled flag (RENDER THREAD)
*
* @param the new value
*/
void FPrimitiveSceneProxy::SetCustomDepthEnabled_RenderThread(const bool bInRenderCustomDepth)
{
	check(IsInRenderingThread());
	bRenderCustomDepth = bInRenderCustomDepth;
}

/**
* Set the custom depth stencil value
*
* @param the new value
*/
void FPrimitiveSceneProxy::SetCustomDepthStencilValue_GameThread(const int32 InCustomDepthStencilValue)
{
	check(IsInGameThread());

	ENQUEUE_RENDER_COMMAND(FSetCustomDepthStencilValue)(
		[this, InCustomDepthStencilValue](FRHICommandList& RHICmdList)
	{
		this->SetCustomDepthStencilValue_RenderThread(InCustomDepthStencilValue);
	});
}

/**
* Set the custom depth stencil value (RENDER THREAD)
*
* @param the new value
*/
void FPrimitiveSceneProxy::SetCustomDepthStencilValue_RenderThread(const int32 InCustomDepthStencilValue)
{
	check(IsInRenderingThread());
	CustomDepthStencilValue = InCustomDepthStencilValue;
}

void FPrimitiveSceneProxy::SetDistanceFieldSelfShadowBias_RenderThread(float NewBias)
{
	DistanceFieldSelfShadowBias = NewBias;
}

/**
 * Updates hover state for the primitive proxy. This is called in the rendering thread by SetHovered_GameThread.
 * @param bInHovered - true if the parent actor is hovered
 */
void FPrimitiveSceneProxy::SetHovered_RenderThread(const bool bInHovered)
{
	check(IsInRenderingThread());
	bHovered = bInHovered;
}

/**
 * Updates hover state for the primitive proxy. This simply sends a message to the rendering thread to call SetHovered_RenderThread.
 * This is called in the game thread as hover state changes
 * @param bInHovered - true if the parent actor is hovered
 */
void FPrimitiveSceneProxy::SetHovered_GameThread(const bool bInHovered)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	FPrimitiveSceneProxy* PrimitiveSceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetNewHovered)(
		[PrimitiveSceneProxy, bInHovered](FRHICommandListImmediate& RHICmdList)
		{
			PrimitiveSceneProxy->SetHovered_RenderThread(bInHovered);
		});
}

void FPrimitiveSceneProxy::SetLightingChannels_GameThread(FLightingChannels LightingChannels)
{
	check(IsInGameThread());

	FPrimitiveSceneProxy* PrimitiveSceneProxy = this;
	const uint8 LocalLightingChannelMask = GetLightingChannelMaskForStruct(LightingChannels);
	ENQUEUE_RENDER_COMMAND(SetLightingChannelsCmd)(
		[PrimitiveSceneProxy, LocalLightingChannelMask](FRHICommandListImmediate& RHICmdList)
	{
		PrimitiveSceneProxy->LightingChannelMask = LocalLightingChannelMask;
		PrimitiveSceneProxy->GetPrimitiveSceneInfo()->SetNeedsUniformBufferUpdate(true);
	});
}

#if ENABLE_DRAW_DEBUG
void FPrimitiveSceneProxy::FDebugMassData::DrawDebugMass(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM) const
{
	const FQuat MassOrientationToWorld = ElemTM.GetRotation() * LocalTensorOrientation;
	const FVector COMWorldPosition = ElemTM.TransformPosition(LocalCenterOfMass);

	const float Size = 15.f;
	const FVector XAxis = MassOrientationToWorld * FVector(1.f, 0.f, 0.f);
	const FVector YAxis = MassOrientationToWorld * FVector(0.f, 1.f, 0.f);
	const FVector ZAxis = MassOrientationToWorld * FVector(0.f, 0.f, 1.f);

	DrawCircle(PDI, COMWorldPosition, XAxis, YAxis, FColor(255, 255, 100), Size, 25, SDPG_World);
	DrawCircle(PDI, COMWorldPosition, ZAxis, YAxis, FColor(255, 255, 100), Size, 25, SDPG_World);

	const float InertiaSize = FMath::Max(MassSpaceInertiaTensor.Size(), UE_KINDA_SMALL_NUMBER);
	const float XSize = Size * MassSpaceInertiaTensor.X / InertiaSize;
	const float YSize = Size * MassSpaceInertiaTensor.Y / InertiaSize;
	const float ZSize = Size * MassSpaceInertiaTensor.Z / InertiaSize;

	const float Thickness = 2.f * FMath::Sqrt(3.f);	//We end up normalizing by inertia size. If the sides are all even we'll end up dividing by sqrt(3) since 1/sqrt(1+1+1)
	const float XThickness = Thickness * MassSpaceInertiaTensor.X / InertiaSize;
	const float YThickness = Thickness * MassSpaceInertiaTensor.Y / InertiaSize;
	const float ZThickness = Thickness * MassSpaceInertiaTensor.Z / InertiaSize;

	PDI->DrawLine(COMWorldPosition + XAxis * Size, COMWorldPosition - Size * XAxis, FColor(255, 0, 0), SDPG_World, XThickness);
	PDI->DrawLine(COMWorldPosition + YAxis * Size, COMWorldPosition - Size * YAxis, FColor(0, 255, 0), SDPG_World, YThickness);
	PDI->DrawLine(COMWorldPosition + ZAxis * Size, COMWorldPosition - Size * ZAxis, FColor(0, 0, 255), SDPG_World, ZThickness);
}
#endif

void FPrimitiveSceneProxy::UpdateDefaultInstanceSceneData()
{
	check(InstanceSceneData.Num() <= 1);
	InstanceSceneData.SetNumUninitialized(1);
	FPrimitiveInstance& DefaultInstance = InstanceSceneData[0];
	DefaultInstance.LocalToPrimitive.SetIdentity();
}

bool FPrimitiveSceneProxy::DrawInVirtualTextureOnly(bool bEditor) const
{
	if (bVirtualTextureMainPassDrawAlways)
	{
		return false;
	}
	else if (bVirtualTextureMainPassDrawNever)
	{
		return true;
	}
	// Conditional path tests the flags stored on scene virtual texture.
	uint8 bHideMaskEditor, bHideMaskGame;
	Scene->GetRuntimeVirtualTextureHidePrimitiveMask(bHideMaskEditor, bHideMaskGame);
	const uint8 bHideMask = bEditor ? bHideMaskEditor : bHideMaskGame;
	const uint8 RuntimeVirtualTextureMask = GetPrimitiveSceneInfo()->GetRuntimeVirtualTextureFlags().RuntimeVirtualTextureMask;
	return (RuntimeVirtualTextureMask & bHideMask) != 0;
}

void FPrimitiveSceneProxy::EnableGPUSceneSupportFlags()
{
	const ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();
	const bool bUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel);
	const bool bMobilePath = (FeatureLevel == ERHIFeatureLevel::ES3_1);

	// Skip primitive uniform buffer if we will be using local vertex factory which gets it's data from GPUScene.
	// Vertex shaders on mobile may still use PrimitiveUB with GPUScene enabled
	bVFRequiresPrimitiveUniformBuffer = !bUseGPUScene || bMobilePath;
	// For mobile we always assume that proxy does not support GPUScene, as it depends on vertex factory setup which happens later
	bSupportsGPUScene = bMobilePath ? false : bUseGPUScene;
}

/**
 * Updates the hidden editor view visibility map on the game thread which just enqueues a command on the render thread
 */
void FPrimitiveSceneProxy::SetHiddenEdViews_GameThread( uint64 InHiddenEditorViews )
{
	check(IsInGameThread());

	FPrimitiveSceneProxy* PrimitiveSceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetEditorVisibility)(
		[PrimitiveSceneProxy, InHiddenEditorViews](FRHICommandListImmediate& RHICmdList)
		{
			PrimitiveSceneProxy->SetHiddenEdViews_RenderThread(InHiddenEditorViews);
		});
}

/**
 * Updates the hidden editor view visibility map on the render thread 
 */
void FPrimitiveSceneProxy::SetHiddenEdViews_RenderThread( uint64 InHiddenEditorViews )
{
#if WITH_EDITOR
	check(IsInRenderingThread());
	HiddenEditorViews = InHiddenEditorViews;
#endif
}

#if WITH_EDITOR
void FPrimitiveSceneProxy::SetIsBeingMovedByEditor_GameThread(bool bIsBeingMoved)
{
	check(IsInGameThread());

	FPrimitiveSceneProxy* PrimitiveSceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetIsBeingMovedByEditor)(
		[PrimitiveSceneProxy, bIsBeingMoved](FRHICommandListImmediate& RHICmdList)
		{
			PrimitiveSceneProxy->bIsBeingMovedByEditor = bIsBeingMoved;
			PrimitiveSceneProxy->GetScene().UpdatePrimitiveVelocityState_RenderThread(PrimitiveSceneProxy->GetPrimitiveSceneInfo(), bIsBeingMoved);
		});
}
#endif

void FPrimitiveSceneProxy::SetEvaluateWorldPositionOffset_GameThread(bool bEvaluate)
{
	check(IsInGameThread());

	FPrimitiveSceneProxy* PrimitiveSceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetEvaluateWorldPositionOffset)
		([PrimitiveSceneProxy, bEvaluate](FRHICommandList& RHICmdList)
	{
		const bool bOptimizedWPO = CVarOptimizedWPO.GetValueOnRenderThread();
		const bool bWPOEvaluate = !bOptimizedWPO || bEvaluate;

		if (PrimitiveSceneProxy->bEvaluateWorldPositionOffset != bWPOEvaluate)
		{
			PrimitiveSceneProxy->bEvaluateWorldPositionOffset = bWPOEvaluate;
			PrimitiveSceneProxy->GetScene().RequestGPUSceneUpdate(*PrimitiveSceneProxy->GetPrimitiveSceneInfo(), EPrimitiveDirtyState::ChangedOther);
		}
	});
}

void FPrimitiveSceneProxy::SetCollisionEnabled_GameThread(const bool bNewEnabled)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread to change draw state
	FPrimitiveSceneProxy* PrimSceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetCollisionEnabled)(
		[PrimSceneProxy, bNewEnabled](FRHICommandListImmediate& RHICmdList)
		{
			PrimSceneProxy->SetCollisionEnabled_RenderThread(bNewEnabled);
		});
}

void FPrimitiveSceneProxy::SetCollisionEnabled_RenderThread(const bool bNewEnabled)
{
	check(IsInRenderingThread());
	bCollisionEnabled = bNewEnabled;
}

/** @return True if the primitive is visible in the given View. */
bool FPrimitiveSceneProxy::IsShown(const FSceneView* View) const
{
	// If primitive is forcibly hidden
	if (IsForceHidden())
	{
		return false;
	}

#if WITH_EDITOR
	// Don't draw editor specific actors during game mode
	if (View->Family->EngineShowFlags.Game)
	{
		if (DrawInAnyEditMode)
		{
			return false;
		}
	}

	if (bIsFoliage && !View->Family->EngineShowFlags.InstancedFoliage)
	{
		return false;
	}

	// After checking for VR/Desktop Edit mode specific actors, check for Editor vs. Game
	if(View->Family->EngineShowFlags.Editor)
	{
		if(!DrawInEditor)
		{
			return false;
		}

		// if all of it's groups are hidden in this view, don't draw
		if ((HiddenEditorViews & View->EditorViewBitflag) != 0)
		{
			return false;
		}

		// If we are in a collision view, hide anything which doesn't have collision enabled
		const bool bCollisionView = (View->Family->EngineShowFlags.CollisionVisibility || View->Family->EngineShowFlags.CollisionPawn);
		if (bCollisionView && !IsCollisionEnabled())
		{
			return false;
		}

		if (DrawInVirtualTextureOnly(true) && !View->bIsVirtualTexture && !View->Family->EngineShowFlags.VirtualTexturePrimitives && !IsSelected())
		{
			return false;
		}
	}
	else
#endif
	{

		if(!DrawInGame
#if WITH_EDITOR
			|| (!View->bIsGameView && View->Family->EngineShowFlags.Game && !DrawInEditor)	// ..."G" mode in editor viewport. covers the case when the primitive must be rendered for the voxelization pass, but the user has chosen to hide the primitive from view.
#endif
			)
		{
			return false;
		}

		if (DrawInVirtualTextureOnly(false) && !View->bIsVirtualTexture)
		{
			return false;
		}

		const bool bOwnersContain = Owners.Contains(View->ViewActor);
		if (bOnlyOwnerSee && !bOwnersContain)
		{
			return false;
		}

		if (bOwnerNoSee && bOwnersContain)
		{
			return false;
		}
	}

	if (View->bIsSceneCapture)
	{
		if (IsHiddenInSceneCapture())
			return false;
	}
	else
	{
		if (IsVisibleInSceneCaptureOnly())
			return false;
	}

	return true;
}

/** @return True if the primitive is casting a shadow. */
bool FPrimitiveSceneProxy::IsShadowCast(const FSceneView* View) const
{
	check(PrimitiveSceneInfo);

	if (!CastsStaticShadow() && !CastsDynamicShadow())
	{
		return false;
	}

	if (!CastsHiddenShadow())
	{
		// Primitives that are hidden in the game don't cast a shadow.
		if (!DrawInGame)
		{
			return false;
		}
		
		if (View->HiddenPrimitives.Contains(PrimitiveComponentId))
		{
			return false;
		}

		if (View->ShowOnlyPrimitives.IsSet() && !View->ShowOnlyPrimitives->Contains(PrimitiveComponentId))
		{
			return false;
		}

#if WITH_EDITOR
		// For editor views, we use a show flag to determine whether shadows from editor-hidden actors are desired.
		if( View->Family->EngineShowFlags.Editor )
		{
			if(!DrawInEditor)
			{
				return false;
			}
		
			// if all of it's groups are hidden in this view, don't draw
			if ((HiddenEditorViews & View->EditorViewBitflag) != 0)
			{
				return false;
			}
		}
#endif	//#if WITH_EDITOR

		// If primitive is forcibly hidden
		if (IsForceHidden())
		{
			return false;
		}

		if (DrawInVirtualTextureOnly(View->Family->EngineShowFlags.Editor) && !View->bIsVirtualTexture)
		{
			return false;
		}

		// In the OwnerSee cases, we still want to respect hidden shadows...
		// This assumes that bCastHiddenShadow trumps the owner see flags.
		const bool bOwnersContain = Owners.Contains(View->ViewActor);
		if (bOnlyOwnerSee && !bOwnersContain)
		{
			return false;
		}

		if (bOwnerNoSee && bOwnersContain)
		{
			return false;
		}
	}

	if (View->bIsSceneCapture && IsHiddenInSceneCapture())
	{
		return false;
	}

	if (!View->bIsSceneCapture && IsVisibleInSceneCaptureOnly())
	{
		return false;
	}

	return true;
}

void FPrimitiveSceneProxy::RenderBounds(
	FPrimitiveDrawInterface* PDI, 
	const FEngineShowFlags& EngineShowFlags, 
	const FBoxSphereBounds& InBounds, 
	bool bRenderInEditor) const
{
	if (EngineShowFlags.Bounds && (EngineShowFlags.Game || bRenderInEditor))
	{
		// Draw the static mesh's bounding box and sphere.
		const ESceneDepthPriorityGroup DrawBoundsDPG = SDPG_World;
		DrawWireBox(PDI,InBounds.GetBox(), FColor(72,72,255), DrawBoundsDPG);
		DrawCircle(PDI, InBounds.Origin, FVector(1, 0, 0), FVector(0, 1, 0), FColor::Yellow, InBounds.SphereRadius, 32, DrawBoundsDPG);
		DrawCircle(PDI, InBounds.Origin, FVector(1, 0, 0), FVector(0, 0, 1), FColor::Yellow, InBounds.SphereRadius, 32, DrawBoundsDPG);
		DrawCircle(PDI, InBounds.Origin, FVector(0, 1, 0), FVector(0, 0, 1), FColor::Yellow, InBounds.SphereRadius, 32, DrawBoundsDPG);
		// render the Local bounds in a red color
		DrawWireBox(PDI, GetLocalToWorld(), GetLocalBounds().GetBox(), FColor(255, 72, 72), DrawBoundsDPG);
	}
}

bool FPrimitiveSceneProxy::VerifyUsedMaterial(const FMaterialRenderProxy* MaterialRenderProxy) const
{
	// Only verify GetUsedMaterials if uncooked and we can compile shaders, because FShaderCompilingManager::PropagateMaterialChangesToPrimitives is what needs GetUsedMaterials to be accurate
#if WITH_EDITOR
	if (bVerifyUsedMaterials)
	{
		const UMaterialInterface* MaterialInterface = MaterialRenderProxy->GetMaterialInterface();

		if (MaterialInterface 
			&& !UsedMaterialsForVerification.Contains(MaterialInterface)
			&& MaterialInterface != UMaterial::GetDefaultMaterial(MD_Surface))
		{
			// Shader compiling uses GetUsedMaterials to detect which components need their scene proxy recreated, so we can only render with materials present in that list
			ensureMsgf(false, TEXT("PrimitiveComponent tried to render with Material %s, which was not present in the component's GetUsedMaterials results\n    Owner: %s, Resource: %s"), *MaterialInterface->GetName(), *GetOwnerName().ToString(), *GetResourceName().ToString());
			return false;
		}
	}
#endif
	return true;
}

void FPrimitiveSceneProxy::DrawArc(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const float Height, const uint32 Segments, const FLinearColor& Color, uint8 DepthPriorityGroup, const float Thickness, const bool bScreenSpace)
{
	if (Segments == 0)
	{
		return;
	}

	const float ARC_PTS_SCALE = 1.0f / (float)Segments;
	const float DepthBias = 0.0f;

	const float X0 = Start.X;
	const float Y0 = Start.Y;
	const float Z0 = Start.Z;
	const float Dx = End.X - X0;
	const float Dy = End.Y - Y0;
	const float Dz = End.Z - Z0;
	const float Length = FMath::Sqrt(Dx*Dx + Dy*Dy + Dz*Dz);
	float Px = X0, Py = Y0, Pz = Z0;
	for (uint32 i = 1; i <= Segments; ++i)
	{
		const float U = i * ARC_PTS_SCALE;
		const float X = X0 + Dx * U;
		const float Y = Y0 + Dy * U;
		const float Z = Z0 + Dz * U + (Length*Height) * (1-(U*2-1)*(U*2-1));

		PDI->DrawLine( FVector(Px, Py, Pz), FVector(X, Y, Z), Color, SDPG_World, Thickness, DepthBias, bScreenSpace);

		Px = X; Py = Y; Pz = Z;
	}
}

void FPrimitiveSceneProxy::DrawArrowHead(FPrimitiveDrawInterface* PDI, const FVector& Tip, const FVector& Origin, const float Size, const FLinearColor& Color, uint8 DepthPriorityGroup, const float Thickness, const bool bScreenSpace)
{
	const FVector Forward = (Origin - Tip).GetUnsafeNormal();
	const FVector Right = FVector::CrossProduct(Forward, FVector::UpVector);
	const float HalfWidth = Size / 3.0f;
	const float DepthBias = 0.0f;
	PDI->DrawLine(Tip, Tip + Forward * Size + Right * HalfWidth, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
	PDI->DrawLine(Tip, Tip + Forward * Size - Right * HalfWidth, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
}


#if WITH_EDITORONLY_DATA
bool FPrimitiveSceneProxy::GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const
{
	const bool bUseNewMetrics = CVarStreamingUseNewMetrics.GetValueOnRenderThread() != 0;

	const FBoxSphereBounds& PrimBounds = GetBounds();

	FVector ViewToObject = PrimBounds.Origin - ViewOrigin;

	float DistSqMinusRadiusSq = 0;
	if (bUseNewMetrics)
	{
		ViewToObject = ViewToObject.GetAbs();
		FVector BoxViewToObject = ViewToObject.ComponentMin(PrimBounds.BoxExtent);
		DistSqMinusRadiusSq = FVector::DistSquared(BoxViewToObject, ViewToObject);
	}
	else
	{
		float Distance = ViewToObject.Size();
		DistSqMinusRadiusSq = FMath::Square(Distance) - FMath::Square(PrimBounds.SphereRadius);
	}

	PrimitiveDistance = FMath::Sqrt(FMath::Max<float>(1.f, DistSqMinusRadiusSq));
	return true;
}

bool FPrimitiveSceneProxy::GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const 
{ 
	return false; 
}

bool FPrimitiveSceneProxy::GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4f* OneOverScales, FIntVector4* UVChannelIndices) const 
{ 
	return false; 
}

#endif // WITH_EDITORONLY_DATA

#if RHI_RAYTRACING
ERayTracingPrimitiveFlags FPrimitiveSceneProxy::GetCachedRayTracingInstance(FRayTracingInstance& OutRayTracingInstance)
{
	if (!IsRayTracingRelevant())
	{
		// The entire proxy type will be skipped. Make sure IsRayTracingRelevant() only depends on proxy type (vtable)
		return ERayTracingPrimitiveFlags::UnsupportedProxyType;
	}

	if (!(IsVisibleInRayTracing() && ShouldRenderInMainPass() && (IsDrawnInGame() || AffectsIndirectLightingWhileHidden())) && !IsRayTracingFarField())
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	static const auto RayTracingStaticMeshesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.StaticMeshes"));

	if (IsRayTracingStaticRelevant() && RayTracingStaticMeshesCVar && RayTracingStaticMeshesCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	static const auto RayTracingHISMCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.HierarchicalInstancedStaticMesh"));

	if (bIsHierarchicalInstancedStaticMesh && RayTracingHISMCVar && RayTracingHISMCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	static const auto RayTracingLandscapeGrassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.LandscapeGrass"));

	if (bIsLandscapeGrass && RayTracingLandscapeGrassCVar && RayTracingLandscapeGrassCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	// Visible in ray tracing. Default to fully dynamic (no caching)
	ERayTracingPrimitiveFlags ResultFlags = ERayTracingPrimitiveFlags::Dynamic;

	if (IsRayTracingStaticRelevant())
	{
		// overwrite flag if static
		ResultFlags = ERayTracingPrimitiveFlags::StaticMesh | ERayTracingPrimitiveFlags::ComputeLOD;
	}

	if (IsRayTracingFarField())
	{
		ResultFlags |= ERayTracingPrimitiveFlags::FarField;
	}

	return ResultFlags;
}
#endif