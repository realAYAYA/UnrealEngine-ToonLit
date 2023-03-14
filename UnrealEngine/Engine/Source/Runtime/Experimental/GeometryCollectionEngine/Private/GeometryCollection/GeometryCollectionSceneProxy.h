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

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
#include "GeometryCollection/GeometryCollectionHitProxy.h"
#endif

class UGeometryCollection;
class UGeometryCollectionComponent;
struct FGeometryCollectionSection;
struct HGeometryCollection;

namespace Nanite
{
	struct FResources;
}

/** Index Buffer */
class FGeometryCollectionIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FGeometryCollectionIndexBuffer"));
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), NumIndices * sizeof(int32), BUF_Dynamic, CreateInfo);
	}

	int32 NumIndices;
};

/** Vertex Buffer for Bone Map*/
class FGeometryCollectionBoneMapBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FGeometryCollectionBoneMapBuffer"));

		// #note: Bone Map is stored in uint16, but shaders only support uint32
		VertexBufferRHI = RHICreateVertexBuffer(NumVertices * sizeof(uint32), BUF_Static | BUF_ShaderResource, CreateInfo);		
		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_UINT);		
	}

	int32 NumVertices;

	FShaderResourceViewRHIRef VertexBufferSRV;
};

/** Vertex Buffer for transform data */
class FGeometryCollectionTransformBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FGeometryCollectionTransformBuffer"));

		// #note: This differs from instanced static mesh in that we are storing the entire transform in the buffer rather than
		// splitting out the translation.  This is to simplify transferring data at runtime as a memcopy
		VertexBufferRHI = RHICreateVertexBuffer(NumTransforms * sizeof(FVector4f) * 4, BUF_Dynamic | BUF_ShaderResource, CreateInfo);		
		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, 16, PF_A32B32G32R32F);
	}

	void UpdateDynamicData(const TArray<FMatrix44f>& Transforms, EResourceLockMode LockMode);

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

/** Immutable rendering data (kind of) */
struct FGeometryCollectionConstantData
{
	TArray<FVector3f> Vertices;
	TArray<FIntVector> Indices;
	TArray<FVector3f> Normals;
	TArray<FVector3f> TangentU;
	TArray<FVector3f> TangentV;
	TArray<TArray<FVector2f>> UVs;
	TArray<FLinearColor> Colors;
	TArray<int32> BoneMap;
	TArray<FLinearColor> BoneColors;
	TArray<FGeometryCollectionSection> Sections;

	uint32 NumTransforms;

	FBox LocalBounds;
	
	TArray<FIntVector> OriginalMeshIndices;
	TArray<FGeometryCollectionSection> OriginalMeshSections;

	TArray<FMatrix44f> RestTransforms;

	void SetRestTransforms(const TArray<FMatrix>& InTransforms)
	{
		// use for LWC as FMatrix and FMatrix44f are different when LWC is on 
		CopyTransformsWithConversionWhenNeeded(RestTransforms, InTransforms);
	}
};

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

	void SetTransforms(const TArray<FMatrix>& InTransforms)
	{
		// use for LWC as FMatrix and FMatrix44f are different when LWC is on 
		CopyTransformsWithConversionWhenNeeded(Transforms, InTransforms);
	}

	void SetPrevTransforms(const TArray<FMatrix>& InTransforms)
	{
		// use for LWC as FMatrix and FMatrix44f are different when LWC is on 
		CopyTransformsWithConversionWhenNeeded(PrevTransforms, InTransforms);
	}

	void SetAllTransforms(const TArray<FMatrix>& InTransforms)
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
*   - @todo shared memory model - The Asset(or Actor?) should hold the Vertex buffer, and pass the reference to the SceneProxy
*   - @todo GPU skin : Make the skinning use the GpuVertexShader
*/
class FGeometryCollectionSceneProxy final : public FPrimitiveSceneProxy
{
	TArray<UMaterialInterface*> Materials;

	FMaterialRelevance MaterialRelevance;

	int32 NumVertices;
	int32 NumIndices;

	FGeometryCollectionVertexFactory VertexFactory;
	
	bool bSupportsManualVertexFetch;
	const bool bSupportsTripleBufferVertexUpload;
	
	FStaticMeshVertexBuffers VertexBuffers;
	FGeometryCollectionIndexBuffer IndexBuffer;
	FGeometryCollectionIndexBuffer OriginalMeshIndexBuffer;
	FGeometryCollectionBoneMapBuffer BoneMapBuffer;
	TArray<FGeometryCollectionTransformBuffer, TInlineAllocator<3>> TransformBuffers;
	TArray<FGeometryCollectionTransformBuffer, TInlineAllocator<3>> PrevTransformBuffers;

	int32 CurrentTransformBufferIndex = 0;
	FBoxSphereBounds PreSkinnedBounds;

	TArray<FGeometryCollectionSection> Sections;
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	FColorVertexBuffer HitProxyIdBuffer;
	TArray<FGeometryCollectionSection> SubSections;
	TArray<TRefCountPtr<HGeometryCollection>> SubSectionHitProxies;
	TMap<int32, int32> SubSectionHitProxyIndexMap;
	// @todo FractureTools - Reconcile with SubSectionHitProxies.  Currently subsection hit proxies dont work for per-vertex submission
	TArray<TRefCountPtr<HGeometryCollectionBone>> PerBoneHitProxies;
	FColor WholeObjectHitProxyColor = FColor(EForceInit::ForceInit);
	bool bUsesSubSections;
#endif

	FGeometryCollectionDynamicData* DynamicData;
	FGeometryCollectionConstantData* ConstantData;

	bool bShowBoneColors;
	bool bEnableBoneSelection;
	bool bSuppressSelectionMaterial;
	int BoneSelectionMaterialID;

	bool bUseFullPrecisionUVs = false;

	bool TransformVertexBuffersContainsOriginalMesh;

public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FGeometryCollectionSceneProxy(UGeometryCollectionComponent* Component);

	/** virtual destructor */
	virtual ~FGeometryCollectionSceneProxy();

	void DestroyRenderThreadResources()override;

	/** Current number of vertices to render */
	int32 GetRequiredVertexCount() const { return NumVertices; }

	/** Current number of indices to connect */
	int32 GetRequiredIndexCount() const { return NumIndices; }

	/** Called on render thread to setup static geometry for rendering */
	void SetConstantData_RenderThread(FGeometryCollectionConstantData* NewConstantData, bool ForceInit = false);

	/** Called on render thread to setup dynamic geometry for rendering */
	void SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData);

	/** Called on render thread to construct the vertex definitions */
	void BuildGeometry(const FGeometryCollectionConstantData* ConstantDataIn, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices, TArray<int32> &OutOriginalMeshIndices);

	/** Called on render thread to setup dynamic geometry for rendering */
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

#if RHI_RAYTRACING
	virtual bool					IsRayTracingRelevant() const { return true; }
	virtual bool					IsRayTracingStaticRelevant() const { return false; }
	virtual void					GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances) override;

	void UpdatingRayTracingGeometry_RenderingThread(FGeometryCollectionIndexBuffer* IndexBuffer);
#endif

	/** Manage the view assignment */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	// @todo allocated size : make this reflect internally allocated memory. 
	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	/** Size of the base class */
	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
	virtual const FColorVertexBuffer* GetCustomHitProxyIdBuffer() const override
	{
		// Note: Could return nullptr when bEnableBoneSelection is false if the hitproxy shader was made to not require per-vertex hit proxy IDs in that case
		return &HitProxyIdBuffer;
	}
#endif // WITH_EDITOR

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	/** Enable/disable the per transform selection mode. 
	 *  This forces more sections/mesh batches to be sent to the renderer while also allowing the editor
	 *  to return a special HitProxy containing the transform index of the section that has been clicked on.
	 */
	void UseSubSections(bool bInUsesSubSections, bool bForceInit);
#endif

	void GetPreSkinnedLocalBounds(FBoxSphereBounds& OutBounds) const override;

	void SetupVertexFactory(FGeometryCollectionVertexFactory& GeometryCollectionVertexFactory) const;

protected:

	/** Create the rendering buffer resources */
	void InitResources();

	/** Return the rendering buffer resources */
	void ReleaseResources();

	/** Get material proxy from material ID */
	FMaterialRenderProxy* GetMaterial(FMeshElementCollector& Collector, int32 MaterialIndex) const;

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

private:
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	/** Create transform index based subsections for all current sections. */
	void InitializeSubSections_RenderThread();

	/** Release subsections by emptying the associated arrays. */
	void ReleaseSubSections_RenderThread();
#endif

#if RHI_RAYTRACING
	bool bGeometryResourceUpdated = false;
	FRayTracingGeometry RayTracingGeometry;
	FRWBuffer RayTracingDynamicVertexBuffer;
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
	virtual SIZE_T GetTypeHash() const override;
	virtual FPrimitiveViewRelevance	GetViewRelevance(const FSceneView* View) const override;
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;

	virtual uint32 GetMemoryFootprint() const override;

	virtual void OnTransformChanged() override;

	// FSceneProxyBase interface.
	virtual void GetNaniteResourceInfo(uint32& ResourceID, uint32& HierarchyOffset, uint32& ImposterIndex) const override;

	virtual Nanite::FResourceMeshInfo GetResourceMeshInfo() const override;

	/** Called on render thread to setup static geometry for rendering */
	void SetConstantData_RenderThread(FGeometryCollectionConstantData* NewConstantData, bool ForceInit = false);

	/** Called on render thread to setup dynamic geometry for rendering */
	void SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData);

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
};
