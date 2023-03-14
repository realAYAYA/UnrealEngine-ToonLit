// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "HairDescription.h"
#include "HairStrandsDatas.h"
#include "HairCardsDatas.h"
#include "RenderResource.h"
#include "HairStrandsInterface.h"
#include "HairStrandsMeshProjection.h"

#define STRANDS_PROCEDURAL_INTERSECTOR_MAX_SPLITS 4

inline uint32 GetBufferTotalNumBytes(const FRDGExternalBuffer& In) 
{
	return In.Buffer ? In.Buffer->GetSize() : 0;
}

enum class EHairStrandsResourcesType : uint8
{
	Guides,		// Guides used for simulation
	Strands,	// Rendering strands 
	Cards		// Guides used for deforming the cards geometry (which is different from the simulation guides)
};

enum class EHairStrandsAllocationType : uint8
{
	Immediate,	// Resources are allocated immediately
	Deferred	// Resources allocation is deferred to first usage
};

/* Hair resource name - Allow more precise resource name for debug purpose. Enabled with HAIR_RESOURCE_DEBUG_NAME */
#define HAIR_RESOURCE_DEBUG_NAME 0
struct FHairResourceName
{
	FHairResourceName() = default;
#if HAIR_RESOURCE_DEBUG_NAME
	FHairResourceName(const FName& In) : AssetName(In) {}
	FHairResourceName(const FName& In, int32 InGroupIndex) : AssetName(In), GroupIndex(InGroupIndex) {}
	FHairResourceName(const FName& In, int32 InGroupIndex, int32 InLODIndex) : AssetName(In), GroupIndex(InGroupIndex), LODIndex(InLODIndex) {}

	FName AssetName;
	int32 GroupIndex = -1;
	int32 LODIndex = -1;
	TArray<FString> Names;
#else
	FHairResourceName(const FName& In) {}
	FHairResourceName(const FName& In, int32 InGroupIndex) {}
	FHairResourceName(const FName& In, int32 InGroupIndex, int32 InLODIndex) {}
#endif
};

enum class EHairResourceLoadingType : uint8
{
	Async,
	Sync
};

enum class EHairResourceStatus : uint8
{
	None = 0,
	Loading = 1,
	Valid = 2
};

FORCEINLINE EHairResourceStatus  operator& (EHairResourceStatus A, EHairResourceStatus B) { return static_cast<EHairResourceStatus>(static_cast<uint8>(A) & static_cast<uint8>(B)); }
FORCEINLINE EHairResourceStatus  operator| (EHairResourceStatus A, EHairResourceStatus B) { return static_cast<EHairResourceStatus>(static_cast<uint8>(A) | static_cast<uint8>(B)); }
FORCEINLINE EHairResourceStatus& operator|=(EHairResourceStatus&A, EHairResourceStatus B) { return A=A|B; }
FORCEINLINE bool operator! (EHairResourceStatus A) { return static_cast<uint8>(A) != 0; }

EHairResourceLoadingType GetHairResourceLoadingType(EHairGeometryType InGeometryType, int32 InLODIndex);

/* Hair resouces which whom allocation can be deferred */
struct FHairCommonResource : public FRenderResource
{
	/** Build the hair strands resource */
	FHairCommonResource(EHairStrandsAllocationType AllocationType, const FHairResourceName& InResourceName, bool bUseRenderGraph=true);

	/* Init/Release buffers (FRenderResource) */
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	/* Init/Release buffers (FHairCommonResource) */
	void Allocate(FRDGBuilder& GraphBuilder, EHairResourceLoadingType LoadingType);
	void Allocate(FRDGBuilder& GraphBuilder, EHairResourceLoadingType LoadingType, EHairResourceStatus& Status);
	void AllocateLOD(FRDGBuilder& GraphBuilder, int32 LODIndex, EHairResourceLoadingType LoadingType, EHairResourceStatus& Status);

	void StreamInData();
	void StreamInLODData(int32 LODIndex);

	virtual void InternalAllocate() {}
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) {}
	virtual void InternalAllocateLOD(FRDGBuilder& GraphBuilder, int32 LODIndex) {}
	virtual void InternalRelease() {}
	virtual bool InternalIsDataLoaded() { return true; }
	virtual bool InternalIsLODDataLoaded(int32 LODIndex) { return true; }

	bool bUseRenderGraph = true;
	bool bIsInitialized = false;
	EHairStrandsAllocationType AllocationType = EHairStrandsAllocationType::Deferred;

	/* Store (precis) debug name */
	FHairResourceName ResourceName;
};

/* Render buffers for root deformation for dynamic meshes */
struct FHairStrandsRestRootResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairStrandsRestRootResource(FHairStrandsRootBulkData& BulkData, EHairStrandsResourcesType CurveType, const FHairResourceName& ResourceName);

	/* Init/Release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalAllocateLOD(FRDGBuilder& GraphBuilder, int32 LODIndex) override;
	virtual void InternalRelease() override;
	virtual bool InternalIsDataLoaded() override;
	virtual bool InternalIsLODDataLoaded(int32 LODIndex) override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairStrandsRestRootResource"); }

	/* Populate GPU LOD data from RootData (this function doesn't initialize resources) */
	void PopulateFromRootData();

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const 
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(VertexToCurveIndexBuffer);
		for (const FLOD& LOD : LODs)
		{
			Total += GetBufferTotalNumBytes(LOD.UniqueTriangleIndexBuffer);
			Total += GetBufferTotalNumBytes(LOD.RootToUniqueTriangleIndexBuffer);
			Total += GetBufferTotalNumBytes(LOD.RootBarycentricBuffer);
			Total += GetBufferTotalNumBytes(LOD.RestUniqueTrianglePosition0Buffer);
			Total += GetBufferTotalNumBytes(LOD.RestUniqueTrianglePosition1Buffer);
			Total += GetBufferTotalNumBytes(LOD.RestUniqueTrianglePosition2Buffer);
			Total += GetBufferTotalNumBytes(LOD.MeshInterpolationWeightsBuffer);
			Total += GetBufferTotalNumBytes(LOD.MeshSampleIndicesBuffer);
			Total += GetBufferTotalNumBytes(LOD.RestSamplePositionsBuffer);
		}
		return Total;
	}

	FRDGExternalBuffer VertexToCurveIndexBuffer;

	struct FLOD
	{
		enum class EStatus { Invalid, Initialized, Completed };

		const bool IsValid() const { return Status == EStatus::Completed; }
		EStatus Status = EStatus::Invalid;
		int32 LODIndex = -1;

		/* Triangle on which a root is attached */
		/* When the projection is done with source to target mesh transfer, the projection indices does not match.
		   In this case we need to separate index computation. The barycentric coords remain the same however. */
		FRDGExternalBuffer UniqueTriangleIndexBuffer;
		/* Strands hair root to unique triangle index */
		FRDGExternalBuffer RootToUniqueTriangleIndexBuffer;
		FRDGExternalBuffer RootBarycentricBuffer;

		/* Strand hair roots translation and rotation in rest position relative to the bound triangle. Positions are relative to the rest root center */
		FRDGExternalBuffer RestUniqueTrianglePosition0Buffer;
		FRDGExternalBuffer RestUniqueTrianglePosition1Buffer;
		FRDGExternalBuffer RestUniqueTrianglePosition2Buffer;

		/* Strand hair mesh interpolation matrix and sample indices */
		uint32 SampleCount = 0;
		FRDGExternalBuffer MeshInterpolationWeightsBuffer;
		FRDGExternalBuffer MeshSampleIndicesBuffer;
		FRDGExternalBuffer RestSamplePositionsBuffer;
	};

	/* Store the hair projection information for each mesh LOD */
	TArray<FLOD> LODs;

	/* LOD bulk data requests */
	TArray<FBulkDataBatchRequest> LODRequests;

	/* Store CPU data for root info & root binding */
	FHairStrandsRootBulkData& BulkData;

	/* Bulk data request handle */
	FBulkDataBatchRequest BulkDataRequest;

	/* Type of curves */
	const EHairStrandsResourcesType CurveType;
};

/* Render buffers for root deformation for dynamic meshes */
struct FHairStrandsDeformedRootResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairStrandsDeformedRootResource(EHairStrandsResourcesType CurveType, const FHairResourceName& ResourceName);
	FHairStrandsDeformedRootResource(const FHairStrandsRestRootResource* InRestResources, EHairStrandsResourcesType CurveType, const FHairResourceName& ResourceName);

	/* Init/Release buffers */
	virtual void InternalAllocateLOD(FRDGBuilder& GraphBuilder, int32 LODIndex) override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairStrandsDeformedRootResource"); }

	/* Indirect if the current root resources are valid and up to date */
	bool IsValid() const { return MeshLODIndex >= 0 && MeshLODIndex < LODs.Num() && LODs[MeshLODIndex].IsValid(); }
	bool IsValid(int32 InMeshLODIndex) const { return InMeshLODIndex >= 0 && InMeshLODIndex < LODs.Num() && LODs[InMeshLODIndex].IsValid(); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		for (const FLOD& LOD : LODs)
		{
			Total += GetBufferTotalNumBytes(LOD.DeformedUniqueTrianglePosition0Buffer[0]);
			Total += GetBufferTotalNumBytes(LOD.DeformedUniqueTrianglePosition1Buffer[0]);
			Total += GetBufferTotalNumBytes(LOD.DeformedUniqueTrianglePosition2Buffer[0]);
			Total += GetBufferTotalNumBytes(LOD.DeformedSamplePositionsBuffer[0]);
			Total += GetBufferTotalNumBytes(LOD.MeshSampleWeightsBuffer[0]);

			// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
			if (IsHairStrandContinuousDecimationReorderingEnabled())
			{
				Total += GetBufferTotalNumBytes(LOD.DeformedUniqueTrianglePosition0Buffer[1]);
				Total += GetBufferTotalNumBytes(LOD.DeformedUniqueTrianglePosition1Buffer[1]);
				Total += GetBufferTotalNumBytes(LOD.DeformedUniqueTrianglePosition2Buffer[1]);
				Total += GetBufferTotalNumBytes(LOD.DeformedSamplePositionsBuffer[1]);
				Total += GetBufferTotalNumBytes(LOD.MeshSampleWeightsBuffer[1]);
			}
		}
		return Total;
	}

	void SwapBuffer()
	{
		for (FLOD& LOD : LODs)
		{
			LOD.SwapBuffer();
		}
	}

	struct FLOD
	{
		enum class EStatus { Invalid, Initialized, Completed };

		// A LOD is considered valid as long as its resources are initialized. 
		// Its state will become completed once its triangle position will be 
		// update, but in order to be update its status needs to be valid.
		const bool IsValid() const { return Status == EStatus::Initialized || Status == EStatus::Completed; }
		EStatus Status = EStatus::Invalid;
		int32 LODIndex = -1;

		/* Strand hair roots translation and rotation in triangle-deformed position relative to the bound triangle. Positions are relative the deformed root center*/
		FRDGExternalBuffer DeformedUniqueTrianglePosition0Buffer[2];
		FRDGExternalBuffer DeformedUniqueTrianglePosition1Buffer[2];
		FRDGExternalBuffer DeformedUniqueTrianglePosition2Buffer[2];

		/* Strand hair mesh interpolation matrix and sample indices */
		uint32 SampleCount = 0;
		FRDGExternalBuffer DeformedSamplePositionsBuffer[2];
		FRDGExternalBuffer MeshSampleWeightsBuffer[2];

		/* Whether the GPU data should be initialized with the asset data or not */
		uint32 CurrentIndex = 0;

		enum EFrameType
		{
			Previous,
			Current
		};

		// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
		inline uint32 GetIndex(EFrameType T) const { return T == EFrameType::Current ? CurrentIndex : 1u - CurrentIndex; }
		inline const FRDGExternalBuffer& GetDeformedUniqueTrianglePosition0Buffer(EFrameType T) const { return IsHairStrandContinuousDecimationReorderingEnabled() ? DeformedUniqueTrianglePosition0Buffer[GetIndex(T)] : DeformedUniqueTrianglePosition0Buffer[0]; }
		inline const FRDGExternalBuffer& GetDeformedUniqueTrianglePosition1Buffer(EFrameType T) const { return IsHairStrandContinuousDecimationReorderingEnabled() ? DeformedUniqueTrianglePosition1Buffer[GetIndex(T)] : DeformedUniqueTrianglePosition1Buffer[0]; }
		inline const FRDGExternalBuffer& GetDeformedUniqueTrianglePosition2Buffer(EFrameType T) const { return IsHairStrandContinuousDecimationReorderingEnabled() ? DeformedUniqueTrianglePosition2Buffer[GetIndex(T)] : DeformedUniqueTrianglePosition2Buffer[0]; }
		inline const FRDGExternalBuffer& GetDeformedSamplePositionsBuffer(EFrameType T) const { return IsHairStrandContinuousDecimationReorderingEnabled() ? DeformedSamplePositionsBuffer[GetIndex(T)] : DeformedSamplePositionsBuffer[0]; }
		inline const FRDGExternalBuffer& GetMeshSampleWeightsBuffer(EFrameType T) const { return IsHairStrandContinuousDecimationReorderingEnabled() ? MeshSampleWeightsBuffer[GetIndex(T)] : MeshSampleWeightsBuffer[0]; }
		inline void SwapBuffer() { CurrentIndex = 1u - CurrentIndex; }
	};

	/* Store the hair projection information for each mesh LOD */
	uint32 RootCount = 0;
	TArray<FLOD> LODs;

	/* Last update MeshLODIndex */
	int32 MeshLODIndex = -1;

	/* Type of curves */
	const EHairStrandsResourcesType CurveType;
};

/* Render buffers that will be used for rendering */
struct FHairStrandsRestResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairStrandsRestResource(FHairStrandsBulkData& InBulkData, EHairStrandsResourcesType CurveType, const FHairResourceName& ResourceName);

	/* Init/Release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;
	virtual bool InternalIsDataLoaded() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairStrandsResource"); }

	FRDGExternalBuffer GetTangentBuffer(class FRDGBuilder& GraphBuilder, class FGlobalShaderMap* ShaderMap);
	
	FVector GetPositionOffset() const { return BulkData.GetPositionOffset(); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(PositionBuffer);
		Total += GetBufferTotalNumBytes(PositionOffsetBuffer);
		Total += GetBufferTotalNumBytes(TangentBuffer);
		Total += GetBufferTotalNumBytes(Attribute0Buffer);
		Total += GetBufferTotalNumBytes(Attribute1Buffer);
		Total += GetBufferTotalNumBytes(MaterialBuffer);
		return Total;
	}

	/* Strand hair rest position buffer */
	FRDGExternalBuffer  PositionBuffer;

	/* Strand hair rest offset position buffer */
	FRDGExternalBuffer  PositionOffsetBuffer;

	/* Strand hair tangent buffer (non-allocated unless used for static geometry) */
	FRDGExternalBuffer TangentBuffer;

	/* Strand hair attribute buffer */
	FRDGExternalBuffer Attribute0Buffer;

	/* Strand hair attribute buffer */
	FRDGExternalBuffer Attribute1Buffer;

	/* Strand hair material buffer */
	FRDGExternalBuffer MaterialBuffer;

	/* Reference to the hair strands render data */
	FHairStrandsBulkData& BulkData;

	/* Handle to bulk data request */
	FBulkDataBatchRequest BulkDataRequest;

	/* Type of curves */
	const EHairStrandsResourcesType CurveType;

	inline uint32 GetVertexCount() const { return BulkData.PointCount; }
};

struct FHairStrandsDeformedResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairStrandsDeformedResource(FHairStrandsBulkData& BulkData, EHairStrandsResourcesType CurveType, const FHairResourceName& ResourceName);

	/* Init/Release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairStrandsDeformedResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(DeformedPositionBuffer[0]);
		Total += GetBufferTotalNumBytes(DeformedPositionBuffer[1]);
		Total += GetBufferTotalNumBytes(TangentBuffer);
		return Total;
	}

	/* Strand hair deformed position buffer (previous and current) */
	FRDGExternalBuffer DeformedPositionBuffer[2];

	/* Strand hair deformed position buffer (previous and current) */
	FRDGExternalBuffer DeformedOffsetBuffer[2];

	/* Strand hair tangent buffer */
	FRDGExternalBuffer TangentBuffer;

	/* Position offset as the deformed positions are expressed in relative coordinate (16bits) */
	FVector PositionOffset[2] = {FVector::ZeroVector, FVector::ZeroVector};

	/* Reference to the hair strands render data */
	FHairStrandsBulkData& BulkData;

	/* Whether the GPU data should be initialized with the asset data or not */
	uint32 CurrentIndex = 0;

	/* Track the view which has update the formed position. This is used when rendering the same 
	   instance accross several editor viewport (not views of the same viewport), to prevent 
	   incorrect motion vector, as editor viewport are refresh not on the same tick, but with a 
	   throttling mechanism which make the updates not coherent. */
	uint32 UniqueViewIDs[2] = { 0, 0 };

	/* Type of curves */
	const EHairStrandsResourcesType CurveType;

	enum EFrameType
	{
		Previous,
		Current
	};

	// Helper accessors
	inline uint32 GetIndex(EFrameType T) const					{ return T == EFrameType::Current ? CurrentIndex : 1u - CurrentIndex; }
	inline FRDGExternalBuffer& GetBuffer(EFrameType T)			{ return DeformedPositionBuffer[GetIndex(T)];  }
	inline FVector& GetPositionOffset(EFrameType T)				{ return PositionOffset[GetIndex(T)]; }
	inline FRDGExternalBuffer& GetPositionOffsetBuffer(EFrameType T) { return DeformedOffsetBuffer[GetIndex(T)]; }
	inline const FVector& GetPositionOffset(EFrameType T) const { return PositionOffset[GetIndex(T)]; }
	inline void SetPositionOffset(EFrameType T, const FVector& Offset)  { PositionOffset[GetIndex(T)] = Offset; }
	inline void SwapBuffer()									{ CurrentIndex = 1u - CurrentIndex; }
	inline uint32& GetUniqueViewID(EFrameType T)				{ return UniqueViewIDs[GetIndex(T)]; }
	inline uint32 GetUniqueViewID(EFrameType T) const			{ return UniqueViewIDs[GetIndex(T)]; }
	//bool NeedsToUpdateTangent();
};

struct FHairStrandsClusterCullingResource : public FHairCommonResource
{
	FHairStrandsClusterCullingResource(FHairStrandsClusterCullingBulkData& Data, const FHairResourceName& ResourceName);

	/* Init/Release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;
	virtual bool InternalIsDataLoaded() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairStrandsClusterResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(ClusterInfoBuffer);
		Total += GetBufferTotalNumBytes(ClusterLODInfoBuffer);
		Total += GetBufferTotalNumBytes(VertexToClusterIdBuffer);
		Total += GetBufferTotalNumBytes(ClusterVertexIdBuffer);
		return Total;
	}

	/* Cluster info buffer */
	FRDGExternalBuffer ClusterInfoBuffer;
	FRDGExternalBuffer ClusterLODInfoBuffer;

	/* VertexId => ClusterId to know which AABB to contribute to*/
	FRDGExternalBuffer VertexToClusterIdBuffer;

	/* Concatenated data for each cluster: list of VertexId pointed to by ClusterInfoBuffer */
	FRDGExternalBuffer ClusterVertexIdBuffer;

	FHairStrandsClusterCullingBulkData& BulkData;

	/* Handle to bulk data request */
	FBulkDataBatchRequest BulkDataRequest;
};

struct FHairStrandsInterpolationResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairStrandsInterpolationResource(FHairStrandsInterpolationBulkData& InBulkData, const FHairResourceName& ResourceName);
	
	/* Init/Release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;
	virtual bool InternalIsDataLoaded() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairStrandsInterplationResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(InterpolationBuffer);
		Total += GetBufferTotalNumBytes(Interpolation0Buffer);
		Total += GetBufferTotalNumBytes(Interpolation1Buffer);
		Total += GetBufferTotalNumBytes(SimRootPointIndexBuffer);
		return Total;
	}

	bool UseSingleGuide() const { return InterpolationBuffer.Buffer != nullptr; }

	FRDGExternalBuffer InterpolationBuffer;
	FRDGExternalBuffer Interpolation0Buffer;
	FRDGExternalBuffer Interpolation1Buffer;
	FRDGExternalBuffer SimRootPointIndexBuffer;

	/* Reference to the hair strands interpolation render data */
	FHairStrandsInterpolationBulkData& BulkData;

	/* Handle to bulk data request */
	FBulkDataBatchRequest BulkDataRequest;
};

#if RHI_RAYTRACING
struct FHairStrandsRaytracingResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairStrandsRaytracingResource(const FHairStrandsBulkData& InData, const FHairResourceName& ResourceName);
	FHairStrandsRaytracingResource(const FHairCardsBulkData& InData, const FHairResourceName& ResourceName);
	FHairStrandsRaytracingResource(const FHairMeshesBulkData& InData, const FHairResourceName& ResourceName);

	/* Init/Release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairStrandsRaytracingResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(PositionBuffer);
		Total += GetBufferTotalNumBytes(IndexBuffer);
		return Total;
	}

	FRDGExternalBuffer PositionBuffer;
	FRDGExternalBuffer IndexBuffer;
	FRayTracingGeometry RayTracingGeometry;
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	bool bOwnBuffers = false;
	bool bIsRTGeometryInitialized = false;
	bool bProceduralPrimitive = false;
};
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cards

class FHairCardsVertexBuffer : public FVertexBufferWithSRV
{
public:
	virtual void InitRHI() override {}
};

class FHairCardIndexBuffer : public FIndexBuffer
{
public:
	const TArray<FHairCardsIndexFormat::Type>& Indices;
	FHairCardIndexBuffer(const TArray<FHairCardsIndexFormat::Type>& InIndices) :Indices(InIndices) {}
	virtual void InitRHI() override;
};

struct FHairCardsBulkData;

/* Render buffers that will be used for rendering */
struct FHairCardsRestResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairCardsRestResource(const FHairCardsBulkData& InBulkData, const FHairResourceName& ResourceName);

	virtual void InitResource() override;
	virtual void ReleaseResource() override;

	/* Init/release buffers */
	virtual void InternalAllocate() override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairCardsResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += BulkData.Positions.GetAllocatedSize();
		Total += BulkData.Normals.GetAllocatedSize();
		Total += BulkData.Indices.GetAllocatedSize();
		Total += BulkData.UVs.GetAllocatedSize();
		Total += BulkData.Materials.GetAllocatedSize();
		return Total;
	}

	uint32 GetVertexCount() const { return BulkData.GetNumVertices();  }
	uint32 GetPrimitiveCount() const { return BulkData.GetNumTriangles(); }

	/* Strand hair rest position buffer */
	FHairCardsVertexBuffer RestPositionBuffer;
	FHairCardIndexBuffer RestIndexBuffer;
	bool bInvertUV = false;

	FHairCardsVertexBuffer NormalsBuffer;
	FHairCardsVertexBuffer UVsBuffer;
	FHairCardsVertexBuffer MaterialsBuffer;

	FSamplerStateRHIRef DepthSampler = nullptr;
	FSamplerStateRHIRef TangentSampler = nullptr;
	FSamplerStateRHIRef CoverageSampler = nullptr;
	FSamplerStateRHIRef AttributeSampler = nullptr;
	FSamplerStateRHIRef AuxilaryDataSampler = nullptr;
	FSamplerStateRHIRef MaterialSampler = nullptr;

	FTextureReferenceRHIRef	DepthTexture = nullptr;
	FTextureReferenceRHIRef	CoverageTexture = nullptr;
	FTextureReferenceRHIRef	TangentTexture = nullptr;
	FTextureReferenceRHIRef	AttributeTexture = nullptr;
	FTextureReferenceRHIRef	AuxilaryDataTexture = nullptr;
	FTextureReferenceRHIRef	MaterialTexture = nullptr;

	/* Reference to the hair strands render data */
	const FHairCardsBulkData& BulkData;
};

/* Render buffers that will be used for rendering */
struct FHairCardsProceduralResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairCardsProceduralResource(const FHairCardsProceduralDatas::FRenderData& HairCardsRenderData, const FIntPoint& AtlasResolution, const FHairCardsVoxel& InVoxel);

	/* Init/release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairCardsResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		return 0;
	}

	/* Strand hair rest position buffer */		
	uint32 CardBoundCount;
	FIntPoint AtlasResolution;

	FRDGExternalBuffer AtlasRectBuffer;
	FRDGExternalBuffer LengthBuffer;
	FRDGExternalBuffer CardItToClusterBuffer;
	FRDGExternalBuffer ClusterIdToVerticesBuffer;
	FRDGExternalBuffer ClusterBoundBuffer;
	FRDGExternalBuffer CardsStrandsPositions;
	FRDGExternalBuffer CardsStrandsAttributes;

	FHairCardsVoxel CardVoxel;

	/* Position offset as the rest positions are expressed in relative coordinate (16bits) */
	//FVector PositionOffset = FVector::ZeroVector;

	/* Reference to the hair strands render data */
	const FHairCardsProceduralDatas::FRenderData& RenderData;
};

struct FHairCardsDeformedResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairCardsDeformedResource(const FHairCardsBulkData& BulkData, bool bInitializeData, const FHairResourceName& ResourceName);

	/* Init/release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairCardsDeformedResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(DeformedPositionBuffer[0]);
		Total += GetBufferTotalNumBytes(DeformedPositionBuffer[1]);
		return Total;
	}

	/* Strand hair deformed position buffer (previous and current) */
	FRDGExternalBuffer DeformedPositionBuffer[2];
	FRDGExternalBuffer DeformedNormalBuffer;

	/* Reference to the hair strands render data */
	const FHairCardsBulkData& BulkData;

	/* Whether the GPU data should be initialized with the asset data or not */
	const bool bInitializedData = false;

	enum EFrameType
	{
		Previous,
		Current
	};

	// Helper accessors
	inline uint32 GetIndex(EFrameType T)				{ return T == EFrameType::Current ? 0u : 1u; }
	inline FRDGExternalBuffer& GetBuffer(EFrameType T)	{ return DeformedPositionBuffer[GetIndex(T)];  }
};

struct FHairCardsInterpolationBulkData;

/** Hair cards points interpolation attributes */
struct HAIRSTRANDSCORE_API FHairCardsInterpolationDatas
{
	/** Set the number of interpolated points */
	void SetNum(const uint32 NumPoints);

	/** Reset the interpolated points to 0 */
	void Reset();

	/** Get the number of interpolated points */
	uint32 Num() const { return PointsSimCurvesVertexIndex.Num(); }

	/** Simulation curve indices */
	TArray<int32> PointsSimCurvesIndex;

	/** Closest vertex indices on simulation curve */
	TArray<int32> PointsSimCurvesVertexIndex;

	/** Lerp value between the closest vertex indices and the next one */
	TArray<float> PointsSimCurvesVertexLerp;
};

struct HAIRSTRANDSCORE_API FHairCardsInterpolationBulkData
{
	TArray<FHairCardsInterpolationFormat::Type> Interpolation;

	void Serialize(FArchive& Ar);
};

struct FHairCardsInterpolationResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairCardsInterpolationResource(FHairCardsInterpolationBulkData& InBulkData, const FHairResourceName& ResourceName);

	/* Init/release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairCardsInterplationResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(InterpolationBuffer);
		return Total;
	}

	FRDGExternalBuffer InterpolationBuffer;

	/* Reference to the hair strands interpolation render data */
	FHairCardsInterpolationBulkData& BulkData;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Meshes

/* Render buffers that will be used for rendering */
struct FHairMeshesRestResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairMeshesRestResource(const FHairMeshesBulkData& BulkData, const FHairResourceName& ResourceName);

	virtual void InitResource() override;
	virtual void ReleaseResource() override;

	/* Init/release buffers */
	virtual void InternalAllocate() override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairMeshesRestResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += BulkData.Positions.GetAllocatedSize();
		Total += BulkData.Normals.GetAllocatedSize();
		Total += BulkData.Indices.GetAllocatedSize();
		Total += BulkData.UVs.GetAllocatedSize();
		return Total;
	}

	uint32 GetVertexCount() const { return BulkData.GetNumVertices(); }
	uint32 GetPrimitiveCount() const { return BulkData.GetNumTriangles(); }

	/* Strand hair rest position buffer */
	FHairCardsVertexBuffer RestPositionBuffer;
	FHairCardIndexBuffer IndexBuffer;	

	FHairCardsVertexBuffer NormalsBuffer;
	FHairCardsVertexBuffer UVsBuffer;

	FSamplerStateRHIRef DepthSampler = nullptr;
	FSamplerStateRHIRef TangentSampler = nullptr;
	FSamplerStateRHIRef CoverageSampler = nullptr;
	FSamplerStateRHIRef AttributeSampler = nullptr;
	FSamplerStateRHIRef AuxilaryDataSampler = nullptr;
	FSamplerStateRHIRef MaterialSampler = nullptr;

	FTextureReferenceRHIRef	DepthTexture = nullptr;
	FTextureReferenceRHIRef	CoverageTexture = nullptr;
	FTextureReferenceRHIRef	TangentTexture = nullptr;
	FTextureReferenceRHIRef	AttributeTexture = nullptr;
	FTextureReferenceRHIRef	AuxilaryDataTexture = nullptr;
	FTextureReferenceRHIRef	MaterialTexture = nullptr;

	/* Reference to the hair strands render data */
	const FHairMeshesBulkData& BulkData;
};


/* Render buffers that will be used for rendering */
struct FHairMeshesDeformedResource : public FHairCommonResource
{
	/** Build the hair strands resource */
	FHairMeshesDeformedResource(const FHairMeshesBulkData& InBulkData, bool bInInitializedData, const FHairResourceName& ResourceName);

	/* Init/release buffers */
	virtual void InternalAllocate(FRDGBuilder& GraphBuilder) override;
	virtual void InternalRelease() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FHairMeshesDeformedResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(DeformedPositionBuffer[0]);
		Total += GetBufferTotalNumBytes(DeformedPositionBuffer[1]);
		return Total;
	}

	/* Strand hair deformed position buffer (previous and current) */
	FRDGExternalBuffer DeformedPositionBuffer[2];

	/* Reference to the hair strands render data */
	const FHairMeshesBulkData& BulkData;

	/* Whether the GPU data should be initialized with the asset data or not */
	const bool bInitializedData = false;

	/* Whether the GPU data should be initialized with the asset data or not */
	uint32 CurrentIndex = 0;

	enum EFrameType
	{
		Previous,
		Current
	};

	// Helper accessors
	inline uint32 GetIndex(EFrameType T) { return T == EFrameType::Current ? CurrentIndex : 1u - CurrentIndex; }
	inline FRDGExternalBuffer& GetBuffer(EFrameType T) { return DeformedPositionBuffer[GetIndex(T)]; }
	inline void SwapBuffer() { CurrentIndex = 1u - CurrentIndex; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug data (used for debug visalization but also for texture generation)
void CreateHairStrandsDebugDatas(const FHairStrandsDatas& InData, FHairStrandsDebugDatas& Out);
void CreateHairStrandsDebugResources(class FRDGBuilder& GraphBuilder, const FHairStrandsDebugDatas* In, FHairStrandsDebugDatas::FResources* Out);
