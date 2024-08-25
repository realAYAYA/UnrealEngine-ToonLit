// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.h: GPU skinned mesh object and resource definitions
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "RenderResource.h"
#include "RayTracingGeometry.h"
#include "ShaderParameters.h"
#include "Components/ExternalMorphSet.h"
#include "Components/SkinnedMeshComponent.h"
#include "GlobalShader.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderPublic.h"
#include "ClothingSystemRuntimeTypes.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Animation/MeshDeformerGeometry.h"

enum class EGPUSkinCacheEntryMode;
class FGPUSkinCache;
class FRayTracingSkinnedGeometryUpdateQueue;
class FSkeletalMeshObjectGPUSkin;
class FVertexOffsetBuffers;

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataGPUSkin
{
	/**
	* Constructor, these are recycled, so you never use a constructor
	*/
	FDynamicSkelMeshObjectDataGPUSkin()
	{
		Clear();
	}

	virtual ~FDynamicSkelMeshObjectDataGPUSkin()
	{
	}

	ENGINE_API void Clear();

public:

	static ENGINE_API FDynamicSkelMeshObjectDataGPUSkin* AllocDynamicSkelMeshObjectDataGPUSkin();
	static ENGINE_API void FreeDynamicSkelMeshObjectDataGPUSkin(FDynamicSkelMeshObjectDataGPUSkin* Who);

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	InActiveMorphTargets - morph targets active for the mesh
	* @param	InMorphTargetWeights - All morph target weights for the mesh
	*/
	ENGINE_API void InitDynamicSkelMeshObjectDataGPUSkin(
		USkinnedMeshComponent* InMeshComponent,
		FSkeletalMeshRenderData* InSkeletalMeshRenderData,
		FSkeletalMeshObjectGPUSkin* InMeshObject,
		int32 InLODIndex,
		const FMorphTargetWeightMap& InActiveMorphTargets,
		const TArray<float>& InMorphTargetWeights, 
		EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode,
		const FExternalMorphWeightData& InExternalMorphWeightData);

	/** ref pose to local space transforms */
	TArray<FMatrix44f> ReferenceToLocal;
	TArray<FMatrix44f> ReferenceToLocalForRayTracing;

	/** Previous ref pose to local space transform */
	TArray<FMatrix44f> PreviousReferenceToLocal;
	TArray<FMatrix44f> PreviousReferenceToLocalForRayTracing;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	/** component space bone transforms*/
	TArray<FTransform> MeshComponentSpaceTransforms;
#endif

	/** currently LOD for bones being updated */
	int32 LODIndex;
#if RHI_RAYTRACING
	int32 RayTracingLODIndex;
#endif
	/** current morph targets active on this mesh */
	FMorphTargetWeightMap ActiveMorphTargets;
	/** All morph target weights on this mesh */
	TArray<float> MorphTargetWeights;
	/** All section ID impacted by active morph target on this mesh */
	TArray<int32> SectionIdsUseByActiveMorphTargets;
	TArray<int32> SectionIdsUseByActiveMorphTargetsForRayTracing;
	/** number of active morph targets with weights > 0 */
	int32 NumWeightedActiveMorphTargets;

	/** 
	 * The dynamic data for each external morph target set.
	 * This dynamic data contains things such as the weights for each set of external morph targets.
	 */
	FExternalMorphWeightData ExternalMorphWeightData;

	/** The external morph target sets for this specific LOD. */
	FExternalMorphSets ExternalMorphSets;

	/** data for updating cloth section */
	TMap<int32, FClothSimulData> ClothingSimData;

    /** store transform of the cloth object **/
    FMatrix ClothObjectLocalToWorld;

	/** store transform of the object **/
	FMatrix LocalToWorld;

	/** a weight factor to blend between simulated positions and skinned positions */	
	float ClothBlendWeight;

	/**
	* Compare the given set of active morph targets with the current list to check if different
	* @param CompareActiveMorphTargets - array of morphs to compare
	* @param MorphTargetWeights - array of morphs weights to compare
	* @return true if both sets of active morphs are equal
	*/
	ENGINE_API bool ActiveMorphTargetsEqual(const FMorphTargetWeightMap& InCompareActiveMorphTargets, const TArray<float>& CompareMorphTargetWeights);
	
	/** Returns the size of memory allocated by render data */
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
		
		CumulativeResourceSize.AddUnknownMemoryBytes(ReferenceToLocal.GetAllocatedSize());
		CumulativeResourceSize.AddUnknownMemoryBytes(ActiveMorphTargets.GetAllocatedSize());
	}

	/** Update Simulated Positions & Normals from APEX Clothing actor */
	UE_DEPRECATED(5.2, "Use USkinnedMeshComponent::GetUpdateClothSimulationData_AnyThread() instead.")
	ENGINE_API bool UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent);

	// Whether this LOD is allowed to use the skin cache feature
	uint8 bIsSkinCacheAllowed : 1;
	
	// Whether animation is done with a mesh deformer.
	uint8 bHasMeshDeformer : 1;

	// Whether to update dynamic bone & cloth sim data immediately, not to wait until GDME or defer update to RHIThread.
	// When set to true, it is the equivalent of r.DeferSkeletalDynamicDataUpdateUntilGDME=0 and r.RHICmdDeferSkeletalLockAndFillToRHIThread=0.
	// When set to false, r.DeferSkeletalDynamicDataUpdateUntilGDME and r.RHICmdDeferSkeletalLockAndFillToRHIThread values are respected.
	uint8 bForceUpdateDynamicDataImmediately : 1;

#if RHI_RAYTRACING
	uint8 bAnySegmentUsesWorldPositionOffset : 1;
#endif
};

/** morph target mesh data for a single vertex delta */
struct FMorphGPUSkinVertex
{
	// Changes to this struct must be reflected in MorphTargets.usf!
	FVector3f			DeltaPosition;
	FVector3f			DeltaTangentZ;

	FMorphGPUSkinVertex() {};
	
	/** Construct for special case **/
	FMorphGPUSkinVertex(const FVector3f& InDeltaPosition, const FVector3f& InDeltaTangentZ)
	{
		DeltaPosition = InDeltaPosition;
		DeltaTangentZ = InDeltaTangentZ;
	}
};

/**
* MorphTarget vertices which have been combined into single position/tangentZ deltas
*/
class FMorphVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Default Constructor
	*/
	FMorphVertexBuffer()
		: bHasBeenUpdated(false)
		, bNeedsInitialClear(true)
		, bUsesComputeShader(false)
		, LODIdx(-1)
		, SkelMeshRenderData(nullptr)
	{
	}

	/** 
	* Constructor
	* @param	InSkelMeshRenderData	- render data containing the data for each LOD
	* @param	InLODIdx				- index of LOD model to use from the parent mesh
	*/
	FMorphVertexBuffer(FSkeletalMeshRenderData* InSkelMeshRenderData, int32 InLODIdx, ERHIFeatureLevel::Type InFeatureLevel)
		:	bHasBeenUpdated(false)	
		,	bNeedsInitialClear(true)
		,	LODIdx(InLODIdx)
		,	FeatureLevel(InFeatureLevel)
		,	SkelMeshRenderData(InSkelMeshRenderData)
	{
		check(SkelMeshRenderData);
		check(SkelMeshRenderData->LODRenderData.IsValidIndex(LODIdx));
		bUsesComputeShader = false;
	}
	/** 
	* Initialize the dynamic RHI for this rendering resource 
	*/
	virtual void InitRHI(FRHICommandListBase& RHICmdList);

	/** 
	* Release the dynamic RHI for this rendering resource 
	*/
	virtual void ReleaseRHI();

	inline void RecreateResourcesIfRequired(FRHICommandListBase& RHICmdList, bool bInUsesComputeShader)
	{
		if (bUsesComputeShader != bInUsesComputeShader)
		{
			UpdateRHI(RHICmdList);
		}
	}

	/** 
	* Morph target vertex name 
	*/
	virtual FString GetFriendlyName() const { return TEXT("Morph target mesh vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitRHI - how much they allocate when initialize
	 */
	SIZE_T GetResourceSize() const
	{
		SIZE_T ResourceSize = sizeof(*this);

		if (VertexBufferRHI)
		{
			// LOD of the skel mesh is used to find number of vertices in buffer
			FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData->LODRenderData[LODIdx];

			// Create the buffer rendering resource
			ResourceSize += LodData.GetNumVertices() * sizeof(FMorphGPUSkinVertex);
		}

		return ResourceSize;
	}
	
	SIZE_T GetNumVerticies() const
	{
		// LOD of the skel mesh is used to find number of vertices in buffer
		FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData->LODRenderData[LODIdx];
		// Create the buffer rendering resource
		return LodData.GetNumVertices();
	}

	/** Has been updated or not by UpdateMorphVertexBuffer**/
	bool bHasBeenUpdated;

	/** DX12 cannot clear the buffer in InitRHI with UAV flag enables, we should really have a Zero initzialized flag instead**/
	bool bNeedsInitialClear;

	// @param guaranteed only to be valid if the vertex buffer is valid
	FRHIShaderResourceView* GetSRV() const
	{
		return SRVValue;
	}

	// @param guaranteed only to be valid if the vertex buffer is valid
	FRHIUnorderedAccessView* GetUAV() const
	{
		return UAVValue;
	}

	FSkeletalMeshLODRenderData* GetLODRenderData() const { return &SkelMeshRenderData->LODRenderData[LODIdx]; }
	
	// section ids that are using this Morph buffer
	TArray<int32> SectionIds;
protected:
	// guaranteed only to be valid if the vertex buffer is valid
	FShaderResourceViewRHIRef SRVValue;

	// guaranteed only to be valid if the vertex buffer is valid
	FUnorderedAccessViewRHIRef UAVValue;

	bool bUsesComputeShader;

private:
	/** index to the SkelMeshResource.LODModels */
	int32	LODIdx;

	ERHIFeatureLevel::Type FeatureLevel;

	// parent mesh containing the source data, never 0
	FSkeletalMeshRenderData* SkelMeshRenderData;

	friend class FMorphVertexBufferPool;
};

/**
* Pooled morph vertex buffers that store the vertex deltas.
*/
class FMorphVertexBufferPool : public FThreadSafeRefCountedObject
{
public:
	FMorphVertexBufferPool(FSkeletalMeshRenderData* InSkelMeshRenderData, int32 InLOD, ERHIFeatureLevel::Type InFeatureLevel)
	{
		MorphVertexBuffers[0] = FMorphVertexBuffer(InSkelMeshRenderData, InLOD, InFeatureLevel);
		MorphVertexBuffers[1] = FMorphVertexBuffer(InSkelMeshRenderData, InLOD, InFeatureLevel);
	}

	~FMorphVertexBufferPool()
	{
		// Note that destruction of this class must occur on the render thread if InitResources has been called!
		// This is normally pointed to by FSkeletalMeshObjectGPUSkin, which is defer deleted on the render thread.
		if (bInitializedResources)
		{
			ReleaseResources();
		}
	}

	void InitResources(const FName& OwnerName);
	void ReleaseResources();
	SIZE_T GetResourceSize() const;
	void EnableDoubleBuffer(FRHICommandListBase& RHICmdList);
	bool IsInitialized() const						{ return bInitializedResources; }
	bool IsDoubleBuffered() const					{ return bDoubleBuffer; }
	void SetUpdatedFrameNumber(uint32 FrameNumber)	{ UpdatedFrameNumber = FrameNumber; }
	uint32 GetUpdatedFrameNumber() const			{ return UpdatedFrameNumber; }
	void SetCurrentRevisionNumber(uint32 RevisionNumber);
	const FMorphVertexBuffer& GetMorphVertexBufferForReading(bool bPrevious) const;
	FMorphVertexBuffer& GetMorphVertexBufferForWriting();

private:
	/** Vertex buffer that stores the morph target vertex deltas. */
	FMorphVertexBuffer MorphVertexBuffers[2];
	/** If data is preserved when recreating render state, resources will already be initialized, so we need a flag to track that. */
	bool bInitializedResources = false;
	/** whether to double buffer. If going through skin cache, then use single buffer; otherwise double buffer. */
	bool bDoubleBuffer = false;

	// 0 / 1 to index into MorphVertexBuffer
	uint32 CurrentBuffer = 0;
	// RevisionNumber Tracker
	uint32 PreviousRevisionNumber = 0;
	uint32 CurrentRevisionNumber = 0;
	// Frame number of the morph vertex buffer that is last updated
	uint32 UpdatedFrameNumber = 0;
};

/**
 * Render data for a GPU skinned mesh
 */
class FSkeletalMeshObjectGPUSkin : public FSkeletalMeshObject
{
public:
	/** @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render */
	ENGINE_API FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshRenderData* InSkelMeshRenderData, ERHIFeatureLevel::Type InFeatureLevel);
	ENGINE_API virtual ~FSkeletalMeshObjectGPUSkin();

	//~ Begin FSkeletalMeshObject Interface
	ENGINE_API virtual void InitResources(USkinnedMeshComponent* InMeshComponent) override;
	ENGINE_API virtual void ReleaseResources() override;
	ENGINE_API virtual void Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const FMorphTargetWeightMap& InActiveMorphTargets, const TArray<float>& InMorphTargetWeights, EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode, const FExternalMorphWeightData& InExternalMorphWeightData) override;
	ENGINE_API void UpdateDynamicData_RenderThread(FGPUSkinCache* GPUSkinCache, FRHICommandList& RHICmdList, FDynamicSkelMeshObjectDataGPUSkin* InDynamicData, FSceneInterface* Scene, uint64 FrameNumberToPrepare, uint32 RevisionNumber, uint32 PreviousRevisionNumber, bool bRecreating);
	ENGINE_API virtual void PreGDMECallback(FRHICommandList& RHICmdList, FGPUSkinCache* GPUSkinCache, uint32 FrameNumber) override;
	ENGINE_API virtual const FVertexFactory* GetSkinVertexFactory(const FSceneView* View, int32 LODIndex,int32 ChunkIdx, ESkinVertexFactoryMode VFMode = ESkinVertexFactoryMode::Default) const override;
	ENGINE_API virtual const FSkinBatchVertexFactoryUserData* GetVertexFactoryUserData(const int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode) const override;
	virtual bool IsCPUSkinned() const override { return false; }
	virtual bool IsGPUSkinMesh() const override { return true; }
	ENGINE_API virtual TArray<FTransform>* GetComponentSpaceTransforms() const override;
	ENGINE_API virtual const TArray<FMatrix44f>& GetReferenceToLocalMatrices() const override;
	ENGINE_API virtual bool GetCachedGeometry(FCachedGeometry& OutCachedGeometry) const override;
	
	ENGINE_API FMeshDeformerGeometry& GetDeformerGeometry(int32 LODIndex);

#if RHI_RAYTRACING
	/** Geometry for ray tracing. */
	FRayTracingGeometry RayTracingGeometry;
	FRayTracingAccelerationStructureSize RayTracingGeometryStructureSize;
	FRWBuffer RayTracingDynamicVertexBuffer;
	FRayTracingSkinnedGeometryUpdateQueue* RayTracingUpdateQueue;

	virtual FRayTracingGeometry* GetRayTracingGeometry() { return &RayTracingGeometry; }
	virtual const FRayTracingGeometry* GetRayTracingGeometry() const { return &RayTracingGeometry; }

	/** Return the internal vertex buffer only when initialized otherwise used the shared vertex buffer - needs to be updated every frame */
	virtual FRWBuffer* GetRayTracingDynamicVertexBuffer() { return RayTracingDynamicVertexBuffer.NumBytes > 0 ? &RayTracingDynamicVertexBuffer : nullptr; }

	virtual int32 GetRayTracingLOD() const override
	{
		if (DynamicData)
		{
			return DynamicData->RayTracingLODIndex;
		}
		else
		{
			return 0;
		}
	}

	/** 
	 * Directly update ray tracing geometry. 
	 * This is quicker than the generic dynamic VSinCS path. 
	 * VSinCS path is still required for world position offset materials but this can still use 
	 * the updated vertex buffers from here with a passthrough vertex factory.
	 */
	ENGINE_API void UpdateRayTracingGeometry(FRHICommandListBase& RHICmdList, FSkeletalMeshLODRenderData& LODModel, uint32 LODIndex, TArray<FBufferRHIRef>& VertexBuffers);

#endif // RHI_RAYTRACING

	virtual int32 GetLOD() const override
	{
		if(DynamicData)
		{
			return DynamicData->LODIndex;
		}
		else
		{
			return 0;
		}
	}

	virtual bool HaveValidDynamicData() const override
	{ 
		return ( DynamicData!=NULL ); 
	}

	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
		
		if(DynamicData)
		{
			DynamicData->GetResourceSizeEx(CumulativeResourceSize);
		}
		
		CumulativeResourceSize.AddUnknownMemoryBytes(LODs.GetAllocatedSize()); 

		// include extra data from LOD
		for (int32 I=0; I<LODs.Num(); ++I)
		{
			LODs[I].GetResourceSizeEx(CumulativeResourceSize);
		}
	}
	//~ End FSkeletalMeshObject Interface

	/**
	 * Calculate how many GPU compressed morph target sets are active.
	 * This includes regular morph targets as well as external morph targets.
	 */
	ENGINE_API int32 CalcNumActiveGPUMorphSets(FMorphVertexBuffer& MorphVertexBuffer, const FExternalMorphSets& ExternalMorphSets) const;

	/** Check if a given morph set is active or not. If so, we will process it. */
	ENGINE_API bool IsExternalMorphSetActive(int32 MorphSetID, const FExternalMorphSet& MorphSet) const;

	ENGINE_API FSkinWeightVertexBuffer* GetSkinWeightVertexBuffer(int32 LODIndex) const;

	/** 
	 * Get the skin vertex factory for direct skinning. 
	 * This is different from GetSkinVertexFactory because it ignores any passthrough vertex factories that may be in use.
	 */
	ENGINE_API FGPUBaseSkinVertexFactory const* GetBaseSkinVertexFactory(int32 LODIndex, int32 ChunkIdx) const;

	/** 
	 * Vertex buffers that can be used for GPU skinning factories 
	 */
	struct FVertexFactoryBuffers
	{
		FStaticMeshVertexBuffers* StaticVertexBuffers = nullptr;
		FSkinWeightVertexBuffer* SkinWeightVertexBuffer = nullptr;
		FColorVertexBuffer*	ColorVertexBuffer = nullptr;
		FMorphVertexBufferPool* MorphVertexBufferPool = nullptr;
		FSkeletalMeshVertexClothBuffer*	APEXClothVertexBuffer = nullptr;
		FVertexOffsetBuffers* VertexOffsetVertexBuffers = nullptr;
		uint32 NumVertices = 0;
	};

	ENGINE_API FMatrix GetTransform() const;
	ENGINE_API virtual void SetTransform(const FMatrix& InNewLocalToWorld, uint32 FrameNumber) override;
	ENGINE_API virtual void RefreshClothingTransforms(const FMatrix& InNewLocalToWorld, uint32 FrameNumber) override;
	ENGINE_API virtual void UpdateSkinWeightBuffer(USkinnedMeshComponent* InMeshComponent) override;

	static ENGINE_API void GetUsedVertexFactoryData(FSkeletalMeshRenderData* SkelMeshRenderData, int32 InLOD, USkinnedMeshComponent* SkinnedMeshComponent, FSkelMeshRenderSection& RenderSection, ERHIFeatureLevel::Type InFeatureLevel, bool bHasMorphTargets, FPSOPrecacheVertexFactoryDataList& VertexFactoryDataList);

protected:
	friend class FSkeletalMeshDeformerHelpers;

	/**
	 * Vertex factories and their matrix arrays
	 */
	class FVertexFactoryData
	{
	public:
		FVertexFactoryData() {}

		/** one vertex factory for each chunk */
		TArray<TUniquePtr<FGPUBaseSkinVertexFactory>> VertexFactories;

		/** one passthrough vertex factory for each chunk */
		TArray<TUniquePtr<FGPUSkinPassthroughVertexFactory>> PassthroughVertexFactories;

		/** Vertex factory defining both the base mesh as well as the morph delta vertex decals */
		TArray<TUniquePtr<FGPUBaseSkinVertexFactory>> MorphVertexFactories;

		/** Vertex factory defining both the base mesh as well as the APEX cloth vertex data */
		TArray<TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>> ClothVertexFactories;

		/** 
		 * Init default vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Sections - relevant section information (either original or from swapped influence)
		 */
		void InitVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshRenderSection>& Sections, ERHIFeatureLevel::Type FeatureLevel);
		/** 
		 * Release default vertex factory resources for this LOD 
		 */
		void ReleaseVertexFactories();
		/** 
		 * Init morph vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Sections - relevant section information (either original or from swapped influence)
		 */
		void InitMorphVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshRenderSection>& Sections, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type InFeatureLevel);
		/** 
		 * Release morph vertex factory resources for this LOD 
		 */
		void ReleaseMorphVertexFactories();
		/** 
		 * Init APEX cloth vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Sections - relevant section information (either original or from swapped influence)
		 */
		void InitAPEXClothVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshRenderSection>& Sections, ERHIFeatureLevel::Type InFeatureLevel);
		/** 
		 * Release morph vertex factory resources for this LOD 
		 */
		void ReleaseAPEXClothVertexFactories();
		
		/** Refreshes the VertexFactor::FDataType to rebind any vertex buffers */
		void UpdateVertexFactoryData(const FVertexFactoryBuffers& VertexBuffers);

		/**
		 * Clear factory arrays
		 */
		void ClearFactories()
		{
			VertexFactories.Empty();
			MorphVertexFactories.Empty();
			ClothVertexFactories.Empty();
		}

		/**
		 * @return memory in bytes of size of the vertex factories and their matrices
		 */
		SIZE_T GetResourceSize()
		{
			SIZE_T Size = 0;
			Size += VertexFactories.GetAllocatedSize();

			Size += MorphVertexFactories.GetAllocatedSize();

			Size += ClothVertexFactories.GetAllocatedSize();

			return Size;
		}	

		private:
			FVertexFactoryData(const FVertexFactoryData&);
			FVertexFactoryData& operator=(const FVertexFactoryData&);
	};

	/** vertex data for rendering a single LOD */
	struct FSkeletalMeshObjectLOD
	{
		FSkeletalMeshObjectLOD(FSkeletalMeshRenderData* InSkelMeshRenderData,int32 InLOD, ERHIFeatureLevel::Type InFeatureLevel, FMorphVertexBufferPool* InRecreateBufferPool)
			: SkelMeshRenderData(InSkelMeshRenderData)
			, LODIndex(InLOD)
			, FeatureLevel(InFeatureLevel)
			, MeshObjectWeightBuffer(nullptr)
			, MeshObjectColorBuffer(nullptr)
		{
			if (InRecreateBufferPool)
			{
				MorphVertexBufferPool = InRecreateBufferPool;
			}
			else
			{
				MorphVertexBufferPool = new FMorphVertexBufferPool(InSkelMeshRenderData, LODIndex, FeatureLevel);
			}
		}

		/** 
		 * Init rendering resources for this LOD 
		 * @param MeshLODInfo - information about the state of the bone influence swapping
		 * @param CompLODInfo - information about this LOD from the skeletal component 
		 */
		void InitResources(const FSkelMeshObjectLODInfo& MeshLODInfo, FSkelMeshComponentLODInfo* CompLODInfo, ERHIFeatureLevel::Type FeatureLevel);

		/** 
		 * Release rendering resources for this LOD 
		 */
		void ReleaseResources();

		/** 
		 * Init rendering resources for the morph stream of this LOD
		 * @param MeshLODInfo - information about the state of the bone influence swapping
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type FeatureLevel);

		/** 
		 * Release rendering resources for the morph stream of this LOD
		 */
		void ReleaseMorphResources();

		/**
		 * @return memory in bytes of size of the resources for this LOD
		 */
		void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(MorphVertexBufferPool->GetResourceSize());
			CumulativeResourceSize.AddUnknownMemoryBytes(GPUSkinVertexFactories.GetResourceSize());
		}

		FSkeletalMeshRenderData* SkelMeshRenderData;
		// index into FSkeletalMeshRenderData::LODRenderData[]
		int32 LODIndex;

		ERHIFeatureLevel::Type FeatureLevel;

		/** Pooled vertex buffers that store the morph target vertex deltas. */
		TRefCountPtr<FMorphVertexBufferPool> MorphVertexBufferPool;

		/** Default GPU skinning vertex factories and matrices */
		FVertexFactoryData GPUSkinVertexFactories;

		/** Skin weight buffer to use, could be from asset or component override */
		FSkinWeightVertexBuffer* MeshObjectWeightBuffer;

		/** Color buffer to user, could be from asset or component override */
		FColorVertexBuffer* MeshObjectColorBuffer;

		/** Mesh deformer output buffers */
		FMeshDeformerGeometry DeformerGeometry;

		/**
		 * Update the contents of the morphtarget vertex buffer by accumulating all 
		 * delta positions and delta normals from the set of active morph targets
		 * @param ActiveMorphTargets - Morph to accumulate. assumed to be weighted and have valid targets
		 * @param MorphTargetWeights - All Morph weights
		 */
		void UpdateMorphVertexBufferCPU(FRHICommandList& RHICmdList, const FMorphTargetWeightMap& InActiveMorphTargets, const TArray<float>& MorphTargetWeights, const TArray<int32>& SectionIdsUseByActiveMorphTargets, 
										bool bGPUSkinCacheEnabled, FMorphVertexBuffer& MorphVertexBuffer);

		void UpdateMorphVertexBufferGPU(FRHICommandList& RHICmdList, const TArray<float>& MorphTargetWeights, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers, 
										const TArray<int32>& SectionIdsUseByActiveMorphTargets, const FName& OwnerName, EGPUSkinCacheEntryMode Mode, FMorphVertexBuffer& MorphVertexBuffer,
										bool bClearMorphVertexBuffer, bool bNormalizePass, const FVector4& MorphScale, const FVector4& InvMorphScale);

		void UpdateSkinWeights(FSkelMeshComponentLODInfo* CompLODInfo);

		/**
		 * Determine the current vertex buffers valid for this LOD
		 *
		 * @param OutVertexBuffers output vertex buffers
		 */
		void GetVertexBuffers(FVertexFactoryBuffers& OutVertexBuffers, FSkeletalMeshLODRenderData& LODData);

		// Temporary arrays used on UpdateMorphVertexBuffer(); these grow to the max and are not thread safe.
		static TArray<float> MorphAccumulatedWeightArray;
	};

	/** 
	* Initialize morph rendering resources for each LOD 
	*/
	ENGINE_API void InitMorphResources(bool bInUsePerBoneMotionBlur, const TArray<float>& MorphTargetWeights);

	/** 
	* Release morph rendering resources for each LOD. 
	*/
	ENGINE_API void ReleaseMorphResources();

	ENGINE_API void ProcessUpdatedDynamicData(EGPUSkinCacheEntryMode Mode, FGPUSkinCache* GPUSkinCache, FRHICommandList& RHICmdList, uint32 FrameNumberToPrepare, uint32 RevisionNumber, uint32 PreviousRevisionNumber, bool bMorphNeedsUpdate, int32 LODIndex, bool bRecreating);

	ENGINE_API virtual void UpdateMorphVertexBuffer(FRHICommandList& RHICmdList, EGPUSkinCacheEntryMode Mode, FSkeletalMeshObjectLOD& LOD, const FSkeletalMeshLODRenderData& LODData, bool bGPUSkinCacheEnabled, FMorphVertexBuffer& MorphVertexBuffer);

	/** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

	/** Data that is updated dynamically and is needed for rendering */
	FDynamicSkelMeshObjectDataGPUSkin* DynamicData;

	/** True if we are doing a deferred update later in GDME. */
	bool bNeedsUpdateDeferred;

	/** If true and we are doing a deferred update, then also update the morphs */
	bool bMorphNeedsUpdateDeferred;

	/** true if the morph resources have been initialized */
	bool bMorphResourcesInitialized;

	/** last updated bone transform revision number */
	uint32 LastBoneTransformRevisionNumber;

	/** true to indicate that a subclass is handling the update of the morph vertex buffer and that it should always be called */
	bool bAlwaysUpdateMorphVertexBuffer;

private:
	ENGINE_API FSkeletalMeshObjectGPUSkin(const FSkeletalMeshObjectGPUSkin&);
	ENGINE_API FSkeletalMeshObjectGPUSkin& operator=(const FSkeletalMeshObjectGPUSkin&);
};


class FGPUMorphUpdateCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FGPUMorphUpdateCS, Global);

	FGPUMorphUpdateCS();
	FGPUMorphUpdateCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static const uint32 MorphTargetDispatchBatchSize = 128;

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		const FVector4& LocalScale,
		const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers,
		FMorphVertexBuffer& MorphVertexBuffer,
		uint32 NumGroups,
		uint32 BatchOffsets[MorphTargetDispatchBatchSize],
		uint32 GroupOffsets[MorphTargetDispatchBatchSize],
		float Weights[MorphTargetDispatchBatchSize]);

	void Dispatch(FRHICommandList& RHICmdList, uint32 Size);
	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

protected:
	LAYOUT_FIELD(FShaderResourceParameter, MorphVertexBufferParameter);

	LAYOUT_FIELD(FShaderParameter, MorphTargetWeightsParameter);
	LAYOUT_FIELD(FShaderParameter, OffsetAndSizeParameter);
	LAYOUT_FIELD(FShaderParameter, MorphTargetBatchOffsetsParameter);
	LAYOUT_FIELD(FShaderParameter, MorphTargetGroupOffsetsParameter);
	LAYOUT_FIELD(FShaderParameter, PositionScaleParameter);
	LAYOUT_FIELD(FShaderParameter, PrecisionParameter);
	LAYOUT_FIELD(FShaderParameter, NumGroupsParameter);

	LAYOUT_FIELD(FShaderResourceParameter, MorphDataBufferParameter);
};

class FGPUMorphNormalizeCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FGPUMorphNormalizeCS, Global);

	FGPUMorphNormalizeCS();
	FGPUMorphNormalizeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FVector4& LocalScale, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers, FMorphVertexBuffer& MorphVertexBuffer, uint32 NumVertices);

	void Dispatch(FRHICommandList& RHICmdList, uint32 NumVertices);
	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds);

protected:
	LAYOUT_FIELD(FShaderResourceParameter, MorphVertexBufferParameter);

	LAYOUT_FIELD(FShaderParameter, PositionScaleParameter);
	LAYOUT_FIELD(FShaderParameter, NumVerticesParameter);
};
