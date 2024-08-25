// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Package.h"
#include "VT/RuntimeVirtualTexture.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/Level.h"
#include "Components/PrimitiveComponent.h"

struct FPrimitiveSceneProxyDesc
{	
	FPrimitiveSceneProxyDesc()
	{
		CastShadow = false;
		bReceivesDecals = true;
		bOnlyOwnerSee = false;
		bOwnerNoSee = false;
		bLevelInstanceEditingState = false;
		bUseViewOwnerDepthPriorityGroup = false;
		bVisibleInReflectionCaptures = true;
		bVisibleInRealTimeSkyCaptures = true;
		bVisibleInRayTracing = true;
		bRenderInDepthPass = true;
		bRenderInMainPass = true;
		bTreatAsBackgroundForOcclusion = false;
		bCastDynamicShadow = true;
		bCastStaticShadow = true;
		bEmissiveLightSource = false;
		bAffectDynamicIndirectLighting = true;
		bAffectIndirectLightingWhileHidden = false;
		bAffectDistanceFieldLighting = true;
		bCastVolumetricTranslucentShadow = false;
		bCastContactShadow = true;
		bCastHiddenShadow = false;
		bCastShadowAsTwoSided = false;
		bSelfShadowOnly = false;
		bCastInsetShadow = false;
		bCastCinematicShadow = false;
		bCastFarShadow = false;
		bLightAttachmentsAsGroup = false;
		bSingleSampleShadowFromStationaryLights = false;
		bUseAsOccluder = false;
		bSelectable = true;
		bHasPerInstanceHitProxies = false;
		bUseEditorCompositing = false;
		bIsBeingMovedByEditor = false;
		bReceiveMobileCSMShadows = true;
		bRenderCustomDepth = false;
		bVisibleInSceneCaptureOnly = false;
		bHiddenInSceneCapture = false;
		bRayTracingFarField = false;
		bIsVisible = true;
		bIsVisibleEditor = true;
		bSelected = false;
		bIndividuallySelected = false;
		bCollisionEnabled = false;
		bIsHidden = false;
		bIsHiddenEd = false;
		bSupportsWorldPositionOffsetVelocity = true;
		bIsOwnerEditorOnly = false;
		bIsInstancedStaticMesh = false;
		bHoldout = false;

		bHasStaticLighting = false;
		bHasValidSettingsForStaticLighting = false;
		bIsPrecomputedLightingValid = false;
		bShadowIndirectOnly = false;
		bShouldRenderProxyFallbackToDefaultMaterial = false;
		bShouldRenderSelected = false;

#if WITH_EDITOR
		bIsOwnedByFoliage = false;
#endif
	}

	ENGINE_API FPrimitiveSceneProxyDesc(const UPrimitiveComponent*);

	void InitializeFrom(const UPrimitiveComponent*);
	
	virtual ~FPrimitiveSceneProxyDesc() = default;

	uint32 CastShadow : 1;
	uint32 bReceivesDecals : 1;
	uint32 bOnlyOwnerSee : 1;
	uint32 bOwnerNoSee : 1;
	uint32 bLevelInstanceEditingState : 1;
	uint32 bUseViewOwnerDepthPriorityGroup  : 1;
	uint32 bVisibleInReflectionCaptures : 1;
	uint32 bVisibleInRealTimeSkyCaptures : 1;
	uint32 bVisibleInRayTracing : 1;
	uint32 bRenderInDepthPass : 1;
	uint32 bRenderInMainPass : 1;
	uint32 bTreatAsBackgroundForOcclusion : 1;
	uint32 bCastDynamicShadow : 1;
	uint32 bCastStaticShadow : 1;
	uint32 bEmissiveLightSource : 1;
	uint32 bAffectDynamicIndirectLighting : 1;
	uint32 bAffectIndirectLightingWhileHidden : 1;
	uint32 bAffectDistanceFieldLighting : 1;
	uint32 bCastVolumetricTranslucentShadow : 1;
	uint32 bCastContactShadow : 1;
	uint32 bCastHiddenShadow : 1;
	uint32 bCastShadowAsTwoSided : 1;
	uint32 bSelfShadowOnly : 1;
	uint32 bCastInsetShadow : 1;
	uint32 bCastCinematicShadow : 1;
	uint32 bCastFarShadow : 1;
	uint32 bLightAttachmentsAsGroup : 1;
	uint32 bSingleSampleShadowFromStationaryLights : 1;
	uint32 bUseAsOccluder : 1;
	uint32 bSelectable : 1;
	uint32 bHasPerInstanceHitProxies : 1;
	uint32 bUseEditorCompositing : 1;
	uint32 bIsBeingMovedByEditor : 1;
	uint32 bReceiveMobileCSMShadows : 1;
	uint32 bRenderCustomDepth : 1;
	uint32 bVisibleInSceneCaptureOnly : 1;
	uint32 bHiddenInSceneCapture : 1;
	uint32 bRayTracingFarField : 1;
	uint32 bHoldout : 1;

	// not mirrored from UPrimitiveComponent
	uint32 bIsVisible : 1;
	uint32 bIsVisibleEditor : 1; 
	uint32 bSelected : 1;
	uint32 bIndividuallySelected : 1;
	uint32 bShouldRenderSelected : 1;
	uint32 bCollisionEnabled : 1;
	uint32 bIsHidden : 1;
	uint32 bIsHiddenEd : 1;
	uint32 bSupportsWorldPositionOffsetVelocity : 1;
	uint32 bIsOwnerEditorOnly : 1;
	uint32 bIsInstancedStaticMesh : 1;
	uint32 bHasStaticLighting : 1;
	uint32 bHasValidSettingsForStaticLighting : 1;
	uint32 bIsPrecomputedLightingValid : 1;
	uint32 bShadowIndirectOnly: 1;
	uint32 bShouldRenderProxyFallbackToDefaultMaterial:1;	
#if WITH_EDITOR
	uint32 bIsOwnedByFoliage:1;
#endif



	TEnumAsByte<EComponentMobility::Type> Mobility = EComponentMobility::Movable;
	int32 TranslucencySortPriority = 0;
	float TranslucencySortDistanceOffset = 0.0f;
	ELightmapType LightmapType = ELightmapType::Default;
	TEnumAsByte<enum ESceneDepthPriorityGroup> ViewOwnerDepthPriorityGroup = ESceneDepthPriorityGroup::SDPG_World;
	int32 CustomDepthStencilValue = 0;
	ERendererStencilMask CustomDepthStencilWriteMask = ERendererStencilMask::ERSM_Default;
	FLightingChannels LightingChannels;
	ERayTracingGroupCullingPriority RayTracingGroupCullingPriority = ERayTracingGroupCullingPriority::CP_4_DEFAULT;
	TEnumAsByte<EIndirectLightingCacheQuality> IndirectLightingCacheQuality = EIndirectLightingCacheQuality::ILCQ_Point;	
	EShadowCacheInvalidationBehavior ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Auto;

	TEnumAsByte<enum ESceneDepthPriorityGroup> DepthPriorityGroup = ESceneDepthPriorityGroup::SDPG_World;
	
	int8 VirtualTextureLodBias = 0;
	int32 VirtualTextureCullMips = 0;
	int8 VirtualTextureMinCoverage = 0;
	FPrimitiveComponentId ComponentId;
	int32 VisibilityId = 0;
	float CachedMaxDrawDistance = 0.0f;
	float MinDrawDistance = 0.0f;
	float BoundsScale = 1.0f;
	int32 RayTracingGroupId = FPrimitiveSceneProxy::InvalidRayTracingGroupId;

	ERHIFeatureLevel::Type FeatureLevel;
	
	UObject* Component = nullptr; 
	UObject* Owner = nullptr;
	UWorld* World = nullptr;	

	// Only used by actors for now, explicitly intended to be moved to the FPrimitiveSceneProxy
	mutable TArray<const AActor*> ActorOwners;	
	const FCustomPrimitiveData* CustomPrimitiveData = nullptr;
	FSceneInterface* Scene = nullptr;
	IPrimitiveComponent* PrimitiveComponentInterface = nullptr;

	uint64 HiddenEditorViews = 0;	

#if MESH_DRAW_COMMAND_STATS
	FName MeshDrawCommandStatsCategory;
	FName GetMeshDrawCommandStatsCategory() const { return MeshDrawCommandStatsCategory; }
#endif //!MESH_DRAW_COMMAND_STATS


	const FCustomPrimitiveData& GetCustomPrimitiveData() const { check(CustomPrimitiveData); return *CustomPrimitiveData; }
	bool IsVisible()  const { return bIsVisible; }
	bool IsVisibleEditor() const { return bIsVisibleEditor; }
	bool ShouldRenderSelected() const { return bShouldRenderSelected; }
	bool IsComponentIndividuallySelected() const { return bIndividuallySelected; }
	ESceneDepthPriorityGroup GetStaticDepthPriorityGroup() const { return DepthPriorityGroup; }
	bool HasStaticLighting() const { return bHasStaticLighting; } 
	bool IsCollisionEnabled() const { return bCollisionEnabled; }
	bool IsPrecomputedLightingValid() const { return false; }
	bool HasValidSettingsForStaticLighting() const { return bHasValidSettingsForStaticLighting; } 
	bool GetShadowIndirectOnly() const { return bShadowIndirectOnly; }
	int32 GetRayTracingGroupId() const { return RayTracingGroupId; }
	FSceneInterface* GetScene()  const { check(Scene); return Scene; }
	bool GetLevelInstanceEditingState() const { return bLevelInstanceEditingState; }

	UObject* GetOwner() const { return Owner; }	
	template<class T>
	T* GetOwner() const { return Cast<T>(Owner); }	

	ULevel* GetLevel() const { return  Owner ? Owner->GetTypedOuter<ULevel>() : nullptr; }
	ULevel* GetComponentLevel() const { return GetLevel(); }

	FString GetPathName() const { return Component->GetPathName(); }
	
	bool IsHidden() const { return bIsHidden; }
	bool IsOwnerEditorOnly() const { return bIsOwnerEditorOnly; }

#if WITH_EDITOR
	bool IsHiddenEd() const { return bIsHiddenEd; }
	uint64 GetHiddenEditorViews() const { return HiddenEditorViews; }
	bool IsOwnedByFoliage() const { return bIsOwnedByFoliage; }	
#endif
	
	const UObject* AdditionalStatObjectPtr = nullptr;
	const UObject* AdditionalStatObject() const
	{
		return AdditionalStatObjectPtr;
	}

	TStatId StatId;
	TStatId GetStatID(bool bForDeferredUse = false) const
	{
		return StatId;
	}

	TArrayView<URuntimeVirtualTexture*>  RuntimeVirtualTextures;
	ERuntimeVirtualTextureMainPassType VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Exclusive;

	TArrayView<URuntimeVirtualTexture*> const& GetRuntimeVirtualTextures() const { return RuntimeVirtualTextures; }
	ERuntimeVirtualTextureMainPassType GetVirtualTextureRenderPassType() const { return VirtualTextureRenderPassType; }
	float VirtualTextureMainPassMaxDrawDistance = 0.0f;
	float GetVirtualTextureMainPassMaxDrawDistance() const { return VirtualTextureMainPassMaxDrawDistance; }
	
	bool ShouldRenderProxyFallbackToDefaultMaterial() const { return bShouldRenderProxyFallbackToDefaultMaterial; }
	ENGINE_API virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const;

	bool SupportsWorldPositionOffsetVelocity() const { return bSupportsWorldPositionOffsetVelocity; }

	IPrimitiveComponent* GetPrimitiveComponentInterface() const { return PrimitiveComponentInterface; }	

	UWorld* GetWorld() const 
	{
		check(World);
		return World;
	}
};
