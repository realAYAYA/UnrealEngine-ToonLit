// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialInterface.h"
#include "ComponentReregisterContext.h"
#include "SkeletalMeshLegacyCustomVersions.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMaterialRenderProxy;
class FMeshElementCollector;
class FPrimitiveDrawInterface;
class FRawStaticIndexBuffer16or32Interface;
class UMorphTarget;
class UPrimitiveComponent;
class USkeletalMesh;
class USkinnedMeshComponent;
class USkeletalMesh;
class FSkeletalMeshRenderData;
class FSkeletalMeshLODRenderData;

/** Flags used when building vertex buffers. */
struct ESkeletalMeshVertexFlags
{
	enum
	{
		None = 0x0,
		UseFullPrecisionUVs = 0x1,
		HasVertexColors = 0x2,
		UseHighPrecisionTangentBasis = 0x4,
		UseBackwardsCompatibleF16TruncUVs = 0x8
	};
};

/** Name of vertex color channels, used by recompute tangents */
enum class ESkinVertexColorChannel : uint8
{
	// Use red channel as recompute tangents blending mask
	Red = 0,
	// Use green channel as recompute tangents blending mask
	Green = 1,
	// Use blue channel as recompute tangents blending mask
	Blue = 2,
	// Alpha channel not used by recompute tangents
	Alpha = 3,
	None = Alpha
};

enum class ESkinVertexFactoryMode
{
	Default,
	RayTracing
};

/**
 * A structure for holding mesh-to-mesh triangle influences to skin one mesh to another (similar to a wrap deformer)
 */
struct FMeshToMeshVertData
{
	// Barycentric coords and distance along normal for the position of the final vert
	FVector4f PositionBaryCoordsAndDist; 

	// Barycentric coords and distance along normal for the location of the unit normal endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	FVector4f NormalBaryCoordsAndDist;

	// Barycentric coords and distance along normal for the location of the unit Tangent endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	FVector4f TangentBaryCoordsAndDist;

	// Contains the 3 indices for verts in the source mesh forming a triangle, the last element
	// is a flag to decide how the skinning works, 0xffff uses no simulation, and just normal
	// skinning, anything else uses the source mesh and the above skin data to get the final position
	uint16	 SourceMeshVertIndices[4];

	// For weighted averaging of multiple triangle influences
	float	 Weight = 0.0f;

	// Dummy for alignment
	uint32	 Padding;

	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FMeshToMeshVertData& V);
};

/**
* Structure to store the buffer offsets to the section's cloth deformer mapping data. 
* Also contains the stride to further cloth LODs data for cases where one section has
* multiple mappings so that it can be wrap deformed with cloth data from a different LOD.
* When using LOD bias in Raytracing for example.
*/
struct FClothBufferIndexMapping
{
	/** Section first index. */
	uint32 BaseVertexIndex;

	/** Offset in the buffer to the corresponding cloth mapping. */
	uint32 MappingOffset;

	/** Stride to the next LOD mapping if any. */
	uint32 LODBiasStride;

	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FClothBufferIndexMapping& V);
};

struct FClothingSectionData
{
	FClothingSectionData()
		: AssetGuid()
		, AssetLodIndex(INDEX_NONE)
	{}

	bool IsValid() const
	{
		return AssetGuid.IsValid() && AssetLodIndex != INDEX_NONE;
	}

	/** Guid of the clothing asset applied to this section */
	FGuid AssetGuid;

	/** LOD inside the applied asset that is used */
	int32 AssetLodIndex;

	friend FArchive& operator<<(FArchive& Ar, FClothingSectionData& Data)
	{
		Ar << Data.AssetGuid;
		Ar << Data.AssetLodIndex;

		return Ar;
	}
};


/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/

/**
 * A skeletal mesh component scene proxy.
 */
class ENGINE_API FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override;

	/** 
	 * Constructor. 
	 * @param	Component - skeletal mesh primitive being added
	 */
	FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshRenderData* InSkelMeshRenderData);

#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

#if RHI_RAYTRACING
	virtual bool HasRayTracingRepresentation() const override;

	virtual bool IsRayTracingRelevant() const override { return true; }

	virtual bool IsRayTracingStaticRelevant() const override
	{
		return bRenderStatic;
	}

	virtual void GetDynamicRayTracingInstances(struct FRayTracingMaterialGatheringContext& Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances) override;
#endif // RHI_RAYTRACING

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	virtual bool IsUsingDistanceCullFade() const override;
	
	virtual bool HasDynamicIndirectShadowCasterRepresentation() const override;
	virtual void GetShadowShapes(FVector PreViewTranslation, TArray<FCapsuleShape3f>& OutCapsuleShapes) const override;

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
	bool GetWorldMatrices( FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal ) const;

	/** Util for getting LOD index currently used by this SceneProxy. */
	int32 GetCurrentLODIndex();

	/** 
	 * Render physics asset for debug display
	 */
	virtual void DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;

	/** Render the bones of the skeleton for debug display */ 
	void DebugDrawSkeleton(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;

#if WITH_EDITOR
	void DebugDrawPoseWatchSkeletons(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;
#endif

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	SIZE_T GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODSections.GetAllocatedSize() ); }

	/**
	* Updates morph material usage for materials referenced by each LOD entry
	*
	* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
	*/
	void UpdateMorphMaterialUsage_GameThread(TArray<UMaterialInterface*>& MaterialUsingMorphTarget);


#if WITH_EDITORONLY_DATA
	virtual bool GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const override;
	virtual bool GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const override;
	virtual bool GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4f* OneOverScales, FIntVector4* UVChannelIndices) const override;
#endif

	friend class FSkeletalMeshSectionIter;

	virtual void OnTransformChanged() override;

	virtual uint8 GetCurrentFirstLODIdx_RenderThread() const final override
	{
		return GetCurrentFirstLODIdx_Internal();
	}

	bool GetCachedGeometry(struct FCachedGeometry& OutCachedGeometry) const;

protected:
	AActor* Owner;
	class FSkeletalMeshObject* MeshObject;
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

	void GetDynamicElementsSection(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
									const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, bool bSectionSelected,
								   const FSectionElementInfo& SectionElementInfo, bool bInSelectable, FMeshElementCollector& Collector) const;

	void GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const;

	/** Only call on render thread timeline */
	uint8 GetCurrentFirstLODIdx_Internal() const;
private:
	void CreateBaseMeshBatch(const FSceneView* View, const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, const FSectionElementInfo& SectionElementInfo, FMeshBatch& Mesh, ESkinVertexFactoryMode VFMode = ESkinVertexFactoryMode::Default) const;
	void UpdateLooseParametersUniformBuffer(const FSceneView* View, const int32 SectionIndex, const FMeshBatch& Mesh, const struct FGPUSkinBatchElementUserData* BatchUserData) const;

public:
#if WITH_EDITORONLY_DATA
	struct FPoseWatchDynamicData* PoseWatchDynamicData;
#endif
};

/** Used to recreate all skinned mesh components for a given skeletal mesh */
class ENGINE_API FSkinnedMeshComponentRecreateRenderStateContext
{
public:

	/** Initialization constructor. */
	FSkinnedMeshComponentRecreateRenderStateContext(USkeletalMesh* InSkeletalMesh, bool InRefreshBounds = false);

	/** Destructor: recreates render state for all components that had their render states destroyed in the constructor. */
	~FSkinnedMeshComponentRecreateRenderStateContext();
	
private:

	/** List of components to reset */
	TArray< TWeakObjectPtr<USkinnedMeshComponent>> MeshComponents;

	/** Whether we'll refresh the component bounds as we reset */
	bool bRefreshBounds;
};

#if WITH_EDITOR

//Helper to scope skeletal mesh post edit change.
class ENGINE_API FScopedSkeletalMeshPostEditChange
{
public:
	/*
	 * This constructor increment the skeletal mesh PostEditChangeStackCounter. If the stack counter is zero before the increment
	 * the skeletal mesh component will be unregister from the world. The component will also release there rendering resources.
	 * Parameters:
	 * @param InbCallPostEditChange - if we are the first scope PostEditChange will be called.
	 * @param InbReregisterComponents - if we are the first scope we will re register component from world and also component render data.
	 */
	FScopedSkeletalMeshPostEditChange(USkeletalMesh* InSkeletalMesh, bool InbCallPostEditChange = true, bool InbReregisterComponents = true);

	/*
	 * This destructor decrement the skeletal mesh PostEditChangeStackCounter. If the stack counter is zero after the decrement,
	 * the skeletal mesh PostEditChange will be call. The component will also be register to the world and there render data resources will be rebuild.
	 */
	~FScopedSkeletalMeshPostEditChange();

	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);

private:
	USkeletalMesh* SkeletalMesh;
	bool bReregisterComponents;
	bool bCallPostEditChange;
	FSkinnedMeshComponentRecreateRenderStateContext* RecreateExistingRenderStateContext;
	TIndirectArray<FComponentReregisterContext> ComponentReregisterContexts;
};

#endif
