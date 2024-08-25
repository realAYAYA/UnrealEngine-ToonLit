// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "Rendering/NaniteResources.h"
#include "RayTracingInstance.h"
#include "RayTracingGeometry.h"
#include "LocalVertexFactory.h"

struct FPerInstanceRenderData;
class UStaticMeshComponent;
class UWorld;
enum ECollisionTraceFlag : int;
enum EMaterialDomain : int;
struct FStaticMeshVertexFactories;
using FStaticMeshVertexFactoriesArray = TArray<FStaticMeshVertexFactories>;
struct FStaticMeshSceneProxyDesc;
struct FInstancedStaticMeshSceneProxyDesc;

namespace Nanite
{

struct FMaterialAuditEntry
{
	UMaterialInterface* Material = nullptr;

	FName MaterialSlotName;
	int32 MaterialIndex = INDEX_NONE;

	uint8 bHasAnyError					: 1;
	uint8 bHasNullMaterial				: 1;
	uint8 bHasWorldPositionOffset		: 1;
	uint8 bHasUnsupportedBlendMode		: 1;
	uint8 bHasUnsupportedShadingModel	: 1;
	uint8 bHasPixelDepthOffset			: 1;
	uint8 bHasTessellationEnabled		: 1;
	uint8 bHasVertexInterpolator		: 1;
	uint8 bHasPerInstanceRandomID		: 1;
	uint8 bHasPerInstanceCustomData		: 1;
	uint8 bHasInvalidUsage				: 1;

	FVector4f LocalUVDensities;
};

struct ENGINE_API FMaterialAudit
{
	FString AssetName;
	TArray<FMaterialAuditEntry, TInlineAllocator<4>> Entries;
	UMaterialInterface* FallbackMaterial;
	uint8 bHasAnyError : 1;
	uint8 bHasMasked : 1;
	uint8 bHasSky : 1;

	FMaterialAudit()
		: FallbackMaterial(nullptr)
		, bHasAnyError(false)
		, bHasMasked(false)
		, bHasSky(false)
	{}

	FORCEINLINE bool IsValid(bool bAllowMasked) const
	{
		return !bHasAnyError && !bHasSky && (bAllowMasked || !bHasMasked);
	}

	FORCEINLINE UMaterialInterface* GetMaterial(int32 MaterialIndex) const
	{
		if (Entries.IsValidIndex(MaterialIndex))
		{
			return Entries[MaterialIndex].Material;
		}

		return nullptr;
	}

	FORCEINLINE UMaterialInterface* GetSafeMaterial(int32 MaterialIndex) const
	{
		if (Entries.IsValidIndex(MaterialIndex))
		{
			const FMaterialAuditEntry& AuditEntry = Entries[MaterialIndex];
			return AuditEntry.bHasAnyError ? FallbackMaterial : AuditEntry.Material;
		}

		return nullptr;
	}

	FORCEINLINE bool HasPerInstanceRandomID(int32 MaterialIndex) const
	{
		if (Entries.IsValidIndex(MaterialIndex))
		{
			return Entries[MaterialIndex].bHasPerInstanceRandomID;
		}

		return false;
	}

	FORCEINLINE bool HasPerInstanceCustomData(int32 MaterialIndex) const
	{
		if (Entries.IsValidIndex(MaterialIndex))
		{
			return Entries[MaterialIndex].bHasPerInstanceCustomData;
		}

		return false;
	}

	FORCEINLINE FVector4f GetLocalUVDensities(int32 MaterialIndex) const
	{
		if (Entries.IsValidIndex(MaterialIndex))
		{
			return Entries[MaterialIndex].LocalUVDensities;
		}
		return FVector4f(1.0f);
	}
};

ENGINE_API void AuditMaterials(const UStaticMeshComponent* Component, FMaterialAudit& Audit, bool bSetMaterialUsage = true);
ENGINE_API void AuditMaterials(const FStaticMeshSceneProxyDesc* ProxyDesc, FMaterialAudit& Audit, bool bSetMaterialUsage = true);

ENGINE_API bool IsSupportedBlendMode(EBlendMode Mode);
ENGINE_API bool IsSupportedBlendMode(const FMaterial& In);
ENGINE_API bool IsSupportedBlendMode(const FMaterialShaderParameters& In);
ENGINE_API bool IsSupportedBlendMode(const UMaterialInterface& In);
ENGINE_API bool IsSupportedMaterialDomain(EMaterialDomain Domain);
ENGINE_API bool IsSupportedShadingModel(FMaterialShadingModelField ShadingModelField);

ENGINE_API bool IsMaskingAllowed(UWorld* World, bool bForceNaniteForMasked);

struct FResourceMeshInfo
{
	TArray<uint32> SegmentMapping;

	uint32 NumClusters = 0;
	uint32 NumNodes = 0;
	uint32 NumVertices = 0;
	uint32 NumTriangles = 0;
	uint32 NumMaterials = 0;
	uint32 NumSegments = 0;

	uint32 NumResidentClusters = 0;

	FDebugName DebugName;
};

// Note: Keep NANITE_FILTER_FLAGS_NUM_BITS in sync
enum class EFilterFlags : uint8
{
	None					= 0u,
	StaticMesh				= (1u << 0u),
	InstancedStaticMesh		= (1u << 1u),
	Foliage					= (1u << 2u),
	Grass					= (1u << 3u),
	Landscape				= (1u << 4u),
	StaticMobility			= (1u << 5u),
	NonStaticMobility		= (1u << 6u),
	All						= 0xFF
};

ENUM_CLASS_FLAGS(EFilterFlags)

class FSceneProxyBase : public FPrimitiveSceneProxy
{
public:
#if WITH_EDITOR
	enum class EHitProxyMode : uint8
	{
		MaterialSection,
		PerInstance,
	};
#endif

	struct FMaterialSection
	{
		FMaterialRenderProxy* RasterMaterialProxy = nullptr;
		FMaterialRenderProxy* ShadingMaterialProxy = nullptr;

	#if WITH_EDITOR
		HHitProxy* HitProxy = nullptr;
	#endif
		int32 MaterialIndex = INDEX_NONE;
		float MaxWPOExtent = 0.0f;

		FDisplacementScaling DisplacementScaling;

		FMaterialRelevance MaterialRelevance;
		FVector4f LocalUVDensities = FVector4f(1.0f);

		uint8 bHasPerInstanceRandomID : 1;
		uint8 bHasPerInstanceCustomData : 1;
		uint8 bHidden : 1;
		uint8 bAlwaysEvaluateWPO : 1;
	#if WITH_EDITORONLY_DATA
		uint8 bSelected : 1;
	#endif

		ENGINE_API void ResetToDefaultMaterial(bool bShading = true, bool bRaster = true);

		inline bool IsProgrammableRaster(bool bEvaluateWPO) const
		{
			// NOTE: MaterialRelevance.bTwoSided does not go into bHasProgrammableRaster
			// because we want only want this flag to control culling, not a full raster bin
			return (bEvaluateWPO && MaterialRelevance.bUsesWorldPositionOffset) ||
				MaterialRelevance.bUsesPixelDepthOffset ||
				MaterialRelevance.bMasked ||
				MaterialRelevance.bUsesDisplacement;
		}
	};

public:

	FSceneProxyBase(const FPrimitiveSceneProxyDesc& Desc)
	: FPrimitiveSceneProxy(Desc)
	{
		bIsNaniteMesh  = true;
		bIsAlwaysVisible = SupportsAlwaysVisible();
		bHasProgrammableRaster = false;
		bHasDynamicDisplacement = false;
		bReverseCulling = false;
	#if WITH_EDITOR
		bHasSelectedInstances = false;
	#endif
	}

	FSceneProxyBase(const UPrimitiveComponent* Component)
	: FPrimitiveSceneProxy(Component)
	{
		bIsNaniteMesh  = true;
		bIsAlwaysVisible = SupportsAlwaysVisible();
		bHasProgrammableRaster = false;
		bHasDynamicDisplacement = false;
		bReverseCulling = false;
	#if WITH_EDITOR
		bHasSelectedInstances = false;
	#endif
	}

	virtual ~FSceneProxyBase() = default;

#if WITH_EDITOR
	ENGINE_API virtual HHitProxy* CreateHitProxies(IPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies) override;
	ENGINE_API virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies) override;
#endif

	virtual bool IsUsingDistanceCullFade() const override
	{
		// Disable distance cull fading, as this is not supported anyways (and it has CPU overhead)
		return false;
	}

	virtual bool CanBeOccluded() const override
	{
		// Disable slow occlusion paths(Nanite does its own occlusion culling)
		return false;
	}

	inline bool HasProgrammableRaster() const
	{
		return bHasProgrammableRaster;
	}

	inline bool HasDynamicDisplacement() const
	{
		return bHasDynamicDisplacement;
	}

	inline const TArray<FMaterialSection>& GetMaterialSections() const
	{
		return MaterialSections;
	}

	inline TArray<FMaterialSection>& GetMaterialSections()
	{
		return MaterialSections;
	}

	inline int32 GetMaterialMaxIndex() const
	{
		return MaterialMaxIndex;
	}

	inline EFilterFlags GetFilterFlags() const
	{
		return FilterFlags;
	}

	inline bool IsCullingReversedByComponent() const
	{
		return bReverseCulling;
	}

	inline const FMaterialRelevance& GetCombinedMaterialRelevance() const
	{
		return CombinedMaterialRelevance;
	}

	virtual FResourceMeshInfo GetResourceMeshInfo() const = 0;

	inline void SetRayTracingId(uint32 InRayTracingId) { RayTracingId = InRayTracingId; }
	inline uint32 GetRayTracingId() const { return RayTracingId; }

	inline void SetRayTracingDataOffset(uint32 InRayTracingDataOffset) { RayTracingDataOffset = InRayTracingDataOffset; }
	inline uint32 GetRayTracingDataOffset() const { return RayTracingDataOffset; }

#if WITH_EDITOR
	inline const TConstArrayView<const FHitProxyId> GetHitProxyIds() const
	{
		return HitProxyIds;
	}

	inline EHitProxyMode GetHitProxyMode() const
	{
		return HitProxyMode;
	}

	inline bool HasSelectedInstances() const
	{
		return bHasSelectedInstances;
	}
#endif

	// Nanite always uses LOD 0, and performs custom LOD streaming.
	virtual uint8 GetCurrentFirstLODIdx_RenderThread() const override { return 0; }

protected:
	ENGINE_API void DrawStaticElementsInternal(FStaticPrimitiveDrawInterface* PDI, const FLightCacheInterface* LCI);
	ENGINE_API void OnMaterialsUpdated();
	ENGINE_API bool SupportsAlwaysVisible() const;

protected:
	TArray<FMaterialSection> MaterialSections;

#if WITH_EDITOR
	TArray<FHitProxyId> HitProxyIds;
	EHitProxyMode HitProxyMode = EHitProxyMode::MaterialSection;
#endif
	int32 MaterialMaxIndex = INDEX_NONE;
	uint32 InstanceWPODisableDistance = 0;
	EFilterFlags FilterFlags = EFilterFlags::None;
	uint8 bHasProgrammableRaster : 1;
	uint8 bHasDynamicDisplacement : 1;
	uint8 bReverseCulling : 1;
#if WITH_EDITOR
	uint8 bHasSelectedInstances : 1;
#endif

private:

	uint32 RayTracingId = INDEX_NONE;
	uint32 RayTracingDataOffset = INDEX_NONE;
};

class FSceneProxy : public FSceneProxyBase
{
public:
	using Super = FSceneProxyBase;

	ENGINE_API FSceneProxy(const FMaterialAudit& MaterialAudit, const FStaticMeshSceneProxyDesc& ProxyDesc, bool bIsInstanced = false);
	ENGINE_API FSceneProxy(const FMaterialAudit& MaterialAudit, const FInstancedStaticMeshSceneProxyDesc& ProxyDesc);

	ENGINE_API FSceneProxy(const FMaterialAudit& MaterialAudit, UStaticMeshComponent* Component);
	ENGINE_API FSceneProxy(const FMaterialAudit& MaterialAudit, UInstancedStaticMeshComponent* Component);
	ENGINE_API FSceneProxy(const FMaterialAudit& MaterialAudit, UHierarchicalInstancedStaticMeshComponent* Component);

	ENGINE_API virtual ~FSceneProxy();

public:
	// FPrimitiveSceneProxy interface.
	ENGINE_API virtual SIZE_T GetTypeHash() const override;
	ENGINE_API virtual FPrimitiveViewRelevance	GetViewRelevance(const FSceneView* View) const override;
	ENGINE_API virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;

#if WITH_EDITOR
	ENGINE_API virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
	ENGINE_API virtual HHitProxy* CreateHitProxies(IPrimitiveComponent* ComponentInterface,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	ENGINE_API virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	ENGINE_API virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

#if NANITE_ENABLE_DEBUG_RENDERING
	/** Sets up a collision FMeshBatch for a specific LOD and element. */
	ENGINE_API virtual bool GetCollisionMeshElement(
		int32 LODIndex,
		int32 BatchIndex,
		int32 ElementIndex,
		uint8 InDepthPriorityGroup,
		const FMaterialRenderProxy* RenderProxy,
		FMeshBatch& OutMeshBatch) const;
#endif

#if RHI_RAYTRACING
	ENGINE_API virtual bool HasRayTracingRepresentation() const override;
	virtual bool IsRayTracingRelevant() const { return true; }
	virtual bool IsRayTracingStaticRelevant() const { return true; }
	ENGINE_API virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances) override;
	ENGINE_API virtual ERayTracingPrimitiveFlags GetCachedRayTracingInstance(FRayTracingInstance& RayTracingInstance) override;
	virtual Nanite::CoarseMeshStreamingHandle GetCoarseMeshStreamingHandle() const override { return CoarseMeshStreamingHandle; }
	ENGINE_API virtual RayTracing::GeometryGroupHandle GetRayTracingGeometryGroupHandle() const override;
#endif

	ENGINE_API virtual uint32 GetMemoryFootprint() const override;

	virtual void GetLCIs(FLCIArray& LCIs) override
	{
		FLightCacheInterface* LCI = &MeshInfo;
		LCIs.Add(LCI);
	}

	ENGINE_API virtual void GetDistanceFieldAtlasData(const FDistanceFieldVolumeData*& OutDistanceFieldData, float& SelfShadowBias) const override;
	ENGINE_API virtual bool HasDistanceFieldRepresentation() const override;

	ENGINE_API virtual const FCardRepresentationData* GetMeshCardRepresentation() const override;

	ENGINE_API virtual int32 GetLightMapCoordinateIndex() const override;

	virtual void GetNaniteResourceInfo(uint32& OutResourceID, uint32& OutHierarchyOffset, uint32& OutImposterIndex) const override
	{
		OutResourceID = Resources->RuntimeResourceID;
		OutHierarchyOffset = Resources->HierarchyOffset;
		OutImposterIndex = Resources->ImposterIndex;
	}

	virtual void GetNaniteMaterialMask(FUint32Vector2& OutMaterialMask) const override
	{
		OutMaterialMask = NaniteMaterialMask;
	}

	ENGINE_API virtual FResourceMeshInfo GetResourceMeshInfo() const override;

	ENGINE_API virtual bool GetInstanceDrawDistanceMinMax(FVector2f& OutCullRange) const override;
	ENGINE_API virtual bool GetInstanceWorldPositionOffsetDisableDistance(float& OutWPODisableDistance) const override;

	ENGINE_API virtual void SetWorldPositionOffsetDisableDistance_GameThread(int32 NewValue) override;

	ENGINE_API virtual void SetInstanceCullDistance_RenderThread(float StartCullDistance, float EndCullDistance) override;

	ENGINE_API virtual FInstanceDataUpdateTaskInfo *GetInstanceDataUpdateTaskInfo() const override;

	const UStaticMesh* GetStaticMesh() const
	{
		return StaticMesh;
	}

protected:
	ENGINE_API virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	
	ENGINE_API virtual void OnEvaluateWorldPositionOffsetChanged_RenderThread() override;

	class FMeshInfo : public FLightCacheInterface
	{
	public:
		FMeshInfo(const FStaticMeshSceneProxyDesc& InProxyDesc);

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneProxy* LightSceneProxy) const override;

	private:
		TArray<FGuid> IrrelevantLights;
	};

	ENGINE_API bool IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const;

#if RHI_RAYTRACING
	ENGINE_API int32 GetFirstValidRaytracingGeometryLODIndex() const;
	ENGINE_API virtual void SetupRayTracingMaterials(int32 LODIndex, TArray<FMeshBatch>& OutMaterials) const;
	ENGINE_API virtual void SetupFallbackRayTracingMaterials(int32 LODIndex, TArray<FMeshBatch>& OutMaterials) const;
#endif // RHI_RAYTRACING

#if RHI_RAYTRACING || NANITE_ENABLE_DEBUG_RENDERING
	/** Configures mesh batch vertex / index state. Returns the number of primitives used in the element. */
	ENGINE_API uint32 SetMeshElementGeometrySource(
		int32 LODIndex,
		int32 ElementIndex,
		bool bWireframe,
		bool bUseInversedIndices,
		const ::FVertexFactory* VertexFactory,
		FMeshBatch& OutMeshElement) const;

	ENGINE_API bool IsReversedCullingNeeded(bool bUseReversedIndices) const;
#endif

protected:
	FMeshInfo MeshInfo;

	const FResources* Resources = nullptr;

	const FStaticMeshRenderData* RenderData;
	const FDistanceFieldVolumeData* DistanceFieldData;
	const FCardRepresentationData* CardRepresentationData;

	FUint32Vector2 NaniteMaterialMask = FUint32Vector2(~uint32(0), ~uint32(0));

	uint32 bHasMaterialErrors : 1;

	const UStaticMesh* StaticMesh = nullptr;

	uint32 EndCullDistance = 0;

	/** Minimum LOD index to use.  Clamped to valid range [0, NumLODs - 1]. */
	int32 ClampedMinLOD;

#if RHI_RAYTRACING
	ENGINE_API void CreateDynamicRayTracingGeometries(FRHICommandListBase& RHICmdList);
	ENGINE_API void ReleaseDynamicRayTracingGeometries();

	TArray<FRayTracingGeometry, TInlineAllocator<MAX_MESH_LOD_COUNT>> DynamicRayTracingGeometries;
	Nanite::CoarseMeshStreamingHandle CoarseMeshStreamingHandle = INDEX_NONE;
	TArray<FMeshBatch> CachedRayTracingMaterials;
	int16 CachedRayTracingMaterialsLODIndex = INDEX_NONE;

	bool bHasRayTracingInstances : 1 = false;
	bool bNeedsDynamicRayTracingGeometries : 1 = false;

	RayTracing::GeometryGroupHandle RayTracingGeometryGroupHandle = INDEX_NONE;
#endif

	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> InstanceDataSceneProxy; 

#if NANITE_ENABLE_DEBUG_RENDERING
	UObject* Owner;

	/** LightMap resolution used for VMI_LightmapDensity */
	int32 LightMapResolution;

	/** Body setup for collision debug rendering */
	UBodySetup* BodySetup;

	/** Collision trace flags */
	ECollisionTraceFlag CollisionTraceFlag;

	/** Collision Response of this component */
	FCollisionResponseContainer CollisionResponse;

	/**
	 * The ForcedLOD set in the static mesh editor, copied from the mesh component
	 */
	int32 ForcedLodModel;

	/** LOD used for collision */
	int32 LODForCollision;

	/** Draw mesh collision if used for complex collision */
	uint32 bDrawMeshCollisionIfComplex : 1;

	/** Draw mesh collision if used for simple collision */
	uint32 bDrawMeshCollisionIfSimple : 1;
#endif

#if RHI_RAYTRACING || NANITE_ENABLE_DEBUG_RENDERING
	class FFallbackLODInfo
	{
	public:
		/** Information about an element of a LOD. */
		struct FSectionInfo
		{
			/** Default constructor. */
			FSectionInfo()
				: MaterialProxy(nullptr)
			#if WITH_EDITOR
				, bSelected(false)
				, HitProxy(nullptr)
			#endif
			{}

			/** The material with which to render this section. */
			FMaterialRenderProxy* MaterialProxy;

		#if WITH_EDITOR
			/** True if this section should be rendered as selected (editor only). */
			bool bSelected;

			/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
			HHitProxy* HitProxy;
		#endif

		#if WITH_EDITORONLY_DATA
			// The material index from the component. Used by the texture streaming accuracy viewmodes.
			int32 MaterialIndex;
		#endif
		};

		/** Per-section information. */
		TArray<FSectionInfo, TInlineAllocator<1>> Sections;

		/** Vertex color data for this LOD (or NULL when not overridden), FStaticMeshComponentLODInfo handles the release of the memory */
		FColorVertexBuffer* OverrideColorVertexBuffer;

		TUniformBufferRef<FLocalVertexFactoryUniformShaderParameters> OverrideColorVFUniformBuffer;

		FFallbackLODInfo(
			const FStaticMeshSceneProxyDesc* InProxyDEsc,
			const FStaticMeshVertexFactoriesArray& InLODVertexFactories,
			int32 InLODIndex,
			int32 InClampedMinLOD
		);
	};

	TArray<FFallbackLODInfo> FallbackLODs;
#endif
};

} // namespace Nanite

