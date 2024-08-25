// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PrimitiveSceneProxy.h"
#include "SkeletalMeshTypes.h"

/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/

class FSkeletalMeshObject;

/**
 * A skeletal mesh component scene proxy.
 */
class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	ENGINE_API SIZE_T GetTypeHash() const override;

	/** 
	 * Constructor. 
	 * @param	Component - skeletal mesh primitive being added
	 */
	ENGINE_API FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshRenderData* InSkelMeshRenderData);

#if WITH_EDITOR
	ENGINE_API virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	ENGINE_API virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	ENGINE_API virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

#if RHI_RAYTRACING
	ENGINE_API virtual bool HasRayTracingRepresentation() const override;

	virtual bool IsRayTracingRelevant() const override { return true; }

	virtual bool IsRayTracingStaticRelevant() const override
	{
		return bRenderStatic;
	}

	ENGINE_API virtual TArray<FRayTracingGeometry*> GetStaticRayTracingGeometries() const override;

	ENGINE_API virtual void GetDynamicRayTracingInstances(struct FRayTracingMaterialGatheringContext& Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances) override;
#endif // RHI_RAYTRACING

	ENGINE_API virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	ENGINE_API virtual bool CanBeOccluded() const override;
	ENGINE_API virtual bool IsUsingDistanceCullFade() const override;
	
	ENGINE_API virtual bool HasDynamicIndirectShadowCasterRepresentation() const override;
	ENGINE_API virtual void GetShadowShapes(FVector PreViewTranslation, TArray<FCapsuleShape3f>& OutCapsuleShapes) const override;

	/** Return the bounds for the pre-skinned primitive in local space */
	virtual void GetPreSkinnedLocalBounds(FBoxSphereBounds& OutBounds) const override { OutBounds = PreSkinnedLocalBounds; }

	/** Returns a pre-sorted list of shadow capsules's bone indicies */
	const TArray<uint16>& GetSortedShadowBoneIndices() const
	{
		return ShadowCapsuleBoneIndices;
	}

	/**
	 * Returns the world transform to use for drawing.
	 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
	 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
	 * 
	 * @return true if out matrices are valid 
	 */
	ENGINE_API bool GetWorldMatrices( FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal ) const;

	/** Util for getting LOD index currently used by this SceneProxy. */
	ENGINE_API int32 GetCurrentLODIndex();

	/** 
	 * Render physics asset for debug display
	 */
	ENGINE_API virtual void DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;

	/** Render the bones of the skeleton for debug display */ 
	ENGINE_API void DebugDrawSkeleton(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;

#if WITH_EDITOR
	ENGINE_API void DebugDrawPoseWatchSkeletons(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;
#endif

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	SIZE_T GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODSections.GetAllocatedSize() ); }

	/**
	* Updates morph material usage for materials referenced by each LOD entry
	*
	* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
	*/
	ENGINE_API void UpdateMorphMaterialUsage_GameThread(TArray<UMaterialInterface*>& MaterialUsingMorphTarget);


#if WITH_EDITORONLY_DATA
	ENGINE_API virtual bool GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const override;
	ENGINE_API virtual bool GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const override;
	ENGINE_API virtual bool GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4f* OneOverScales, FIntVector4* UVChannelIndices) const override;
#endif

	friend class FSkeletalMeshSectionIter;

	ENGINE_API virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override;

	virtual uint8 GetCurrentFirstLODIdx_RenderThread() const final override
	{
		return GetCurrentFirstLODIdx_Internal();
	}

	ENGINE_API bool GetCachedGeometry(struct FCachedGeometry& OutCachedGeometry) const;

	const FSkeletalMeshObject* GetMeshObject() const { return MeshObject; }

protected:
	AActor* Owner;
	FSkeletalMeshObject* MeshObject;
	FSkeletalMeshRenderData* SkeletalMeshRenderData;

	/** The points to the skeletal mesh and physics assets are purely for debug purposes. Access is NOT thread safe! */
	const class USkinnedAsset* SkeletalMeshForDebug;
	class UPhysicsAsset* PhysicsAssetForDebug;
	
	UMaterialInterface* OverlayMaterial;
	float OverlayMaterialMaxDrawDistance;
public:
#if RHI_RAYTRACING
	bool bAnySegmentUsesWorldPositionOffset : 1;
#endif

protected:
	/** data copied for rendering */
	uint8 bForceWireframe : 1;
	uint8 bIsCPUSkinned : 1;
	uint8 bCanHighlightSelectedSections : 1;
	uint8 bRenderStatic:1;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	uint8 bDrawDebugSkeleton:1;
#endif

	TEnumAsByte<ERHIFeatureLevel::Type> FeatureLevel;

	bool bMaterialsNeedMorphUsage_GameThread;

	FMaterialRelevance MaterialRelevance;


	/** info for section element in an LOD */
	struct FSectionElementInfo
	{
		FSectionElementInfo(UMaterialInterface* InMaterial, bool bInEnableShadowCasting, int32 InUseMaterialIndex)
		: Material( InMaterial )
		, bEnableShadowCasting( bInEnableShadowCasting )
		, UseMaterialIndex( InUseMaterialIndex )
#if WITH_EDITOR
		, HitProxy(NULL)
#endif
		{}
		
		UMaterialInterface* Material;
		
		/** Whether shadow casting is enabled for this section. */
		bool bEnableShadowCasting;
		
		/** Index into the materials array of the skel mesh or the component after LOD mapping */
		int32 UseMaterialIndex;

#if WITH_EDITOR
		/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
		HHitProxy* HitProxy;
#endif
	};

	/** Section elements for a particular LOD */
	struct FLODSectionElements
	{
		TArray<FSectionElementInfo> SectionElements;
	};
	
	/** Array of section elements for each LOD */
	TArray<FLODSectionElements> LODSections;

	/** 
	 * BoneIndex->capsule pairs used for rendering sphere/capsule shadows 
	 * Note that these are in refpose component space, NOT bone space.
	 */
	TArray<TPair<int32, FCapsuleShape>> ShadowCapsuleData;
	TArray<uint16> ShadowCapsuleBoneIndices;

	/** Set of materials used by this scene proxy, safe to access from the game thread. */
	TSet<UMaterialInterface*> MaterialsInUse_GameThread;
	
	/** The primitive's pre-skinned local space bounds. */
	FBoxSphereBounds PreSkinnedLocalBounds;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** The color we draw this component in if drawing debug bones */
	TOptional<FLinearColor> DebugDrawColor;
#endif

#if WITH_EDITORONLY_DATA
	/** The component streaming distance multiplier */
	float StreamingDistanceMultiplier;
#endif

	ENGINE_API void GetDynamicElementsSection(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
									const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, bool bSectionSelected,
								   const FSectionElementInfo& SectionElementInfo, bool bInSelectable, FMeshElementCollector& Collector) const;

	ENGINE_API void GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const;

	/** Only call on render thread timeline */
	ENGINE_API uint8 GetCurrentFirstLODIdx_Internal() const;
private:
	ENGINE_API void CreateBaseMeshBatch(const FSceneView* View, const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, const FSectionElementInfo& SectionElementInfo, FMeshBatch& Mesh, ESkinVertexFactoryMode VFMode = ESkinVertexFactoryMode::Default) const;

public:
#if WITH_EDITORONLY_DATA
	struct FPoseWatchDynamicData* PoseWatchDynamicData;
#endif
};
