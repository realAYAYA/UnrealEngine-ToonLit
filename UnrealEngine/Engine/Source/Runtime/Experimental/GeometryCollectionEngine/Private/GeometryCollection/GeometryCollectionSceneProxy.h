// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "HAL/CriticalSection.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "GeometryCollectionRendering.h"
#include "GeometryCollection/GeometryCollectionEditorSelection.h"
#include "HitProxies.h"
#include "EngineUtils.h"
#include "NaniteSceneProxy.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionRenderData.h"
#include "GeometryCollection/GeometryCollectionHitProxy.h"
#include "InstanceDataSceneProxy.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
struct FGeometryCollectionSection;

namespace Nanite
{
	struct FResources;
}


/** Vertex Buffer for transform data */
class FGeometryCollectionTransformBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FGeometryCollectionTransformBuffer"));

		// #note: This differs from instanced static mesh in that we are storing the entire transform in the buffer rather than
		// splitting out the translation.  This is to simplify transferring data at runtime as a memcopy
		VertexBufferRHI = RHICmdList.CreateVertexBuffer(NumTransforms * sizeof(FVector4f) * 4, BUF_Dynamic | BUF_ShaderResource, CreateInfo);		
		VertexBufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, 16, PF_A32B32G32R32F);
	}

	void UpdateDynamicData(FRHICommandListBase& RHICmdList, const TArray<FMatrix44f>& Transforms, EResourceLockMode LockMode);

	int32 NumTransforms;

	FShaderResourceViewRHIRef VertexBufferSRV;
};

inline void CopyTransformsWithConversionWhenNeeded(TArray<FMatrix44f>& DstTransforms, const TArray<FMatrix>& SrcTransforms)
{
	// LWC_TODO : we have no choice but to convert each element at this point to avoid changing GeometryCollectionAlgo::GlobalMatrices that is used all over the place
	DstTransforms.SetNumUninitialized(SrcTransforms.Num());
	for (int TransformIndex = 0; TransformIndex < SrcTransforms.Num(); ++TransformIndex)
	{
		DstTransforms[TransformIndex] = FMatrix44f(SrcTransforms[TransformIndex]); // LWC_TODO: Perf pessimization
	}
}

inline void CopyTransformsWithConversionWhenNeeded(TArray<FMatrix44f>& DstTransforms, const TArray<FTransform>& SrcTransforms)
{
	// LWC_TODO : we have no choice but to convert each element at this point to avoid changing GeometryCollectionAlgo::GlobalMatrices that is used all over the place
	DstTransforms.SetNumUninitialized(SrcTransforms.Num());
	for (int TransformIndex = 0; TransformIndex < SrcTransforms.Num(); ++TransformIndex)
	{
		DstTransforms[TransformIndex] = FTransform3f(SrcTransforms[TransformIndex]).ToMatrixWithScale(); // LWC_TODO: Perf pessimization
	}
}

inline void CopyTransformsWithConversionWhenNeeded(TArray<FMatrix44f>& DstTransforms, const TArray<FTransform3f>& SrcTransforms)
{
	DstTransforms.SetNumUninitialized(SrcTransforms.Num());
	for (int TransformIndex = 0; TransformIndex < SrcTransforms.Num(); ++TransformIndex)
	{
		DstTransforms[TransformIndex] = SrcTransforms[TransformIndex].ToMatrixWithScale();
	}
}

/** Mutable rendering data */
struct FGeometryCollectionDynamicData
{
	TArray<FMatrix44f> Transforms;
	TArray<FMatrix44f> PrevTransforms;
	uint32 ChangedCount;
	uint8 IsDynamic : 1;
	uint8 IsLoading : 1;

	FGeometryCollectionDynamicData()
	{
		Reset();
	}

	void Reset()
	{
		Transforms.Reset();
		PrevTransforms.Reset();
		IsDynamic = false;
		IsLoading = false;
	}

	UE_DEPRECATED(5.3, "Use FTransform version of SetTransforms instead")
	void SetTransforms(const TArray<FMatrix>& InTransforms)
	{
		// use for LWC as FMatrix and FMatrix44f are different when LWC is on 
		CopyTransformsWithConversionWhenNeeded(Transforms, InTransforms);
	}

	void SetTransforms(const TArray<FTransform>& InTransforms)
	{
		// use for LWC as FMatrix and FMatrix44f are different when LWC is on 
		CopyTransformsWithConversionWhenNeeded(Transforms, InTransforms);
	}

	void SetTransforms(const TArray<FTransform3f>& InTransforms)
	{
		CopyTransformsWithConversionWhenNeeded(Transforms, InTransforms);
	}

	UE_DEPRECATED(5.3, "Use FTransform version of SetPrevTransforms instead")
	void SetPrevTransforms(const TArray<FMatrix>& InTransforms)
	{
		// use for LWC as FMatrix and FMatrix44f are different when LWC is on 
		CopyTransformsWithConversionWhenNeeded(PrevTransforms, InTransforms);
	}

	void SetPrevTransforms(const TArray<FTransform>& InTransforms)
	{
		// use for LWC as FMatrix and FMatrix44f are different when LWC is on 
		CopyTransformsWithConversionWhenNeeded(PrevTransforms, InTransforms);
	}

	void SetPrevTransforms(const TArray<FTransform3f>& InTransforms)
	{
		CopyTransformsWithConversionWhenNeeded(PrevTransforms, InTransforms);
	}

	UE_DEPRECATED(5.3, "Use FTransform version of SetAllTransforms instead")
	void SetAllTransforms(const TArray<FMatrix>& InTransforms)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		SetTransforms(InTransforms);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		PrevTransforms = Transforms;
		ChangedCount = Transforms.Num();
	}

	void SetAllTransforms(const TArray<FTransform>& InTransforms)
	{
		SetTransforms(InTransforms);
		PrevTransforms = Transforms;
		ChangedCount = Transforms.Num();
	}

	void DetermineChanges()
	{
		// Check if previous transforms are the same as current
		const float EqualTolerance = 1e-6;

		check(Transforms.Num() == PrevTransforms.Num());
		if (Transforms.Num() != PrevTransforms.Num())
		{
			ChangedCount = Transforms.Num();
		}
		else
		{
			ChangedCount = 0;
			for (int32 TransformIndex = 0; TransformIndex < Transforms.Num(); ++TransformIndex)
			{
				if (!PrevTransforms[TransformIndex].Equals(Transforms[TransformIndex], EqualTolerance))
				{
					++ChangedCount;
				}
			}
		}
	}
};


class FGeometryCollectionDynamicDataPool
{
public:
	FGeometryCollectionDynamicDataPool();
	~FGeometryCollectionDynamicDataPool();

	FGeometryCollectionDynamicData* Allocate();
	void Release(FGeometryCollectionDynamicData* DynamicData);

private:
	TArray<FGeometryCollectionDynamicData*> UsedList;
	TArray<FGeometryCollectionDynamicData*> FreeList;

	FCriticalSection ListLock;
};


/***
*   FGeometryCollectionSceneProxy
*    
*	The FGeometryCollectionSceneProxy manages the interaction between the GeometryCollectionComponent
*   on the game thread and the vertex buffers on the render thread.
*
*   NOTE : This class is still in flux, and has a few pending todos. Your comments and 
*   thoughts are appreciated though. The remaining items to address involve:
*   - @todo double buffer - The double buffering of the FGeometryCollectionDynamicData.
*   - @todo previous state - Saving the previous FGeometryCollectionDynamicData for rendering motion blur.
*   - @todo GPU skin : Make the skinning use the GpuVertexShader
*/
class FGeometryCollectionSceneProxy final : public FPrimitiveSceneProxy
{
	TArray<UMaterialInterface*> Materials;

	FMaterialRelevance MaterialRelevance;

	FGeometryCollectionMeshResources const& MeshResource;
	FGeometryCollectionMeshDescription MeshDescription;

	int32 NumTransforms = 0;
	TArray<FMatrix44f> RestTransforms;

	FBoxSphereBounds PreSkinnedBounds;

	FGeometryCollectionVertexFactory VertexFactory;
	
	bool bSupportsManualVertexFetch;
	FPositionVertexBuffer SkinnedPositionVertexBuffer;

	int32 CurrentTransformBufferIndex = 0;
	bool TransformVertexBuffersContainsRestTransforms = true;
	bool bSupportsTripleBufferVertexUpload = false;
	bool bRenderResourcesCreated = false;
	TArray<FGeometryCollectionTransformBuffer, TInlineAllocator<3>> TransformBuffers;
	TArray<FGeometryCollectionTransformBuffer, TInlineAllocator<3>> PrevTransformBuffers;

	FGeometryCollectionDynamicData* DynamicData = nullptr;

#if WITH_EDITOR
	bool bShowBoneColors = false;
	bool bSuppressSelectionMaterial = false;
	TArray<FColor> BoneColors;
	FColorVertexBuffer ColorVertexBuffer;
	FGeometryCollectionVertexFactory VertexFactoryDebugColor;
	UMaterialInterface* BoneSelectedMaterial = nullptr;
	TArray<bool> HiddenTransforms;
#endif

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	bool bUsesSubSections = false;
	bool bEnableBoneSelection = false;
	TArray<TRefCountPtr<HHitProxy>> HitProxies;
	FColorVertexBuffer HitProxyIdBuffer;
#endif

#if RHI_RAYTRACING
	bool bGeometryResourceUpdated = false;
	FRayTracingGeometry RayTracingGeometry;
	FRWBuffer RayTracingDynamicVertexBuffer;
#endif

public:
	FGeometryCollectionSceneProxy(UGeometryCollectionComponent* Component);
	virtual ~FGeometryCollectionSceneProxy();

	/** Called on render thread to setup dynamic geometry for rendering */
	void SetDynamicData_RenderThread(FRHICommandListBase& RHICmdList, FGeometryCollectionDynamicData* NewDynamicData);

	uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
	uint32 GetAllocatedSize() const;

	SIZE_T GetTypeHash() const override;
	void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	void DestroyRenderThreadResources() override;
	void GetPreSkinnedLocalBounds(FBoxSphereBounds& OutBounds) const override;
	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual bool AllowInstanceCullingOcclusionQueries() const override { return true; }

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
 	virtual const FColorVertexBuffer* GetCustomHitProxyIdBuffer() const override
 	{
		return (bEnableBoneSelection || bUsesSubSections) ? &HitProxyIdBuffer : nullptr;
 	}
#endif

#if RHI_RAYTRACING
	bool IsRayTracingRelevant() const override { return true; }
	bool IsRayTracingStaticRelevant() const override { return false; }
	void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances) override;
#endif

protected:
	/** Setup a geometry collection vertex factory. */
	void SetupVertexFactory(FRHICommandListBase& RHICmdList, FGeometryCollectionVertexFactory& GeometryCollectionVertexFactory, FColorVertexBuffer* ColorOverride = nullptr) const;
	/** Update skinned position buffer used by mobile CPU skinning path. */
	void UpdateSkinnedPositions(FRHICommandListBase& RHICmdList, TArray<FMatrix44f> const& Transforms);
	/** Get material proxy from material ID */
	FMaterialRenderProxy* GetMaterial(FMeshElementCollector& Collector, int32 MaterialIndex) const;
	/** Get the standard or debug vertex factory dependent on current state. */
	FVertexFactory const* GetVertexFactory() const;

	FGeometryCollectionTransformBuffer& GetCurrentTransformBuffer()
	{
		return TransformBuffers[CurrentTransformBufferIndex];
	}
	FGeometryCollectionTransformBuffer& GetCurrentPrevTransformBuffer()
	{
		return PrevTransformBuffers[CurrentTransformBufferIndex];
	}

	void CycleTransformBuffers(bool bCycle)
	{
		if (bCycle)
		{
			CurrentTransformBufferIndex = (CurrentTransformBufferIndex + 1) % TransformBuffers.Num();
		}
	}

#if RHI_RAYTRACING
	void UpdatingRayTracingGeometry_RenderingThread(TArray<FGeometryCollectionMeshElement> const& InSectionArray);
#endif
};


class FNaniteGeometryCollectionSceneProxy : public Nanite::FSceneProxyBase
{
public:
	using Super = Nanite::FSceneProxyBase;
	
	FNaniteGeometryCollectionSceneProxy(UGeometryCollectionComponent* Component);
	virtual ~FNaniteGeometryCollectionSceneProxy() = default;

public:
	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual SIZE_T GetTypeHash() const override;
	virtual FPrimitiveViewRelevance	GetViewRelevance(const FSceneView* View) const override;
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;

	virtual uint32 GetMemoryFootprint() const override;

	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override;

	// FSceneProxyBase interface.
	virtual void GetNaniteResourceInfo(uint32& ResourceID, uint32& HierarchyOffset, uint32& ImposterIndex) const override;
	virtual void GetNaniteMaterialMask(FUint32Vector2& OutMaterialMask) const override;

	virtual Nanite::FResourceMeshInfo GetResourceMeshInfo() const override;

	/** Called on render thread to setup dynamic geometry for rendering */
	void SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData, const FMatrix &PrimitiveLocalToWorld);

	void ResetPreviousTransforms_RenderThread();

	void FlushGPUSceneUpdate_GameThread();

	FORCEINLINE void SetRequiresGPUSceneUpdate_RenderThread(bool bRequireUpdate)
	{
		bRequiresGPUSceneUpdate = bRequireUpdate;
	}

	FORCEINLINE bool GetRequiresGPUSceneUpdate_RenderThread() const
	{
		return bRequiresGPUSceneUpdate;
	}

	void OnMotionBegin();
	void OnMotionEnd();

protected:
	// TODO : Copy required data from UObject instead of using unsafe object pointer.
	const UGeometryCollection* GeometryCollection = nullptr;

	struct FGeometryNaniteData
	{
		FBoxSphereBounds LocalBounds;
		uint32 HierarchyOffset;
	};
	TArray<FGeometryNaniteData> GeometryNaniteData;

	uint32 NaniteResourceID = INDEX_NONE;
	uint32 NaniteHierarchyOffset = INDEX_NONE;

	// TODO: Should probably calculate this on the materials array above instead of on the component
	//       Null and !Opaque are assigned default material unlike the component material relevance.
	FMaterialRelevance MaterialRelevance;

	uint32 bCastShadow : 1;
	uint32 bReverseCulling : 1;
	uint32 bHasMaterialErrors : 1;
	uint32 bCurrentlyInMotion : 1;
	uint32 bRequiresGPUSceneUpdate : 1;

	FInstanceSceneDataBuffers InstanceSceneDataBuffersImpl;
};
